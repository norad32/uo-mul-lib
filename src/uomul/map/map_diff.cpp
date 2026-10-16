#include "map_helper.h"
#include "uomul/map.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <expected>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ios>
#include <istream>
#include <limits>
#include <optional>
#include <ostream>
#include <string>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

namespace uomul
{

using detail::MapHelper;

namespace
{

constexpr std::uint32_t mapBlockIdByteCount = 4;

std::uint64_t countChangedTiles(const LandBlockBytes & left, const LandBlockBytes & right)
{
    std::uint64_t changedTiles = 0;
    for (std::size_t tile = 0; tile < landBlockTileCount; ++tile)
    {
        const auto offset = static_cast<std::size_t>(MapHelper::landHeaderByteSize) + (tile * 3U);
        changedTiles += left.at(offset) != right.at(offset) || left.at(offset + 1) != right.at(offset + 1) ||
                                left.at(offset + 2) != right.at(offset + 2)
                            ? 1U
                            : 0U;
    }
    return changedTiles;
}

struct LandBlockComparison
{
    LandBlockBytes targetRaw{};
    std::uint64_t changedTiles = 0;
};

std::expected<LandBlockComparison, Error> compareLandBlocks(int targetDescriptor,
                                                            const std::filesystem::path & targetPath,
                                                            Dimensions dimensions,
                                                            BlockCoord coordinate,
                                                            const LandBlockBytes & sourceRaw,
                                                            std::optional<TileRegion> tileRegion = std::nullopt)
{
    LandBlockComparison comparison{};
    const auto offset = MapHelper::blockOffset(coordinate, dimensions);
    if (!MapHelper::readExact(targetDescriptor, comparison.targetRaw.data(), comparison.targetRaw.size(), offset))
    {
        return std::unexpected(
            MapHelper::makeError(ErrorCode::truncatedInput, targetPath, offset, "could not read target block"));
    }
    if (!tileRegion)
    {
        comparison.changedTiles = countChangedTiles(sourceRaw, comparison.targetRaw);
        return comparison;
    }

    std::ranges::copy_n(sourceRaw.begin(),
                        static_cast<std::size_t>(MapHelper::landHeaderByteSize),
                        comparison.targetRaw.begin());
    for (std::size_t index = 0; index < landBlockTileCount; ++index)
    {
        const auto localX = static_cast<std::uint32_t>(index % landBlockDimension);
        const auto localY = static_cast<std::uint32_t>(index / landBlockDimension);
        const TileCoord tile{
            .x = (coordinate.x * landBlockDimension) + localX,
            .y = (coordinate.y * landBlockDimension) + localY,
        };
        if (tile.x >= tileRegion->min.x && tile.x < tileRegion->maxExclusive.x && tile.y >= tileRegion->min.y &&
            tile.y < tileRegion->maxExclusive.y)
        {
            const auto tileOffset = static_cast<std::size_t>(MapHelper::landHeaderByteSize) + (index * 3U);
            comparison.changedTiles += sourceRaw.at(tileOffset) != comparison.targetRaw.at(tileOffset) ||
                                               sourceRaw.at(tileOffset + 1) != comparison.targetRaw.at(tileOffset + 1) ||
                                               sourceRaw.at(tileOffset + 2) != comparison.targetRaw.at(tileOffset + 2)
                                           ? 1U
                                           : 0U;
        }
        else
        {
            const auto tileOffset = static_cast<std::size_t>(MapHelper::landHeaderByteSize) + (index * 3U);
            comparison.targetRaw.at(tileOffset) = sourceRaw.at(tileOffset);
            comparison.targetRaw.at(tileOffset + 1) = sourceRaw.at(tileOffset + 1);
            comparison.targetRaw.at(tileOffset + 2) = sourceRaw.at(tileOffset + 2);
        }
    }
    return comparison;
}

bool writeLe32(std::ostream & stream, std::uint32_t value)
{
    const auto encoded = detail::encodeLittleEndian<std::uint32_t, mapBlockIdByteCount>(value);
    std::array<char, mapBlockIdByteCount> bytes{};
    std::ranges::transform(
        encoded, bytes.begin(), [](std::byte byte) { return static_cast<char>(std::to_integer<unsigned char>(byte)); });
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return stream.good();
}

constexpr std::uint32_t feluccaWidth = 6144;
constexpr std::uint32_t feluccaHeight = 4096;
constexpr std::uint32_t ilshenarWidth = 2304;
constexpr std::uint32_t ilshenarHeight = 1600;
constexpr std::uint32_t malasWidth = 2560;
constexpr std::uint32_t malasHeight = 2048;
constexpr std::uint32_t tokunoSize = 1448;
constexpr std::uint32_t termurWidth = 1280;

struct DiffOutputPaths
{
    std::filesystem::path data;
    std::filesystem::path index;
};

struct DiffTemporaryFiles
{
    DiffOutputPaths outputs;
    std::filesystem::path data;
    std::filesystem::path index;
    int dataDescriptor = -1;
    int indexDescriptor = -1;
};

struct DiffBackups
{
    std::filesystem::path data;
    std::filesystem::path index;
};

struct DiffGenerationState
{
    std::filesystem::path targetPath;
    std::filesystem::path dataTemporary;
    std::filesystem::path indexTemporary;
    Dimensions dimensions{};
    int targetDescriptor = -1;
    std::ofstream * data = nullptr;
    std::ofstream * index = nullptr;
    DiffReport * report = nullptr;
    std::optional<TileRegion> tileRegion;
};

void closeDiffDescriptors(DiffTemporaryFiles & files)
{
    if (files.dataDescriptor >= 0)
    {
        static_cast<void>(::close(files.dataDescriptor));
        files.dataDescriptor = -1;
    }
    if (files.indexDescriptor >= 0)
    {
        static_cast<void>(::close(files.indexDescriptor));
        files.indexDescriptor = -1;
    }
}

void removeDiffTemporaries(DiffTemporaryFiles & files)
{
    closeDiffDescriptors(files);
    if (!files.data.empty())
    {
        static_cast<void>(::unlink(files.data.c_str()));
    }
    if (!files.index.empty())
    {
        static_cast<void>(::unlink(files.index.c_str()));
    }
}

class DiffTemporaryCleanup
{
public:
    explicit DiffTemporaryCleanup(DiffTemporaryFiles & files) : files_(files) {}

