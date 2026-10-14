#include "uomul/map.h"
#include <algorithm>
#include <array>
#include <atomic>
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
#include <iterator>
#include <limits>
#include <memory>
#include <ostream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

namespace uomul
{

namespace
{

constexpr std::uint64_t landBlockByteSize = landBlockByteCount;
constexpr std::uint64_t landTileByteSize = 3;
constexpr std::uint64_t landHeaderByteSize = 4;
constexpr std::uint64_t mapBlockIdByteCount = 4;
constexpr std::uint32_t blockLocalWidth = landBlockDimension;
constexpr std::uint32_t bitsPerByte = std::numeric_limits<unsigned char>::digits;
constexpr int maximumAttempts = 16;
constexpr std::uint32_t byteMask = std::numeric_limits<std::uint8_t>::max();
constexpr std::uint64_t journalHeaderBytes = sizeof(std::uint64_t);
constexpr std::size_t bufferSize = 1'048'576U;
constexpr std::uint32_t feluccaWidth = 6144;
constexpr std::uint32_t feluccaHeight = 4096;
constexpr std::uint32_t ilshenarWidth = 2304;
constexpr std::uint32_t ilshenarHeight = 1600;
constexpr std::uint32_t malasWidth = 2560;
constexpr std::uint32_t malasHeight = 2048;
constexpr std::uint32_t tokunoSize = 1448;
constexpr std::uint32_t termurWidth = 1280;
std::atomic<std::uint64_t> temporarySequence{0};

[[nodiscard]] Error makeError(ErrorCode code, const std::filesystem::path & path, std::uint64_t offset, std::string message)
{
    return Error{.code = code, .path = path, .offset = offset, .message = std::move(message)};
}

[[nodiscard]] std::expected<std::uint64_t, Error> blockCount(Dimensions dimensions, const std::filesystem::path & path)
{
    if (dimensions.widthTiles == 0 || dimensions.heightTiles == 0 || dimensions.widthTiles % landBlockDimension != 0 ||
        dimensions.heightTiles % landBlockDimension != 0)
    {
        return std::unexpected(makeError(ErrorCode::invalidDimensions, path, 0, "dimensions must be positive multiples of 8"));
    }

    const auto width = static_cast<std::uint64_t>(dimensions.widthTiles / landBlockDimension);
    const auto height = static_cast<std::uint64_t>(dimensions.heightTiles / landBlockDimension);
    const auto blocks = width * height;

    if (blocks > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()) / landBlockByteSize)
    {
        return std::unexpected(
            makeError(ErrorCode::invalidDimensions, path, 0, "map file size exceeds supported offset range"));
    }

    return blocks;
}

[[nodiscard]] std::expected<void, Error>
validateBlock(BlockCoord block, Dimensions dimensions, const std::filesystem::path & path)
{
    if (block.x >= dimensions.widthTiles / landBlockDimension || block.y >= dimensions.heightTiles / landBlockDimension)
    {
        return std::unexpected(makeError(ErrorCode::invalidCoordinate, path, 0, "block coordinate is outside map dimensions"));
    }

    return {};
}

[[nodiscard]] std::expected<void, Error> validateTile(TileCoord tile, Dimensions dimensions, const std::filesystem::path & path)
{
    if (tile.x >= dimensions.widthTiles || tile.y >= dimensions.heightTiles)
    {
        return std::unexpected(makeError(ErrorCode::invalidCoordinate, path, 0, "tile coordinate is outside map dimensions"));
    }

    return {};
}

[[nodiscard]] std::uint64_t blockId(BlockCoord block, Dimensions dimensions)
{
    return (static_cast<std::uint64_t>(block.x) * (dimensions.heightTiles / landBlockDimension)) + block.y;
}

[[nodiscard]] std::uint64_t blockOffset(BlockCoord block, Dimensions dimensions)
{
    return blockId(block, dimensions) * landBlockByteSize;
}

[[nodiscard]] bool readExact(int descriptor, void * data, std::size_t size, std::uint64_t offset)
{
    auto * bytes = static_cast<std::byte *>(data);
    std::size_t readBytes = 0;

    while (readBytes < size)
    {
        const auto result = ::pread(descriptor,
                                    std::next(bytes, static_cast<std::ptrdiff_t>(readBytes)),
                                    size - readBytes,
                                    static_cast<off_t>(offset + readBytes));

        if (result < 0 && errno == EINTR)
        {
            continue;
        }

        if (result <= 0)
        {
            return false;
        }

        readBytes += static_cast<std::size_t>(result);
    }

    return true;
}

[[nodiscard]] bool writeExact(int descriptor, const void * data, std::size_t size, std::uint64_t offset)
{
    const auto * bytes = static_cast<const std::byte *>(data);
    std::size_t bytesWritten = 0;

    while (bytesWritten < size)
    {
        const auto result = ::pwrite(descriptor,
                                     std::next(bytes, static_cast<std::ptrdiff_t>(bytesWritten)),
                                     size - bytesWritten,
                                     static_cast<off_t>(offset + bytesWritten));

        if (result < 0 && errno == EINTR)
        {
            continue;
        }

        if (result <= 0)
        {
            return false;
        }

        bytesWritten += static_cast<std::size_t>(result);
    }

    return true;
}

[[nodiscard]] LandBlock decodeBlock(const LandBlockBytes & raw)
{
    LandBlock block{};

    for (std::size_t index = 0; index < block.size(); ++index)
    {
        const auto offset = static_cast<std::size_t>(landHeaderByteSize) + (index * static_cast<std::size_t>(landTileByteSize));
        const auto low = std::to_integer<std::uint8_t>(raw.at(offset));
        const auto high = std::to_integer<std::uint8_t>(raw.at(offset + 1));
        block.at(index) = LandTile{
            .id =
                static_cast<std::uint16_t>(static_cast<std::uint32_t>(low) | (static_cast<std::uint32_t>(high) << bitsPerByte)),
            .z = static_cast<std::int8_t>(std::to_integer<std::uint8_t>(raw.at(offset + 2))),
        };
    }

    return block;
}

void encodeBlock(LandBlockBytes & raw, const LandBlock & block)
{
    for (std::size_t index = 0; index < block.size(); ++index)
    {
        const auto offset = static_cast<std::size_t>(landHeaderByteSize) + (index * static_cast<std::size_t>(landTileByteSize));
        raw.at(offset) = static_cast<std::byte>(block.at(index).id & byteMask);
        raw.at(offset + 1) = static_cast<std::byte>(block.at(index).id >> bitsPerByte);
        raw.at(offset + 2) = static_cast<std::byte>(static_cast<std::uint8_t>(block.at(index).z));
    }
}

[[nodiscard]] std::filesystem::path temporaryPath(const std::filesystem::path & path)
{
    const auto sequence = temporarySequence.fetch_add(1, std::memory_order_relaxed);
    return path.parent_path() /
           (path.filename().string() + ".uomul-tmp-" + std::to_string(::getpid()) + "-" + std::to_string(sequence));
}

[[nodiscard]] std::expected<int, Error> createTemporary(const std::filesystem::path & path, std::filesystem::path & temporary)
{
    for (int attempt = 0; attempt < maximumAttempts; ++attempt)
    {
        temporary = temporaryPath(path);
        const int descriptor = ::open(temporary.c_str(), O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600);

        if (descriptor >= 0)
        {
            return descriptor;
        }

        if (errno != EEXIST)
        {
            return std::unexpected(makeError(ErrorCode::io, temporary, 0, std::strerror(errno)));
        }
    }

    return std::unexpected(makeError(ErrorCode::io, path, 0, "unable to allocate unique temporary file"));
}

[[nodiscard]] std::expected<void, Error>
publishFile(const std::filesystem::path & temporary, const std::filesystem::path & destination, bool overwrite)
{
    std::error_code filesystemError;
    const bool destinationExists = std::filesystem::exists(destination, filesystemError);

    if (filesystemError)
    {
        return std::unexpected(makeError(ErrorCode::commitFailed, destination, 0, filesystemError.message()));
    }

    if (destinationExists && !overwrite)
    {
        return std::unexpected(makeError(ErrorCode::commitFailed, destination, 0, "output already exists"));
    }

    if (!destinationExists)
    {
        if (::link(temporary.c_str(), destination.c_str()) == 0)
        {
            static_cast<void>(::unlink(temporary.c_str()));
            return {};
        }

        return std::unexpected(makeError(ErrorCode::commitFailed, destination, 0, std::strerror(errno)));
    }

    const auto backup = temporaryPath(destination);
    if (::rename(destination.c_str(), backup.c_str()) != 0)
    {
        return std::unexpected(makeError(ErrorCode::commitFailed, destination, 0, std::strerror(errno)));
    }

    if (::rename(temporary.c_str(), destination.c_str()) != 0)
    {
        const int savedErrno = errno;
        static_cast<void>(::rename(backup.c_str(), destination.c_str()));
        return std::unexpected(makeError(ErrorCode::commitFailed, destination, 0, std::strerror(savedErrno)));
    }

    static_cast<void>(::unlink(backup.c_str()));
    return {};
}

[[nodiscard]] std::expected<void, Error>
validateRegion(BlockRegion region, Dimensions dimensions, const std::filesystem::path & path)
{
    if (region.min.x > region.maxExclusive.x || region.min.y > region.maxExclusive.y ||
        region.maxExclusive.x > dimensions.widthTiles / landBlockDimension ||
        region.maxExclusive.y > dimensions.heightTiles / landBlockDimension)
    {
        return std::unexpected(
            makeError(ErrorCode::invalidCoordinate, path, 0, "block region is invalid or exceeds map dimensions"));
    }

    return {};
}

[[nodiscard]] std::expected<void, Error>
validateRegion(TileRegion region, Dimensions dimensions, const std::filesystem::path & path)
{
    if (region.min.x > region.maxExclusive.x || region.min.y > region.maxExclusive.y ||
        region.maxExclusive.x > dimensions.widthTiles || region.maxExclusive.y > dimensions.heightTiles)
    {
        return std::unexpected(
            makeError(ErrorCode::invalidCoordinate, path, 0, "tile region is invalid or exceeds map dimensions"));
    }

    return {};
}

[[nodiscard]] std::expected<LandBlock, Error>
readRawBlock(int descriptor, const std::filesystem::path & path, Dimensions dimensions, BlockCoord block)
{
    if (auto valid = validateBlock(block, dimensions, path); !valid)
    {
        return std::unexpected(valid.error());
    }

    LandBlockBytes raw{};
    const auto offset = blockOffset(block, dimensions);

    if (!readExact(descriptor, raw.data(), raw.size(), offset))
    {
        return std::unexpected(makeError(ErrorCode::truncatedInput, path, offset, "could not read complete land block"));
    }

    return decodeBlock(raw);
}

[[nodiscard]] std::expected<DiffReport, Error>
compareMaps(const std::filesystem::path & sourcePath,
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
        return std::unexpected(makeError(ErrorCode::io, targetPath, 0, std::strerror(errno)));
    }

    DiffReport report{};
    const BlockRegion fullRegion{
        .min = {.x = 0, .y = 0},
        .maxExclusive = {.x = dimensions.widthTiles / landBlockDimension, .y = dimensions.heightTiles / landBlockDimension},
    };
    auto scan = source->readRawLandBlocks(
        fullRegion,
        [&](BlockCoord coordinate, const LandBlockBytes & sourceRaw) -> std::expected<void, Error>
        {
            LandBlockBytes targetRaw{};
            const auto offset = blockOffset(coordinate, dimensions);
            if (!readExact(targetDescriptor, targetRaw.data(), targetRaw.size(), offset))
            {
                return std::unexpected(makeError(ErrorCode::truncatedInput, targetPath, offset, "could not read target block"));
            }

            const auto left = decodeBlock(sourceRaw);
            const auto right = decodeBlock(targetRaw);
            std::uint64_t changedTiles = 0;
            for (std::size_t index = 0; index < left.size(); ++index)
            {
                changedTiles += left.at(index) != right.at(index) ? 1U : 0U;
            }

            if (changedTiles != 0)
            {
                ++report.landBlocksChanged;
                report.landTilesChanged += changedTiles;
                if (onChange)
                {
                    onChange(blockId(coordinate, dimensions), coordinate, changedTiles);
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

[[nodiscard]] std::expected<void, Error> checkOutputPath(const std::filesystem::path & path, bool overwrite)
{
    std::error_code filesystemError;
    const bool exists = std::filesystem::exists(path, filesystemError);

    if (filesystemError)
    {
        return std::unexpected(makeError(ErrorCode::io, path, 0, filesystemError.message()));
    }

    if (!overwrite && exists)
    {
        return std::unexpected(makeError(ErrorCode::io, path, 0, "output already exists"));
    }

    return {};
}

[[nodiscard]] bool writeLe32(std::ostream & stream, std::uint32_t value)
{
    const std::array<char, mapBlockIdByteCount> bytes{
        static_cast<char>(value & byteMask),
        static_cast<char>((value >> bitsPerByte) & byteMask),
        static_cast<char>((value >> (2 * bitsPerByte)) & byteMask),
        static_cast<char>((value >> (3 * bitsPerByte)) & byteMask),
    };
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return stream.good();
}

[[nodiscard]] bool readLe32(std::istream & stream, std::uint32_t & value)
{
    std::array<char, mapBlockIdByteCount> bytes{};
    stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!stream)
    {
        return false;
    }

    value = static_cast<std::uint32_t>(static_cast<unsigned char>(bytes.at(0))) |
            (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes.at(1))) << bitsPerByte) |
            (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes.at(2))) << (2 * bitsPerByte)) |
            (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes.at(3))) << (3 * bitsPerByte));
    return true;
}