    DiffTemporaryCleanup(const DiffTemporaryCleanup &) = delete;
    DiffTemporaryCleanup & operator=(const DiffTemporaryCleanup &) = delete;
    DiffTemporaryCleanup(DiffTemporaryCleanup &&) = delete;
    DiffTemporaryCleanup & operator=(DiffTemporaryCleanup &&) = delete;

    ~DiffTemporaryCleanup()
    {
        if (active_)
        {
            removeDiffTemporaries(files_);
        }
    }

    void release() noexcept { active_ = false; }

private:
    DiffTemporaryFiles & files_;
    bool active_ = true;
};

std::expected<DiffTemporaryFiles, Error>
prepareDiffTemporaries(const std::filesystem::path & directory, std::uint32_t mapIndex, bool overwrite)
{
    std::error_code filesystemError;
    if (!std::filesystem::is_directory(directory, filesystemError) || filesystemError)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, directory, 0, "diff output directory does not exist"));
    }

    DiffTemporaryFiles files{};
    files.outputs.data = directory / ("mapdif" + std::to_string(mapIndex) + ".mul");
    files.outputs.index = directory / ("mapdifl" + std::to_string(mapIndex) + ".mul");
    if (auto result = MapHelper::checkOutputPath(files.outputs.data, overwrite); !result)
    {
        return std::unexpected(result.error());
    }
    if (auto result = MapHelper::checkOutputPath(files.outputs.index, overwrite); !result)
    {
        return std::unexpected(result.error());
    }

    auto dataDescriptor = MapHelper::createTemporary(files.outputs.data, files.data);
    if (!dataDescriptor)
    {
        return std::unexpected(dataDescriptor.error());
    }
    files.dataDescriptor = *dataDescriptor;
    auto indexDescriptor = MapHelper::createTemporary(files.outputs.index, files.index);
    if (!indexDescriptor)
    {
        removeDiffTemporaries(files);
        return std::unexpected(indexDescriptor.error());
    }
    files.indexDescriptor = *indexDescriptor;
    return files;
}

bool readLe32(std::istream & stream, std::uint32_t & value)
{
    std::array<char, mapBlockIdByteCount> bytes{};
    stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!stream)
    {
        return false;
    }

    std::array<std::byte, mapBlockIdByteCount> encoded{};
    std::ranges::transform(
        bytes, encoded.begin(), [](char byte) { return static_cast<std::byte>(static_cast<unsigned char>(byte)); });
    value = detail::decodeLittleEndian<std::uint32_t, mapBlockIdByteCount>(encoded);
    return true;
}

std::expected<void, Error>
appendChangedDiffBlock(const DiffGenerationState & state, BlockCoord coordinate, const LandBlockBytes & sourceRaw)
{
    auto comparison =
        compareLandBlocks(state.targetDescriptor, state.targetPath, state.dimensions, coordinate, sourceRaw, state.tileRegion);
    if (!comparison)
    {
        return std::unexpected(comparison.error());
    }
    const auto changedTiles = comparison->changedTiles;
    if (changedTiles == 0)
    {
        return {};
    }

    const auto id = MapHelper::blockId(coordinate, state.dimensions);
    if (id > std::numeric_limits<std::uint32_t>::max() || !writeLe32(*state.index, static_cast<std::uint32_t>(id)))
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, state.indexTemporary, 0, "could not write diff index"));
    }
    std::array<char, MapHelper::landBlockByteSize> bytes{};
    std::ranges::transform(comparison->targetRaw,
                           bytes.begin(),
                           [](std::byte byte) { return static_cast<char>(std::to_integer<unsigned char>(byte)); });
    state.data->write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!*state.data)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, state.dataTemporary, 0, "could not write diff block data"));
    }

    ++state.report->landBlocksChanged;
    state.report->landTilesChanged += changedTiles;
    return {};
}

std::expected<DiffReport, Error> scanDiffTemporaries(const std::filesystem::path & baseMap,
                                                     const std::filesystem::path & targetMap,
                                                     Dimensions dimensions,
                                                     DiffTemporaryFiles & files,
                                                     BlockRegion region,
                                                     std::optional<TileRegion> tileRegion = std::nullopt)
{
    DiffTemporaryCleanup cleanup(files);
    std::ofstream data(files.data, std::ios::binary | std::ios::trunc);
    std::ofstream index(files.index, std::ios::binary | std::ios::trunc);
    if (!data || !index)
    {
        return std::unexpected(
            MapHelper::makeError(ErrorCode::io, files.outputs.data.parent_path(), 0, "could not create diff files"));
    }

    auto source = MapReader::open(baseMap, dimensions);
    auto target = MapReader::open(targetMap, dimensions);
    if (!source || !target)
    {
        return std::unexpected(!source ? source.error() : target.error());
    }

    struct stat targetStatus{};
    const detail::ScopedDescriptor targetDescriptor(::open(targetMap.c_str(), O_RDONLY | O_CLOEXEC));
    if (targetDescriptor.get() < 0 || ::fstat(targetDescriptor.get(), &targetStatus) != 0 || !S_ISREG(targetStatus.st_mode))
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, targetMap, 0, std::strerror(errno)));
    }

    DiffReport report{};
    DiffGenerationState state{
        .targetPath = targetMap,
        .dataTemporary = files.data,
        .indexTemporary = files.index,
        .dimensions = dimensions,
        .targetDescriptor = targetDescriptor.get(),
        .data = &data,
        .index = &index,
        .report = &report,
        .tileRegion = tileRegion,
    };
    auto compared = source->readRawLandBlocks(region,
                                              [&state](BlockCoord coordinate, const LandBlockBytes & sourceRaw)
                                              { return appendChangedDiffBlock(state, coordinate, sourceRaw); });
    if (!compared)
    {
        return std::unexpected(compared.error());
    }

    data.close();
    index.close();
    if (!data || !index)
    {
        return std::unexpected(
            MapHelper::makeError(ErrorCode::io, files.outputs.data.parent_path(), 0, "could not write diff files"));
    }
    closeDiffDescriptors(files);
    cleanup.release();
    return report;
}