[[nodiscard]] std::expected<DiffReport, Error> writeMapCsv(const std::filesystem::path & sourceMap,
                                                           const std::filesystem::path & targetMap,
                                                           const std::filesystem::path & reportFile,
                                                           Dimensions dimensions,
                                                           bool overwrite)
{
    if (auto result = checkOutputPath(reportFile, overwrite); !result)
    {
        return std::unexpected(result.error());
    }

    const auto parent = reportFile.parent_path();
    if (!parent.empty() && !std::filesystem::is_directory(parent))
    {
        return std::unexpected(makeError(ErrorCode::io, parent, 0, "report directory does not exist"));
    }

    std::filesystem::path temporary;
    auto descriptor = createTemporary(reportFile, temporary);
    if (!descriptor)
    {
        return std::unexpected(descriptor.error());
    }

    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        static_cast<void>(::close(*descriptor));
        static_cast<void>(::unlink(temporary.c_str()));
        return std::unexpected(makeError(ErrorCode::io, temporary, 0, "could not create report"));
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
        return std::unexpected(makeError(ErrorCode::io, temporary, 0, "could not write report"));
    }

    auto published = publishFile(temporary, reportFile, overwrite);
    if (!published)
    {
        return std::unexpected(published.error());
    }

    return *compared;
}

} // namespace

struct MapReader::Impl
{
    std::filesystem::path path;
    Dimensions dimensions{};
    int descriptor = -1;
    std::uint64_t blocks = 0;
    std::uint64_t ignoredBytes = 0;
    std::uint64_t missingBlocks = 0;
};

MapReader::MapReader(std::unique_ptr<Impl> impl) : impl_(std::move(impl))
{
}

MapReader::MapReader(MapReader &&) noexcept = default;
MapReader & MapReader::operator=(MapReader &&) noexcept = default;
MapReader::~MapReader()
{
    if (impl_ && impl_->descriptor >= 0)
    {
        static_cast<void>(::close(impl_->descriptor));
    }
}

std::expected<MapReader, Error> MapReader::open(const std::filesystem::path & path, Dimensions dimensions, OpenOptions options)
{
    auto count = blockCount(dimensions, path);
    if (!count)
    {
        return std::unexpected(count.error());
    }

    const int descriptor = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (descriptor < 0)
    {
        return std::unexpected(makeError(ErrorCode::io, path, 0, std::strerror(errno)));
    }

    struct stat fileStatus{};
    if (::fstat(descriptor, &fileStatus) != 0)
    {
        const auto error = makeError(ErrorCode::io, path, 0, std::strerror(errno));
        static_cast<void>(::close(descriptor));
        return std::unexpected(error);
    }
    if (fileStatus.st_size < 0)
    {
        const auto error = makeError(ErrorCode::io, path, 0, "could not determine map size");
        static_cast<void>(::close(descriptor));
        return std::unexpected(error);
    }

    const auto fileSize = static_cast<std::uint64_t>(fileStatus.st_size);
    const auto expectedSize = *count * landBlockByteSize;
    const auto completeBlockCount = fileSize / landBlockByteSize;
    if (fileSize % landBlockByteSize != 0 && (fileSize > expectedSize || !options.recoverTruncatedMap))
    {
        const auto error =
            makeError(ErrorCode::truncatedInput, path, fileSize, "map has trailing bytes beyond complete blocks");
        static_cast<void>(::close(descriptor));
        return std::unexpected(error);
    }

    if (!options.recoverTruncatedMap && fileSize != expectedSize)
    {
        const auto error =
            makeError(ErrorCode::truncatedInput, path, fileSize, "map size does not match dimensions and complete blocks");
        static_cast<void>(::close(descriptor));
        return std::unexpected(error);
    }

    if (fileSize > expectedSize)
    {
        const auto error =
            makeError(ErrorCode::truncatedInput, path, expectedSize, "map contains more blocks than specified dimensions");
        static_cast<void>(::close(descriptor));
        return std::unexpected(error);
    }

    if (!S_ISREG(fileStatus.st_mode))
    {
        const auto error = makeError(ErrorCode::io, path, 0, "map input must be a regular file");
        static_cast<void>(::close(descriptor));
        return std::unexpected(error);
    }

    auto impl = std::make_unique<Impl>();
    impl->path = path;
    impl->dimensions = dimensions;
    impl->descriptor = descriptor;
    impl->blocks = std::min(*count, completeBlockCount);
    impl->missingBlocks = *count - impl->blocks;
    impl->ignoredBytes = fileSize % landBlockByteSize;
    return MapReader(std::move(impl));
}

Dimensions MapReader::dimensions() const noexcept
{
    return impl_->dimensions;
}

std::uint64_t MapReader::completeBlocks() const noexcept
{
    return impl_->blocks;
}

std::uint64_t MapReader::ignoredTrailingBytes() const noexcept
{
    return impl_->ignoredBytes;
}

std::uint64_t MapReader::missingBlocks() const noexcept
{
    return impl_->missingBlocks;
}

std::expected<LandBlock, Error> MapReader::readLandBlock(BlockCoord block) const
{
    if (auto valid = validateBlock(block, impl_->dimensions, impl_->path); !valid)
    {
        return std::unexpected(valid.error());
    }

    LandBlockBytes raw{};
    const auto offset = blockOffset(block, impl_->dimensions);
    if (blockId(block, impl_->dimensions) >= impl_->blocks || !readExact(impl_->descriptor, raw.data(), raw.size(), offset))
    {
        return std::unexpected(makeError(ErrorCode::truncatedInput, impl_->path, offset, "land block is missing or truncated"));
    }

    return decodeBlock(raw);
}

std::expected<LandTile, Error> MapReader::readLandTile(TileCoord tile) const
{
    if (auto valid = validateTile(tile, impl_->dimensions, impl_->path); !valid)
    {
        return std::unexpected(valid.error());
    }

    auto block = readLandBlock({.x = tile.x / landBlockDimension, .y = tile.y / landBlockDimension});
    if (!block)
    {
        return std::unexpected(block.error());
    }

    return block->at(((tile.y % landBlockDimension) * blockLocalWidth) + (tile.x % landBlockDimension));
}

std::expected<void, Error> MapReader::readLandBlocks(BlockRegion region, const LandBlockRead & callback) const
{
    if (auto valid = validateRegion(region, impl_->dimensions, impl_->path); !valid)
    {
        return valid;
    }

    if (!callback)
    {
        return std::unexpected(makeError(ErrorCode::io, impl_->path, 0, "read callback is empty"));
    }

    for (std::uint32_t x = region.min.x; x < region.maxExclusive.x; ++x)
    {
        for (std::uint32_t y = region.min.y; y < region.maxExclusive.y; ++y)
        {
            auto block = readLandBlock({.x = x, .y = y});
            if (!block)
            {
                return std::unexpected(block.error());
            }

            auto result = callback(BlockCoord{.x = x, .y = y}, *block);
            if (!result)
            {
                return result;
            }
        }
    }

    return {};
}