void restoreDiffBackups(const DiffBackups & backups, const DiffOutputPaths & outputs)
{
    if (!backups.data.empty())
    {
        static_cast<void>(::rename(backups.data.c_str(), outputs.data.c_str()));
    }
    if (!backups.index.empty())
    {
        static_cast<void>(::rename(backups.index.c_str(), outputs.index.c_str()));
    }
}

std::expected<DiffBackups, Error> backupDiffOutputs(const DiffOutputPaths & outputs, DiffTemporaryFiles & files)
{
    DiffBackups backups{};
    if (std::filesystem::exists(outputs.data))
    {
        backups.data = MapHelper::temporaryPath(outputs.data);
        if (::rename(outputs.data.c_str(), backups.data.c_str()) != 0)
        {
            const auto error = MapHelper::makeError(ErrorCode::commitFailed, outputs.data, 0, std::strerror(errno));
            removeDiffTemporaries(files);
            return std::unexpected(error);
        }
    }
    if (std::filesystem::exists(outputs.index))
    {
        backups.index = MapHelper::temporaryPath(outputs.index);
        if (::rename(outputs.index.c_str(), backups.index.c_str()) != 0)
        {
            const auto error = MapHelper::makeError(ErrorCode::commitFailed, outputs.index, 0, std::strerror(errno));
            restoreDiffBackups(backups, outputs);
            removeDiffTemporaries(files);
            return std::unexpected(error);
        }
    }

    return backups;
}

void removeDiffBackups(const DiffBackups & backups)
{
    if (!backups.data.empty())
    {
        static_cast<void>(::unlink(backups.data.c_str()));
    }
    if (!backups.index.empty())
    {
        static_cast<void>(::unlink(backups.index.c_str()));
    }
}

class DiffBackupCleanup
{
public:
    DiffBackupCleanup(const DiffBackups & backups, const DiffOutputPaths & outputs) : backups_(backups), outputs_(outputs) {}

    DiffBackupCleanup(const DiffBackupCleanup &) = delete;
    DiffBackupCleanup & operator=(const DiffBackupCleanup &) = delete;
    DiffBackupCleanup(DiffBackupCleanup &&) = delete;
    DiffBackupCleanup & operator=(DiffBackupCleanup &&) = delete;

    ~DiffBackupCleanup()
    {
        if (active_)
        {
            restoreDiffBackups(backups_, outputs_);
        }
    }

    void release() noexcept { active_ = false; }

private:
    const DiffBackups & backups_;
    const DiffOutputPaths & outputs_;
    bool active_ = true;
};

std::expected<void, Error> installDiffPair(const DiffOutputPaths & outputs, const DiffTemporaryFiles & files)
{
    if (::link(files.data.c_str(), outputs.data.c_str()) != 0)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::commitFailed, outputs.data, 0, std::strerror(errno)));
    }
    static_cast<void>(::unlink(files.data.c_str()));
    if (::link(files.index.c_str(), outputs.index.c_str()) != 0)
    {
        const int savedErrno = errno;
        static_cast<void>(::unlink(outputs.data.c_str()));
        return std::unexpected(MapHelper::makeError(ErrorCode::commitFailed, outputs.index, 0, std::strerror(savedErrno)));
    }
    static_cast<void>(::unlink(files.index.c_str()));
    return {};
}

std::expected<DiffReport, Error>
publishDiffWithoutOverwrite(const DiffOutputPaths & outputs, DiffTemporaryFiles & files, DiffReport report)
{
    auto installed = installDiffPair(outputs, files);
    if (!installed)
    {
        removeDiffTemporaries(files);
        return std::unexpected(installed.error());
    }
    return report;
}

std::expected<DiffReport, Error>
publishDiffWithOverwrite(const DiffOutputPaths & outputs, DiffTemporaryFiles & files, DiffReport report)
{
    auto backups = backupDiffOutputs(outputs, files);
    if (!backups)
    {
        return std::unexpected(backups.error());
    }
    DiffBackupCleanup rollback(*backups, outputs);
    auto installed = installDiffPair(outputs, files);
    if (!installed)
    {
        removeDiffTemporaries(files);
        return std::unexpected(installed.error());
    }
    rollback.release();
    removeDiffBackups(*backups);
    return report;
}

std::expected<DiffReport, Error>
publishDiffFiles(const DiffOutputPaths & outputs, DiffTemporaryFiles & files, bool overwrite, DiffReport report)
{
    if (overwrite)
    {
        return publishDiffWithOverwrite(outputs, files, report);
    }

    return publishDiffWithoutOverwrite(outputs, files, report);
}

struct DiffLengths
{
    std::uint64_t blockCount = 0;
};

std::expected<DiffLengths, Error> validateDiffLengths(std::istream & data,
                                                      std::istream & index,
                                                      const std::filesystem::path & dataPath,
                                                      const std::filesystem::path & diffDirectory)
{
    data.seekg(0, std::ios::end);
    index.seekg(0, std::ios::end);
    const auto dataLength = data.tellg();
    const auto indexLength = index.tellg();
    if (dataLength < 0 || indexLength < 0)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, diffDirectory, 0, "could not determine diff file sizes"));
    }

    const auto dataBytes = static_cast<std::uint64_t>(dataLength);
    const auto indexBytes = static_cast<std::uint64_t>(indexLength);
    if (indexBytes % mapBlockIdByteCount != 0 || dataBytes % MapHelper::landBlockByteSize != 0 ||
        dataBytes / MapHelper::landBlockByteSize != indexBytes / mapBlockIdByteCount)
    {
        return std::unexpected(
            MapHelper::makeError(ErrorCode::truncatedInput, dataPath, 0, "diff data and index lengths are inconsistent"));
    }
    data.seekg(0);
    index.seekg(0);
    return DiffLengths{.blockCount = indexBytes / mapBlockIdByteCount};
}

std::expected<std::pair<BlockCoord, LandBlock>, Error>
readDiffReplacement(std::istream & data, const std::filesystem::path & dataPath, Dimensions dimensions, std::uint32_t id)
{
    LandBlockBytes raw{};
    for (auto & byte : raw)
    {
        const auto value = data.get();
        if (value == std::char_traits<char>::eof())
        {
            break;
        }
        byte = static_cast<std::byte>(static_cast<unsigned char>(value));
    }
    if (!data)
    {
        return std::unexpected(MapHelper::makeError(
            ErrorCode::truncatedInput, dataPath, static_cast<std::uint64_t>(data.tellg()), "diff block is truncated"));
    }

    const auto blockHeight = dimensions.heightTiles / landBlockDimension;
    const BlockCoord coordinate{.x = id / blockHeight, .y = id % blockHeight};
    return std::pair{coordinate, MapHelper::decodeBlock(raw)};
}

std::expected<std::vector<std::pair<BlockCoord, LandBlock>>, Error>
readDiffReplacements(std::istream & data,
                     std::istream & index,
                     const std::filesystem::path & dataPath,
                     const std::filesystem::path & indexPath,
                     Dimensions dimensions,
                     std::uint64_t expectedBlockCount,
                     std::uint64_t replacementCount)
{
    std::vector<bool> seen(static_cast<std::size_t>(expectedBlockCount));
    std::vector<std::pair<BlockCoord, LandBlock>> replacements;
    replacements.reserve(static_cast<std::size_t>(replacementCount));
    std::uint32_t id = 0;
    while (readLe32(index, id))
    {
        if (static_cast<std::uint64_t>(id) >= expectedBlockCount || seen.at(id))
        {
            return std::unexpected(MapHelper::makeError(ErrorCode::invalidBlockCount,
                                                        indexPath,
                                                        static_cast<std::uint64_t>(index.tellg()) - mapBlockIdByteCount,
                                                        "diff contains an out-of-range or duplicate block ID"));
        }
        seen.at(id) = true;
        auto replacement = readDiffReplacement(data, dataPath, dimensions, id);
        if (!replacement)
        {
            return std::unexpected(replacement.error());
        }
        replacements.push_back(std::move(*replacement));
    }
    if (!index.eof())
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, indexPath, 0, "failed while reading diff index"));
    }

    return replacements;
}