std::expected<void, Error> MapReader::readRawLandBlocks(BlockRegion region, const LandBlockBytesRead & callback) const
{
    if (auto valid = validateRegion(region, impl_->dimensions, impl_->path); !valid)
    {
        return valid;
    }

    LandBlockBytes raw{};
    if (!callback)
    {
        return std::unexpected(makeError(ErrorCode::io, impl_->path, 0, "read callback is empty"));
    }

    for (std::uint32_t x = region.min.x; x < region.maxExclusive.x; ++x)
    {
        for (std::uint32_t y = region.min.y; y < region.maxExclusive.y; ++y)
        {
            const BlockCoord coordinate{.x = x, .y = y};
            const auto offset = blockOffset(coordinate, impl_->dimensions);
            if (blockId(coordinate, impl_->dimensions) >= impl_->blocks ||
                !readExact(impl_->descriptor, raw.data(), raw.size(), offset))
            {
                return std::unexpected(
                    makeError(ErrorCode::truncatedInput, impl_->path, offset, "land block is missing or truncated"));
            }

            auto result = callback(coordinate, raw);
            if (!result)
            {
                return result;
            }
        }
    }

    return {};
}

std::expected<void, Error> MapReader::readLandTiles(TileRegion region, const LandTileRead & callback) const
{
    if (auto valid = validateRegion(region, impl_->dimensions, impl_->path); !valid)
    {
        return valid;
    }

    if (!callback)
    {
        return std::unexpected(makeError(ErrorCode::io, impl_->path, 0, "read callback is empty"));
    }

    const std::uint32_t firstBlockX = region.min.x / landBlockDimension;
    const std::uint32_t firstBlockY = region.min.y / landBlockDimension;
    const std::uint32_t lastBlockX =
        (region.maxExclusive.x / landBlockDimension) + (region.maxExclusive.x % landBlockDimension == 0 ? 0U : 1U);
    const std::uint32_t lastBlockY =
        (region.maxExclusive.y / landBlockDimension) + (region.maxExclusive.y % landBlockDimension == 0 ? 0U : 1U);

    for (std::uint32_t blockX = firstBlockX; blockX < lastBlockX; ++blockX)
    {
        for (std::uint32_t blockY = firstBlockY; blockY < lastBlockY; ++blockY)
        {
            auto block = readLandBlock({.x = blockX, .y = blockY});
            if (!block)
            {
                return std::unexpected(block.error());
            }

            const auto minX = std::max(region.min.x, blockX * landBlockDimension);
            const auto minY = std::max(region.min.y, blockY * landBlockDimension);
            const auto maxX = std::min(region.maxExclusive.x, (blockX * landBlockDimension) + blockLocalWidth);
            const auto maxY = std::min(region.maxExclusive.y, (blockY * landBlockDimension) + blockLocalWidth);

            for (std::uint32_t x = minX; x < maxX; ++x)
            {
                for (std::uint32_t y = minY; y < maxY; ++y)
                {
                    auto const result =
                        callback(TileCoord{.x = x, .y = y},
                                 block->at(((y % landBlockDimension) * blockLocalWidth) + (x % landBlockDimension)));
                    if (!result)
                    {
                        return result;
                    }
                }
            }
        }
    }

    return {};
}

struct MapEditor::Impl
{
    std::filesystem::path source;
    std::filesystem::path output;
    std::filesystem::path staging;
    Dimensions dimensions{};
    int descriptor = -1;
    bool overwrite = false;
    bool committed = false;
    bool failed = false;
};

MapEditor::MapEditor(std::unique_ptr<Impl> impl) : impl_(std::move(impl))
{
}
MapEditor::MapEditor(MapEditor &&) noexcept = default;
MapEditor & MapEditor::operator=(MapEditor &&) noexcept = default;
MapEditor::~MapEditor()
{
    abort();
}

std::expected<MapEditor, Error> MapEditor::create(const std::filesystem::path & sourceMap,
                                                  const std::filesystem::path & outputMap,
                                                  Dimensions dimensions,
                                                  EditOptions options)
{
    auto source = MapReader::open(sourceMap, dimensions);
    if (!source)
    {
        return std::unexpected(source.error());
    }

    std::error_code pathError;
    const auto sourceAbsolute = std::filesystem::absolute(sourceMap, pathError).lexically_normal();

    if (pathError)
    {
        return std::unexpected(makeError(ErrorCode::io, sourceMap, 0, pathError.message()));
    }
    const auto outputAbsolute = std::filesystem::absolute(outputMap, pathError).lexically_normal();

    if (pathError)
    {
        return std::unexpected(makeError(ErrorCode::io, outputMap, 0, pathError.message()));
    }

    if (sourceAbsolute == outputAbsolute)
    {
        return std::unexpected(
            makeError(ErrorCode::io, outputMap, 0, "in-place editing is not supported; choose a separate output path"));
    }
    pathError.clear();
    const bool outputExists = std::filesystem::exists(outputMap, pathError);

    if (pathError)
    {
        return std::unexpected(makeError(ErrorCode::io, outputMap, 0, pathError.message()));
    }

    if (outputExists && std::filesystem::equivalent(sourceMap, outputMap, pathError))
    {
        return std::unexpected(makeError(ErrorCode::io, outputMap, 0, "output path resolves to the source map"));
    }

    if (pathError)
    {
        return std::unexpected(makeError(ErrorCode::io, outputMap, 0, pathError.message()));
    }

    if (auto check = checkOutputPath(outputMap, options.overwriteOutput); !check)
    {
        return std::unexpected(check.error());
    }

    const auto parent = outputMap.parent_path();
    std::error_code directoryError;
    const bool parentExists = parent.empty() || std::filesystem::is_directory(parent, directoryError);
    if (directoryError || !parentExists)
    {
        return std::unexpected(
            makeError(ErrorCode::io, parent, 0, directoryError ? directoryError.message() : "output directory does not exist"));
    }

    auto impl = std::make_unique<Impl>();
    impl->source = sourceMap;
    impl->output = outputMap;
    impl->dimensions = dimensions;
    impl->overwrite = options.overwriteOutput;

    auto temporary_descriptor = createTemporary(outputMap, impl->staging);
    if (!temporary_descriptor)
    {
        return std::unexpected(temporary_descriptor.error());
    }
    impl->descriptor = *temporary_descriptor;

    const int sourceDescriptor = ::open(sourceMap.c_str(), O_RDONLY | O_CLOEXEC);
    if (sourceDescriptor < 0)
    {
        const auto error = makeError(ErrorCode::io, sourceMap, 0, std::strerror(errno));
        static_cast<void>(::close(impl->descriptor));
        static_cast<void>(::unlink(impl->staging.c_str()));
        return std::unexpected(error);
    }
    std::array<std::byte, bufferSize> buffer{};
    std::uint64_t offset = 0;
    const auto totalBytes = source->completeBlocks() * landBlockByteSize;

    struct stat sourceStatus{};
    if (::fstat(sourceDescriptor, &sourceStatus) != 0 || !S_ISREG(sourceStatus.st_mode))
    {
        const auto error = makeError(ErrorCode::io, sourceMap, 0, "map input must be a regular file");
        static_cast<void>(::close(sourceDescriptor));
        static_cast<void>(::close(impl->descriptor));
        impl->descriptor = -1;
        static_cast<void>(::unlink(impl->staging.c_str()));
        return std::unexpected(error);
    }

    while (offset < totalBytes)
    {
        const auto amount = static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(), totalBytes - offset));
        if (!readExact(sourceDescriptor, buffer.data(), amount, offset) ||
            !writeExact(impl->descriptor, buffer.data(), amount, offset))
        {
            const auto error = makeError(ErrorCode::io, impl->staging, offset, "failed to stage source map");
            static_cast<void>(::close(sourceDescriptor));
            static_cast<void>(::close(impl->descriptor));
            impl->descriptor = -1;
            static_cast<void>(::unlink(impl->staging.c_str()));
            return std::unexpected(error);
        }
        offset += amount;
    }

    static_cast<void>(::close(sourceDescriptor));
    return MapEditor(std::move(impl));
}