std::expected<void, Error> writeDiffReplacements(MapEditor & editor,
                                                 const std::vector<std::pair<BlockCoord, LandBlock>> & replacements)
{
    for (const auto & [coordinate, block] : replacements)
    {
        auto result = editor.writeLandBlock(coordinate, block);
        if (!result)
        {
            return result;
        }
    }

    return {};
}

std::expected<DiffReport, Error> compareMaps(const std::filesystem::path & sourcePath,
                                             const std::filesystem::path & targetPath,
                                             Dimensions dimensions,
                                             const std::function<void(std::uint64_t, BlockCoord, std::uint64_t)> & onChange)
{
    auto source = MapReader::open(sourcePath, dimensions);
    if (!source)
    {
        return std::unexpected(source.error());
    }
    auto target = MapReader::open(targetPath, dimensions);
    if (!target)
    {
        return std::unexpected(target.error());
    }
    const int targetDescriptor = ::open(targetPath.c_str(), O_RDONLY | O_CLOEXEC);
    if (targetDescriptor < 0)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, targetPath, 0, std::strerror(errno)));
    }

    DiffReport report{};
    const BlockRegion fullRegion{
        .min = {.x = 0, .y = 0},
        .maxExclusive = {.x = dimensions.widthTiles / landBlockDimension, .y = dimensions.heightTiles / landBlockDimension},
    };
    auto scan =
        source->readRawLandBlocks(fullRegion,
                                  [&](BlockCoord coordinate, const LandBlockBytes & sourceRaw) -> std::expected<void, Error>
                                  {
                                      auto comparison =
                                          compareLandBlocks(targetDescriptor, targetPath, dimensions, coordinate, sourceRaw);
                                      if (!comparison)
                                      {
                                          return std::unexpected(comparison.error());
                                      }
                                      const auto changedTiles = comparison->changedTiles;

                                      if (changedTiles != 0)
                                      {
                                          ++report.landBlocksChanged;
                                          report.landTilesChanged += changedTiles;
                                          if (onChange)
                                          {
                                              onChange(MapHelper::blockId(coordinate, dimensions), coordinate, changedTiles);
                                          }
                                      }

                                      return {};
                                  });
    static_cast<void>(::close(targetDescriptor));
    if (!scan)
    {
        return std::unexpected(scan.error());
    }

    return report;
}

std::expected<DiffReport, Error> writeMapCsv(const std::filesystem::path & sourceMap,
                                             const std::filesystem::path & targetMap,
                                             const std::filesystem::path & reportFile,
                                             Dimensions dimensions,
                                             bool overwrite)
{
    if (auto result = MapHelper::checkOutputPath(reportFile, overwrite); !result)
    {
        return std::unexpected(result.error());
    }
    const auto parent = reportFile.parent_path();
    if (!parent.empty() && !std::filesystem::is_directory(parent))
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, parent, 0, "report directory does not exist"));
    }

    std::filesystem::path temporary;
    auto descriptor = MapHelper::createTemporary(reportFile, temporary);
    if (!descriptor)
    {
        return std::unexpected(descriptor.error());
    }
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        static_cast<void>(::close(*descriptor));
        static_cast<void>(::unlink(temporary.c_str()));
        return std::unexpected(MapHelper::makeError(ErrorCode::io, temporary, 0, "could not create report"));
    }

    output << "blockId,blockX,blockY,changedLandTiles\n";
    auto compared = compareMaps(sourceMap,
                                targetMap,
                                dimensions,
                                [&output](std::uint64_t id, BlockCoord coordinate, std::uint64_t changed)
                                { output << id << ',' << coordinate.x << ',' << coordinate.y << ',' << changed << '\n'; });
    output.flush();
    output.close();
    static_cast<void>(::close(*descriptor));
    if (!compared)
    {
        static_cast<void>(::unlink(temporary.c_str()));
        return std::unexpected(compared.error());
    }
    if (!output)
    {
        static_cast<void>(::unlink(temporary.c_str()));
        return std::unexpected(MapHelper::makeError(ErrorCode::io, temporary, 0, "could not write report"));
    }
    auto published = MapHelper::publishFile(temporary, reportFile, overwrite);
    if (!published)
    {
        return std::unexpected(published.error());
    }

    return *compared;
}

} // namespace

std::expected<DiffReport, Error> MapDiff::generate(const std::filesystem::path & baseMap,
                                                   const std::filesystem::path & targetMap,
                                                   const std::filesystem::path & diffDirectory,
                                                   std::uint32_t mapIndex,
                                                   Dimensions dimensions,
                                                   bool overwrite)
{
    const BlockRegion fullRegion{
        .min =
            {
                .x = 0,
                .y = 0,
            },
        .maxExclusive =
            {
                .x = dimensions.widthTiles / landBlockDimension,
                .y = dimensions.heightTiles / landBlockDimension,
            },
    };

    return generateFromBlockRegion(baseMap, targetMap, diffDirectory, mapIndex, dimensions, fullRegion, overwrite);
}

std::expected<DiffReport, Error> MapDiff::generateFromTileRegion(const std::filesystem::path & baseMap,
                                                                 const std::filesystem::path & targetMap,
                                                                 const std::filesystem::path & diffDirectory,
                                                                 std::uint32_t mapIndex,
                                                                 Dimensions dimensions,
                                                                 TileRegion region,
                                                                 bool overwrite)
{
    if (auto base = MapReader::open(baseMap, dimensions); !base)
    {
        return std::unexpected(base.error());
    }
    if (auto valid = detail::MapHelper::validateRegion(region, dimensions, baseMap); !valid)
    {
        return std::unexpected(valid.error());
    }
    auto filesResult = prepareDiffTemporaries(diffDirectory, mapIndex, overwrite);
    if (!filesResult)
    {
        return std::unexpected(filesResult.error());
    }
    auto files = std::move(*filesResult);
    auto report = scanDiffTemporaries(baseMap, targetMap, dimensions, files, detail::blockRegionForTiles(region), region);
    if (!report)
    {
        return std::unexpected(report.error());
    }
    return publishDiffFiles(files.outputs, files, overwrite, *report);
}

std::expected<DiffReport, Error> MapDiff::generateFromBlockRegion(const std::filesystem::path & baseMap,
                                                                  const std::filesystem::path & targetMap,
                                                                  const std::filesystem::path & diffDirectory,
                                                                  std::uint32_t mapIndex,
                                                                  Dimensions dimensions,
                                                                  BlockRegion region,
                                                                  bool overwrite)
{
    if (auto base = MapReader::open(baseMap, dimensions); !base)
    {
        return std::unexpected(base.error());
    }
    if (auto valid = detail::MapHelper::validateRegion(region, dimensions, baseMap); !valid)
    {
        return std::unexpected(valid.error());
    }
    auto filesResult = prepareDiffTemporaries(diffDirectory, mapIndex, overwrite);
    if (!filesResult)
    {
        return std::unexpected(filesResult.error());
    }
    auto files = std::move(*filesResult);
    auto report = scanDiffTemporaries(baseMap, targetMap, dimensions, files, region);
    if (!report)
    {
        return std::unexpected(report.error());
    }
    return publishDiffFiles(files.outputs, files, overwrite, *report);
}