std::expected<void, Error> MapEditor::writeLandBlock(BlockCoord block, const LandBlock & data)
{
    if (impl_->committed || impl_->descriptor < 0 || impl_->failed)
    {
        return std::unexpected(makeError(ErrorCode::commitFailed, impl_->output, 0, "editor is no longer active"));
    }

    if (auto valid = validateBlock(block, impl_->dimensions, impl_->source); !valid)
    {
        return valid;
    }

    LandBlockBytes raw{};
    const auto offset = blockOffset(block, impl_->dimensions);
    if (!readExact(impl_->descriptor, raw.data(), raw.size(), offset))
    {
        return std::unexpected(makeError(ErrorCode::io, impl_->staging, offset, "could not read staged block"));
    }

    encodeBlock(raw, data);
    if (!writeExact(impl_->descriptor, raw.data(), raw.size(), offset))
    {
        return std::unexpected(makeError(ErrorCode::io, impl_->staging, offset, "could not stage land block"));
    }

    return {};
}

std::expected<void, Error> MapEditor::writeLandTile(TileCoord tile, LandTile data)
{
    if (impl_->committed || impl_->descriptor < 0 || impl_->failed)
    {
        return std::unexpected(makeError(ErrorCode::commitFailed, impl_->output, 0, "editor is no longer active"));
    }

    if (auto valid = validateTile(tile, impl_->dimensions, impl_->source); !valid)
    {
        return valid;
    }

    const BlockCoord coordinate{.x = tile.x / landBlockDimension, .y = tile.y / landBlockDimension};
    auto block = readRawBlock(impl_->descriptor, impl_->staging, impl_->dimensions, coordinate);
    if (!block)
    {
        return std::unexpected(block.error());
    }

    block->at(((tile.y % landBlockDimension) * blockLocalWidth) + (tile.x % landBlockDimension)) = data;
    return writeLandBlock(coordinate, *block);
}

namespace
{

constexpr std::uint64_t journalEntryBytes = journalHeaderBytes + (2 * landBlockByteSize);

[[nodiscard]] std::expected<std::uint64_t, Error> writeEditJournal(int stagedDescriptor,
                                                                   const std::filesystem::path & staging,
                                                                   int journal,
                                                                   const std::filesystem::path & journalPath,
                                                                   Dimensions dimensions,
                                                                   BlockRegion region,
                                                                   const LandBlockEdit & callback)
{
    std::uint64_t entryCount = 0;
    for (std::uint32_t x = region.min.x; x < region.maxExclusive.x; ++x)
    {
        for (std::uint32_t y = region.min.y; y < region.maxExclusive.y; ++y)
        {
            const BlockCoord coordinate{.x = x, .y = y};
            const auto offset = blockOffset(coordinate, dimensions);
            LandBlockBytes before{};
            if (!readExact(stagedDescriptor, before.data(), before.size(), offset))
            {
                return std::unexpected(makeError(ErrorCode::io, staging, offset, "could not read staged block"));
            }

            auto changed = callback(coordinate, decodeBlock(before));
            if (!changed)
            {
                return std::unexpected(changed.error());
            }

            LandBlockBytes after = before;
            encodeBlock(after, *changed);
            std::array<std::byte, journalEntryBytes> entry{};
            for (std::size_t byte = 0; byte < sizeof(offset); ++byte)
            {
                entry.at(byte) = static_cast<std::byte>((offset >> (byte * bitsPerByte)) & byteMask);
            }

            const auto beforeOffset = static_cast<std::size_t>(journalHeaderBytes);
            const auto afterOffset = beforeOffset + static_cast<std::size_t>(landBlockByteSize);
            std::ranges::copy(before, std::next(entry.begin(), static_cast<std::ptrdiff_t>(beforeOffset)));
            std::ranges::copy(after, std::next(entry.begin(), static_cast<std::ptrdiff_t>(afterOffset)));
            if (!writeExact(journal, entry.data(), entry.size(), entryCount * journalEntryBytes))
            {
                return std::unexpected(
                    makeError(ErrorCode::io, journalPath, entryCount * journalEntryBytes, "could not write edit journal"));
            }
            ++entryCount;
        }
    }

    return entryCount;
}

[[nodiscard]] bool replayEditJournal(int stagedDescriptor, int journal, std::uint64_t entryCount, std::uint64_t & attempted)
{
    std::array<std::byte, journalEntryBytes> entry{};
    for (attempted = 0; attempted < entryCount; ++attempted)
    {
        if (!readExact(journal, entry.data(), entry.size(), attempted * journalEntryBytes))
        {
            return false;
        }

        std::uint64_t offset = 0;
        for (std::size_t byte = 0; byte < sizeof(offset); ++byte)
        {
            offset |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(entry.at(byte))) << (byte * bitsPerByte);
        }
        const auto afterOffset = static_cast<std::size_t>(journalHeaderBytes + landBlockByteSize);
        if (!writeExact(stagedDescriptor,
                        std::next(entry.begin(), static_cast<std::ptrdiff_t>(afterOffset)),
                        static_cast<std::size_t>(landBlockByteSize),
                        offset))
        {
            ++attempted;
            return false;
        }
    }

    return true;
}

[[nodiscard]] bool rollbackEditJournal(int stagedDescriptor, int journal, std::uint64_t entryCount)
{
    std::array<std::byte, journalEntryBytes> entry{};
    bool rollbackSucceeded = true;
    for (std::uint64_t item = 0; item < entryCount; ++item)
    {
        if (!readExact(journal, entry.data(), entry.size(), item * journalEntryBytes))
        {
            rollbackSucceeded = false;
            continue;
        }

        std::uint64_t offset = 0;
        for (std::size_t byte = 0; byte < sizeof(offset); ++byte)
        {
            offset |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(entry.at(byte))) << (byte * bitsPerByte);
        }
        const auto beforeOffset = static_cast<std::size_t>(journalHeaderBytes);
        if (!writeExact(stagedDescriptor,
                        std::next(entry.begin(), static_cast<std::ptrdiff_t>(beforeOffset)),
                        static_cast<std::size_t>(landBlockByteSize),
                        offset))
        {
            rollbackSucceeded = false;
        }
    }

    return rollbackSucceeded;
}

void discardEditJournal(int journal, const std::filesystem::path & journalPath)
{
    static_cast<void>(::close(journal));
    static_cast<void>(::unlink(journalPath.c_str()));
}

} // namespace

std::expected<void, Error> MapEditor::editLandBlocks(BlockRegion region, const LandBlockEdit & callback)
{
    if (impl_->committed || impl_->descriptor < 0 || impl_->failed)
    {
        return std::unexpected(makeError(ErrorCode::commitFailed, impl_->output, 0, "editor is no longer active"));
    }

    if (!callback)
    {
        return std::unexpected(makeError(ErrorCode::io, impl_->source, 0, "edit callback is empty"));
    }

    if (auto valid = validateRegion(region, impl_->dimensions, impl_->source); !valid)
    {
        return valid;
    }

    std::filesystem::path journalPath;
    auto journalDescriptor = createTemporary(impl_->output, journalPath);
    if (!journalDescriptor)
    {
        return std::unexpected(journalDescriptor.error());
    }

    const int journal = *journalDescriptor;
    auto entryCount =
        writeEditJournal(impl_->descriptor, impl_->staging, journal, journalPath, impl_->dimensions, region, callback);
    if (!entryCount)
    {
        discardEditJournal(journal, journalPath);
        return std::unexpected(entryCount.error());
    }

    std::uint64_t attempted = 0;
    if (!replayEditJournal(impl_->descriptor, journal, *entryCount, attempted))
    {
        const bool rollbackSucceeded = rollbackEditJournal(impl_->descriptor, journal, attempted);
        discardEditJournal(journal, journalPath);
        if (!rollbackSucceeded)
        {
            impl_->failed = true;
        }
        return std::unexpected(makeError(ErrorCode::io,
                                         impl_->staging,
                                         0,
                                         rollbackSucceeded ? "edit journal failed; staged changes were rolled back"
                                                           : "edit failed and rollback was incomplete"));
    }

    discardEditJournal(journal, journalPath);
    return {};
}

std::expected<void, Error> MapEditor::editLandTiles(TileRegion region, const LandTileEdit & callback)
{
    if (impl_->committed || impl_->descriptor < 0 || impl_->failed)
    {
        return std::unexpected(makeError(ErrorCode::commitFailed, impl_->output, 0, "editor is no longer active"));
    }

    if (!callback)
    {
        return std::unexpected(makeError(ErrorCode::io, impl_->source, 0, "edit callback is empty"));
    }

    if (auto valid = validateRegion(region, impl_->dimensions, impl_->source); !valid)
    {
        return valid;
    }

    BlockRegion const blocks{
        .min = {.x = region.min.x / landBlockDimension, .y = region.min.y / landBlockDimension},
        .maxExclusive =
            {
                .x = (region.maxExclusive.x / landBlockDimension) + (region.maxExclusive.x % landBlockDimension == 0 ? 0U : 1U),
                .y = (region.maxExclusive.y / landBlockDimension) + (region.maxExclusive.y % landBlockDimension == 0 ? 0U : 1U),
            },
    };
    return editLandBlocks(
        blocks,
        [&region, &callback](BlockCoord coordinate, const LandBlock & current) -> std::expected<LandBlock, Error>
        {
            LandBlock changed = current;
            const auto minX = std::max(region.min.x, coordinate.x * landBlockDimension);
            const auto minY = std::max(region.min.y, coordinate.y * landBlockDimension);
            const auto maxX = std::min(region.maxExclusive.x, (coordinate.x * landBlockDimension) + blockLocalWidth);
            const auto maxY = std::min(region.maxExclusive.y, (coordinate.y * landBlockDimension) + blockLocalWidth);
            for (std::uint32_t x = minX; x < maxX; ++x)
            {
                for (std::uint32_t y = minY; y < maxY; ++y)
                {
                    const TileCoord tile{.x = x, .y = y};
                    const auto index = ((y % landBlockDimension) * blockLocalWidth) + (x % landBlockDimension);

                    auto result = callback(tile, current.at(index));
                    if (!result)
                    {
                        return std::unexpected(result.error());
                    }
                    changed.at(index) = *result;
                }
            }

            return changed;
        });
}

std::expected<void, Error> MapEditor::fill(TileRegion region, LandTile value)
{
    return editLandTiles(region, [value](TileCoord, const LandTile &) -> std::expected<LandTile, Error> { return value; });
}

std::expected<void, Error> MapEditor::replaceIds(TileRegion region, std::uint16_t from, std::uint16_t to)
{
    return editLandTiles(region,
                         [from, to](TileCoord, const LandTile & tile) -> std::expected<LandTile, Error>
                         {
                             auto result = tile;
                             if (result.id == from)
                             {
                                 result.id = to;
                             }

                             return result;
                         });
}

std::expected<void, Error> MapEditor::replaceZ(TileRegion region, std::int8_t from, std::int8_t to)
{
    return editLandTiles(region,
                         [from, to](TileCoord, const LandTile & tile) -> std::expected<LandTile, Error>
                         {
                             auto result = tile;
                             if (result.z == from)
                             {
                                 result.z = to;
                             }

                             return result;
                         });
}

std::expected<void, Error> MapEditor::commit()
{
    if (impl_->committed || impl_->descriptor < 0)
    {
        return std::unexpected(makeError(ErrorCode::commitFailed, impl_->output, 0, "editor is no longer active"));
    }
    if (::fsync(impl_->descriptor) != 0)
    {
        const auto error = makeError(ErrorCode::commitFailed, impl_->staging, 0, std::strerror(errno));
        static_cast<void>(::close(impl_->descriptor));
        impl_->descriptor = -1;
        static_cast<void>(::unlink(impl_->staging.c_str()));
        impl_->committed = true;
        return std::unexpected(error);
    }
    if (::close(impl_->descriptor) != 0)
    {
        const auto error = makeError(ErrorCode::commitFailed, impl_->staging, 0, std::strerror(errno));
        impl_->descriptor = -1;
        static_cast<void>(::unlink(impl_->staging.c_str()));
        impl_->committed = true;
        return std::unexpected(error);
    }
    impl_->descriptor = -1;

    auto result = publishFile(impl_->staging, impl_->output, impl_->overwrite);
    if (!result)
    {
        static_cast<void>(::unlink(impl_->staging.c_str()));
        impl_->committed = true;
        return result;
    }

    impl_->committed = true;
    return {};
}