std::expected<void, Error> MapDiff::apply(const std::filesystem::path & baseMap,
                                          const std::filesystem::path & diffDirectory,
                                          const std::filesystem::path & outputMap,
                                          std::uint32_t mapIndex,
                                          Dimensions dimensions,
                                          bool overwrite)
{
    const auto dataPath = diffDirectory / ("mapdif" + std::to_string(mapIndex) + ".mul");
    const auto indexPath = diffDirectory / ("mapdifl" + std::to_string(mapIndex) + ".mul");
    auto baseReader = MapReader::open(baseMap, dimensions);
    if (!baseReader)
    {
        return std::unexpected(baseReader.error());
    }

    std::error_code filesystemError;
    if (!std::filesystem::is_regular_file(dataPath, filesystemError) || filesystemError ||
        !std::filesystem::is_regular_file(indexPath, filesystemError) || filesystemError)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, diffDirectory, 0, "both map diff files are required"));
    }
    std::ifstream data(dataPath, std::ios::binary);
    std::ifstream index(indexPath, std::ios::binary);
    if (!data || !index)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, diffDirectory, 0, "could not open diff files"));
    }
    auto lengths = validateDiffLengths(data, index, dataPath, diffDirectory);
    if (!lengths)
    {
        return std::unexpected(lengths.error());
    }
    const auto expectedBlockCount = MapHelper::blockCount(dimensions, baseMap);
    if (!expectedBlockCount)
    {
        return std::unexpected(expectedBlockCount.error());
    }
    if (*expectedBlockCount > std::numeric_limits<std::uint32_t>::max())
    {
        return std::unexpected(MapHelper::makeError(
            ErrorCode::invalidDimensions, baseMap, 0, "map has more blocks than diff indices can address"));
    }

    auto editor = MapEditor::create(baseMap, outputMap, dimensions, EditOptions{.overwriteOutput = overwrite});
    if (!editor)
    {
        return std::unexpected(editor.error());
    }
    auto replacements =
        readDiffReplacements(data, index, dataPath, indexPath, dimensions, *expectedBlockCount, lengths->blockCount);
    if (!replacements)
    {
        return std::unexpected(replacements.error());
    }
    if (auto written = writeDiffReplacements(*editor, *replacements); !written)
    {
        return written;
    }

    return editor->commit();
}

std::expected<DiffReport, Error> MapDiff::report(const std::filesystem::path & sourceMap,
                                                 const std::filesystem::path & targetMap,
                                                 const std::filesystem::path & reportFile,
                                                 Dimensions dimensions,
                                                 bool overwrite)
{
    return writeMapCsv(sourceMap, targetMap, reportFile, dimensions, overwrite);
}

std::expected<Dimensions, Error> facetDimensions(const std::string & name)
{
    std::string normalized = name;
    std::ranges::transform(
        normalized, normalized.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    if (normalized == "felucca" || normalized == "trammel")
    {
        return Dimensions{.widthTiles = feluccaWidth, .heightTiles = feluccaHeight};
    }
    if (normalized == "ilshenar")
    {
        return Dimensions{.widthTiles = ilshenarWidth, .heightTiles = ilshenarHeight};
    }
    if (normalized == "malas")
    {
        return Dimensions{.widthTiles = malasWidth, .heightTiles = malasHeight};
    }
    if (normalized == "tokuno")
    {
        return Dimensions{.widthTiles = tokunoSize, .heightTiles = tokunoSize};
    }
    if (normalized == "ter-mur" || normalized == "termur")
    {
        return Dimensions{.widthTiles = termurWidth, .heightTiles = feluccaHeight};
    }

    return std::unexpected(MapHelper::makeError(ErrorCode::invalidDimensions, {}, 0, "unknown facet preset"));
}

std::expected<void, Error> writeCsvReport(const std::filesystem::path & sourceMap,
                                          const std::filesystem::path & targetMap,
                                          const std::filesystem::path & reportFile,
                                          Dimensions dimensions,
                                          bool overwrite)
{
    auto report = writeMapCsv(sourceMap, targetMap, reportFile, dimensions, overwrite);
    if (!report)
    {
        return std::unexpected(report.error());
    }

    return {};
}

} // namespace uomul