void MapEditor::abort() noexcept
{
    if (!impl_)
    {
        return;
    }

    if (impl_->descriptor >= 0)
    {
        static_cast<void>(::close(impl_->descriptor));
        impl_->descriptor = -1;
    }

    if (!impl_->committed && !impl_->staging.empty())
    {
        static_cast<void>(::unlink(impl_->staging.c_str()));
    }

    impl_->committed = true;
}

namespace
{

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

[[nodiscard]] std::expected<DiffTemporaryFiles, Error>
prepareDiffTemporaries(const std::filesystem::path & directory, std::uint32_t mapIndex, bool overwrite)
{
    std::error_code filesystemError;
    if (!std::filesystem::is_directory(directory, filesystemError) || filesystemError)
    {
        return std::unexpected(makeError(ErrorCode::io, directory, 0, "diff output directory does not exist"));
    }

    DiffTemporaryFiles files{};
    files.outputs.data = directory / ("mapdif" + std::to_string(mapIndex) + ".mul");
    files.outputs.index = directory / ("mapdifl" + std::to_string(mapIndex) + ".mul");
    if (auto result = checkOutputPath(files.outputs.data, overwrite); !result)
    {
        return std::unexpected(result.error());
    }
    if (auto result = checkOutputPath(files.outputs.index, overwrite); !result)
    {
        return std::unexpected(result.error());
    }

    auto dataDescriptor = createTemporary(files.outputs.data, files.data);
    if (!dataDescriptor)
    {
        return std::unexpected(dataDescriptor.error());
    }
    files.dataDescriptor = *dataDescriptor;
    auto indexDescriptor = createTemporary(files.outputs.index, files.index);
    if (!indexDescriptor)
    {
        removeDiffTemporaries(files);
        return std::unexpected(indexDescriptor.error());
    }
    files.indexDescriptor = *indexDescriptor;
    return files;
}

[[nodiscard]] std::expected<void, Error>
appendChangedDiffBlock(const DiffGenerationState & state, BlockCoord coordinate, const LandBlockBytes & sourceRaw)
{
    LandBlockBytes targetRaw{};
    const auto offset = blockOffset(coordinate, state.dimensions);
    if (!readExact(state.targetDescriptor, targetRaw.data(), targetRaw.size(), offset))
    {
        return std::unexpected(makeError(ErrorCode::truncatedInput, state.targetPath, offset, "could not read target block"));
    }

    const auto sourceBlock = decodeBlock(sourceRaw);
    const auto targetBlock = decodeBlock(targetRaw);
    std::uint64_t changedTiles = 0;
    for (std::size_t tile = 0; tile < sourceBlock.size(); ++tile)
    {
        changedTiles += sourceBlock.at(tile) != targetBlock.at(tile) ? 1U : 0U;
    }
    if (changedTiles == 0)
    {
        return {};
    }

    const auto id = blockId(coordinate, state.dimensions);
    if (id > std::numeric_limits<std::uint32_t>::max() || !writeLe32(*state.index, static_cast<std::uint32_t>(id)))
    {
        return std::unexpected(makeError(ErrorCode::io, state.indexTemporary, 0, "could not write diff index"));
    }
    for (const auto byte : targetRaw)
    {
        state.data->put(static_cast<char>(std::to_integer<unsigned char>(byte)));
    }
    if (!*state.data)
    {
        return std::unexpected(makeError(ErrorCode::io, state.dataTemporary, 0, "could not write diff block data"));
    }

    ++state.report->landBlocksChanged;
    state.report->landTilesChanged += changedTiles;
    return {};
}

[[nodiscard]] std::expected<DiffReport, Error> scanDiffTemporaries(const std::filesystem::path & baseMap,
                                                                   const std::filesystem::path & targetMap,
                                                                   Dimensions dimensions,
                                                                   DiffTemporaryFiles & files)
{
    std::ofstream data(files.data, std::ios::binary | std::ios::trunc);
    std::ofstream index(files.index, std::ios::binary | std::ios::trunc);
    if (!data || !index)
    {
        data.close();
        index.close();
        removeDiffTemporaries(files);
        return std::unexpected(makeError(ErrorCode::io, files.outputs.data.parent_path(), 0, "could not create diff files"));
    }

    auto source = MapReader::open(baseMap, dimensions);
    auto target = MapReader::open(targetMap, dimensions);
    if (!source || !target)
    {
        data.close();
        index.close();
        removeDiffTemporaries(files);
        return std::unexpected(!source ? source.error() : target.error());
    }

    struct stat targetStatus{};
    const int targetDescriptor = ::open(targetMap.c_str(), O_RDONLY | O_CLOEXEC);
    if (targetDescriptor < 0 || ::fstat(targetDescriptor, &targetStatus) != 0 || !S_ISREG(targetStatus.st_mode))
    {
        if (targetDescriptor >= 0)
        {
            static_cast<void>(::close(targetDescriptor));
        }
        data.close();
        index.close();
        removeDiffTemporaries(files);
        return std::unexpected(makeError(ErrorCode::io, targetMap, 0, std::strerror(errno)));
    }

    DiffReport report{};
    DiffGenerationState state{
        .targetPath = targetMap,
        .dataTemporary = files.data,
        .indexTemporary = files.index,
        .dimensions = dimensions,
        .targetDescriptor = targetDescriptor,
        .data = &data,
        .index = &index,
        .report = &report,
    };
    const BlockRegion fullRegion{
        .min = {.x = 0, .y = 0},
        .maxExclusive = {.x = dimensions.widthTiles / landBlockDimension, .y = dimensions.heightTiles / landBlockDimension},
    };
    auto compared = source->readRawLandBlocks(fullRegion,
                                              [&state](BlockCoord coordinate, const LandBlockBytes & sourceRaw)
                                              { return appendChangedDiffBlock(state, coordinate, sourceRaw); });
    static_cast<void>(::close(targetDescriptor));
    data.close();
    index.close();
    closeDiffDescriptors(files);

    if (!compared)
    {
        removeDiffTemporaries(files);
        return std::unexpected(compared.error());
    }
    if (!data || !index)
    {
        const auto error = makeError(ErrorCode::io, files.outputs.data.parent_path(), 0, "could not write diff files");
        removeDiffTemporaries(files);
        return std::unexpected(error);
    }
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

[[nodiscard]] std::expected<DiffBackups, Error> backupDiffOutputs(const DiffOutputPaths & outputs, DiffTemporaryFiles & files)
{
    DiffBackups backups{};
    if (std::filesystem::exists(outputs.data))
    {
        backups.data = temporaryPath(outputs.data);
        if (::rename(outputs.data.c_str(), backups.data.c_str()) != 0)
        {
            const auto error = makeError(ErrorCode::commitFailed, outputs.data, 0, std::strerror(errno));
            removeDiffTemporaries(files);
            return std::unexpected(error);
        }
    }
    if (std::filesystem::exists(outputs.index))
    {
        backups.index = temporaryPath(outputs.index);
        if (::rename(outputs.index.c_str(), backups.index.c_str()) != 0)
        {
            const auto error = makeError(ErrorCode::commitFailed, outputs.index, 0, std::strerror(errno));
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

[[nodiscard]] std::expected<DiffReport, Error>
publishDiffWithoutOverwrite(const DiffOutputPaths & outputs, DiffTemporaryFiles & files, DiffReport report)
{
    if (::link(files.data.c_str(), outputs.data.c_str()) != 0)
    {
        const auto error = makeError(ErrorCode::commitFailed, outputs.data, 0, std::strerror(errno));
        removeDiffTemporaries(files);
        return std::unexpected(error);
    }
    static_cast<void>(::unlink(files.data.c_str()));
    if (::link(files.index.c_str(), outputs.index.c_str()) != 0)
    {
        const int savedErrno = errno;
        static_cast<void>(::unlink(outputs.data.c_str()));
        removeDiffTemporaries(files);
        return std::unexpected(makeError(ErrorCode::commitFailed, outputs.index, 0, std::strerror(savedErrno)));
    }
    static_cast<void>(::unlink(files.index.c_str()));
    return report;
}

[[nodiscard]] std::expected<DiffReport, Error>
publishDiffWithOverwrite(const DiffOutputPaths & outputs, DiffTemporaryFiles & files, DiffReport report)
{
    auto backups = backupDiffOutputs(outputs, files);
    if (!backups)
    {
        return std::unexpected(backups.error());
    }
    if (::link(files.data.c_str(), outputs.data.c_str()) != 0)
    {
        const auto error = makeError(ErrorCode::commitFailed, outputs.data, 0, std::strerror(errno));
        restoreDiffBackups(*backups, outputs);
        removeDiffTemporaries(files);
        return std::unexpected(error);
    }
    static_cast<void>(::unlink(files.data.c_str()));
    if (::link(files.index.c_str(), outputs.index.c_str()) != 0)
    {
        const int savedErrno = errno;
        static_cast<void>(::unlink(outputs.data.c_str()));
        restoreDiffBackups(*backups, outputs);
        removeDiffTemporaries(files);
        return std::unexpected(makeError(ErrorCode::commitFailed, outputs.index, 0, std::strerror(savedErrno)));
    }
    static_cast<void>(::unlink(files.index.c_str()));
    removeDiffBackups(*backups);
    return report;
}

[[nodiscard]] std::expected<DiffReport, Error>
publishDiffFiles(const DiffOutputPaths & outputs, DiffTemporaryFiles & files, bool overwrite, DiffReport report)
{
    if (overwrite)
    {
        return publishDiffWithOverwrite(outputs, files, report);
    }
    return publishDiffWithoutOverwrite(outputs, files, report);
}

} // namespace

std::expected<DiffReport, Error> MapDiff::generate(const std::filesystem::path & baseMap,
                                                   const std::filesystem::path & targetMap,
                                                   const std::filesystem::path & diffDirectory,
                                                   std::uint32_t mapIndex,
                                                   Dimensions dimensions,
                                                   bool overwrite)
{
    auto filesResult = prepareDiffTemporaries(diffDirectory, mapIndex, overwrite);
    if (!filesResult)
    {
        return std::unexpected(filesResult.error());
    }
    auto files = std::move(*filesResult);

    auto report = scanDiffTemporaries(baseMap, targetMap, dimensions, files);
    if (!report)
    {
        return std::unexpected(report.error());
    }
    return publishDiffFiles(files.outputs, files, overwrite, *report);
}

namespace
{

struct DiffLengths
{
    std::uint64_t blockCount = 0;
};

[[nodiscard]] std::expected<DiffLengths, Error> validateDiffLengths(std::istream & data,
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
        return std::unexpected(makeError(ErrorCode::io, diffDirectory, 0, "could not determine diff file sizes"));
    }

    const auto dataBytes = static_cast<std::uint64_t>(dataLength);
    const auto indexBytes = static_cast<std::uint64_t>(indexLength);
    if (indexBytes % mapBlockIdByteCount != 0 || dataBytes % landBlockByteSize != 0 ||
        dataBytes / landBlockByteSize != indexBytes / mapBlockIdByteCount)
    {
        return std::unexpected(
            makeError(ErrorCode::truncatedInput, dataPath, 0, "diff data and index lengths are inconsistent"));
    }

    data.seekg(0);
    index.seekg(0);
    return DiffLengths{.blockCount = indexBytes / mapBlockIdByteCount};
}

[[nodiscard]] std::expected<std::vector<std::pair<BlockCoord, LandBlock>>, Error>
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
    LandBlockBytes raw{};
    std::uint32_t id = 0;
    while (readLe32(index, id))
    {
        if (static_cast<std::uint64_t>(id) >= expectedBlockCount || seen.at(id))
        {
            return std::unexpected(makeError(ErrorCode::invalidBlockCount,
                                             indexPath,
                                             static_cast<std::uint64_t>(index.tellg()) - mapBlockIdByteCount,
                                             "diff contains an out-of-range or duplicate block ID"));
        }
        seen.at(id) = true;

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
            return std::unexpected(makeError(
                ErrorCode::truncatedInput, dataPath, static_cast<std::uint64_t>(data.tellg()), "diff block is truncated"));
        }

        const auto blockHeight = dimensions.heightTiles / landBlockDimension;
        const BlockCoord coordinate{.x = id / blockHeight, .y = id % blockHeight};
        replacements.emplace_back(coordinate, decodeBlock(raw));
    }
    if (!index.eof())
    {
        return std::unexpected(makeError(ErrorCode::io, indexPath, 0, "failed while reading diff index"));
    }
    return replacements;
}

[[nodiscard]] std::expected<void, Error>
writeDiffReplacements(MapEditor & editor, const std::vector<std::pair<BlockCoord, LandBlock>> & replacements)
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

} // namespace

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
        return std::unexpected(makeError(ErrorCode::io, diffDirectory, 0, "both map diff files are required"));
    }

    std::ifstream data(dataPath, std::ios::binary);
    std::ifstream index(indexPath, std::ios::binary);
    if (!data || !index)
    {
        return std::unexpected(makeError(ErrorCode::io, diffDirectory, 0, "could not open diff files"));
    }

    auto lengths = validateDiffLengths(data, index, dataPath, diffDirectory);
    if (!lengths)
    {
        return std::unexpected(lengths.error());
    }
    const auto expectedBlockCount = blockCount(dimensions, baseMap);

    if (!expectedBlockCount)
    {
        return std::unexpected(expectedBlockCount.error());
    }

    if (*expectedBlockCount > std::numeric_limits<std::uint32_t>::max())
    {
        return std::unexpected(
            makeError(ErrorCode::invalidDimensions, baseMap, 0, "map has more blocks than diff indices can address"));
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

    return std::unexpected(makeError(ErrorCode::invalidDimensions, {}, 0, "unknown facet preset"));
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
