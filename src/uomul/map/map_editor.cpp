#include "map_helper.h"
#include "uomul/map.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <fcntl.h>
#include <filesystem>
#include <iterator>
#include <memory>
#include <span>
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

constexpr std::size_t bufferSize = 1'048'576U;

std::expected<void, Error> ensureEditorActive(bool committed, int descriptor, bool failed, const std::filesystem::path & output)
{
    if (committed || descriptor < 0 || failed)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::commitFailed, output, 0, "editor is no longer active"));
    }
    return {};
}

template <typename Region, typename Callback>
std::expected<void, Error> validateEditRequest(bool committed,
                                               int descriptor,
                                               bool failed,
                                               const std::filesystem::path & output,
                                               const std::filesystem::path & source,
                                               Region region,
                                               const Callback & callback,
                                               Dimensions dimensions)
{
    if (auto active = ensureEditorActive(committed, descriptor, failed, output); !active)
    {
        return active;
    }
    if (!callback)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, source, 0, "edit callback is empty"));
    }
    return MapHelper::validateRegion(region, dimensions, source);
}

} // namespace

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
        return std::unexpected(MapHelper::makeError(ErrorCode::io, sourceMap, 0, pathError.message()));
    }
    const auto outputAbsolute = std::filesystem::absolute(outputMap, pathError).lexically_normal();
    if (pathError)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, outputMap, 0, pathError.message()));
    }
    if (sourceAbsolute == outputAbsolute)
    {
        return std::unexpected(MapHelper::makeError(
            ErrorCode::io, outputMap, 0, "in-place editing is not supported; choose a separate output path"));
    }
    pathError.clear();
    const bool outputExists = std::filesystem::exists(outputMap, pathError);
    if (pathError)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, outputMap, 0, pathError.message()));
    }
    if (outputExists && std::filesystem::equivalent(sourceMap, outputMap, pathError))
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, outputMap, 0, "output path resolves to the source map"));
    }
    if (pathError)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, outputMap, 0, pathError.message()));
    }
    if (auto check = MapHelper::checkOutputPath(outputMap, options.overwriteOutput); !check)
    {
        return std::unexpected(check.error());
    }

    const auto parent = outputMap.parent_path();
    std::error_code directoryError;
    const bool parentExists = parent.empty() || std::filesystem::is_directory(parent, directoryError);
    if (directoryError || !parentExists)
    {
        return std::unexpected(MapHelper::makeError(
            ErrorCode::io, parent, 0, directoryError ? directoryError.message() : "output directory does not exist"));
    }

    auto impl = std::make_unique<Impl>();
    impl->source = sourceMap;
    impl->output = outputMap;
    impl->dimensions = dimensions;
    impl->overwrite = options.overwriteOutput;
    auto temporaryDescriptor = MapHelper::createTemporary(outputMap, impl->staging);
    if (!temporaryDescriptor)
    {
        return std::unexpected(temporaryDescriptor.error());
    }
    detail::TemporaryPath stagingCleanup(impl->staging);
    detail::ScopedDescriptor stagedDescriptor(*temporaryDescriptor);
    const detail::ScopedDescriptor sourceDescriptor(::open(sourceMap.c_str(), O_RDONLY | O_CLOEXEC));
    if (sourceDescriptor.get() < 0)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, sourceMap, 0, std::strerror(errno)));
    }
    std::array<std::byte, bufferSize> buffer{};
    std::uint64_t offset = 0;
    const auto totalBytes = source->completeBlocks() * MapHelper::landBlockByteSize;
    struct stat sourceStatus{};
    if (::fstat(sourceDescriptor.get(), &sourceStatus) != 0 || !S_ISREG(sourceStatus.st_mode))
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, sourceMap, 0, "map input must be a regular file"));
    }
    while (offset < totalBytes)
    {
        const auto amount = static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(), totalBytes - offset));
        if (!MapHelper::readExact(sourceDescriptor.get(), buffer.data(), amount, offset) ||
            !MapHelper::writeExact(stagedDescriptor.get(), buffer.data(), amount, offset))
        {
            return std::unexpected(MapHelper::makeError(ErrorCode::io, impl->staging, offset, "failed to stage source map"));
        }
        offset += amount;
    }

    impl->descriptor = stagedDescriptor.release();
    stagingCleanup.release();
    return MapEditor(std::move(impl));
}

std::expected<void, Error> MapEditor::writeLandBlock(BlockCoord block, const LandBlock & data)
{
    if (auto active = ensureEditorActive(impl_->committed, impl_->descriptor, impl_->failed, impl_->output); !active)
    {
        return active;
    }
    if (auto valid = MapHelper::validateBlock(block, impl_->dimensions, impl_->source); !valid)
    {
        return valid;
    }

    LandBlockBytes raw{};
    const auto offset = MapHelper::blockOffset(block, impl_->dimensions);
    if (!MapHelper::readExact(impl_->descriptor, raw.data(), raw.size(), offset))
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, impl_->staging, offset, "could not read staged block"));
    }
    MapHelper::encodeBlock(raw, data);
    if (!MapHelper::writeExact(impl_->descriptor, raw.data(), raw.size(), offset))
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, impl_->staging, offset, "could not stage land block"));
    }

    return {};
}

std::expected<void, Error> MapEditor::writeLandTile(TileCoord tile, LandTile data)
{
    if (auto active = ensureEditorActive(impl_->committed, impl_->descriptor, impl_->failed, impl_->output); !active)
    {
        return active;
    }
    if (auto valid = MapHelper::validateTile(tile, impl_->dimensions, impl_->source); !valid)
    {
        return valid;
    }

    const BlockCoord coordinate{.x = tile.x / landBlockDimension, .y = tile.y / landBlockDimension};
    LandBlockBytes raw{};
    const auto offset = MapHelper::blockOffset(coordinate, impl_->dimensions);
    if (!MapHelper::readExact(impl_->descriptor, raw.data(), raw.size(), offset))
    {
        return std::unexpected(
            MapHelper::makeError(ErrorCode::truncatedInput, impl_->staging, offset, "could not read complete land block"));
    }
    const auto tileOffset = static_cast<std::size_t>(MapHelper::landHeaderByteSize) +
                            (detail::tileIndexInBlock(tile) * static_cast<std::size_t>(MapHelper::landTileByteSize));
    raw.at(tileOffset) = static_cast<std::byte>(data.id & MapHelper::byteMask);
    raw.at(tileOffset + 1) = static_cast<std::byte>(data.id >> MapHelper::bitsPerByte);
    raw.at(tileOffset + 2) = static_cast<std::byte>(static_cast<std::uint8_t>(data.z));
    if (!MapHelper::writeExact(impl_->descriptor, raw.data(), raw.size(), offset))
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, impl_->staging, offset, "could not stage land tile"));
    }
    return {};
}

namespace
{

constexpr std::uint64_t journalHeaderBytes = sizeof(std::uint64_t);
constexpr std::size_t journalEntryBytes = static_cast<std::size_t>(journalHeaderBytes + (2 * MapHelper::landBlockByteSize));

std::uint64_t journalOffset(std::span<const std::byte, journalEntryBytes> entry)
{
    std::array<std::byte, sizeof(std::uint64_t)> encoded{};
    std::ranges::copy_n(entry.begin(), encoded.size(), encoded.begin());
    return detail::decodeLittleEndian<std::uint64_t>(encoded);
}

std::expected<std::uint64_t, Error> writeEditJournal(int stagedDescriptor,
                                                     const std::filesystem::path & staging,
                                                     int journal,
                                                     const std::filesystem::path & journalPath,
                                                     Dimensions dimensions,
                                                     BlockRegion region,
                                                     const LandBlockEdit & callback)
{
    constexpr std::size_t entriesPerWrite = 4096;
    std::vector<std::byte> buffer(entriesPerWrite * static_cast<std::size_t>(journalEntryBytes));
    std::size_t bufferedEntries = 0;
    std::uint64_t entryCount = 0;
    for (std::uint32_t x = region.min.x; x < region.maxExclusive.x; ++x)
    {
        for (std::uint32_t y = region.min.y; y < region.maxExclusive.y; ++y)
        {
            const BlockCoord coordinate{.x = x, .y = y};
            const auto offset = MapHelper::blockOffset(coordinate, dimensions);
            LandBlockBytes before{};
            if (!MapHelper::readExact(stagedDescriptor, before.data(), before.size(), offset))
            {
                return std::unexpected(MapHelper::makeError(ErrorCode::io, staging, offset, "could not read staged block"));
            }
            auto changed = callback(coordinate, MapHelper::decodeBlock(before));
            if (!changed)
            {
                return std::unexpected(changed.error());
            }

            LandBlockBytes after = before;
            MapHelper::encodeBlock(after, *changed);
            std::array<std::byte, journalEntryBytes> entry{};
            const auto encodedOffset = detail::encodeLittleEndian(offset);
            std::ranges::copy(encodedOffset, entry.begin());
            const auto beforeOffset = static_cast<std::size_t>(journalHeaderBytes);
            const auto afterOffset = beforeOffset + static_cast<std::size_t>(MapHelper::landBlockByteSize);
            std::ranges::copy(before, std::next(entry.begin(), static_cast<std::ptrdiff_t>(beforeOffset)));
            std::ranges::copy(after, std::next(entry.begin(), static_cast<std::ptrdiff_t>(afterOffset)));
            const auto bufferOffset = bufferedEntries * static_cast<std::size_t>(journalEntryBytes);
            std::ranges::copy(entry, std::next(buffer.begin(), static_cast<std::ptrdiff_t>(bufferOffset)));
            ++bufferedEntries;
            ++entryCount;
            if (bufferedEntries == entriesPerWrite)
            {
                const auto journalOffset = (entryCount - bufferedEntries) * journalEntryBytes;
                const auto byteCount = bufferedEntries * static_cast<std::size_t>(journalEntryBytes);
                if (!MapHelper::writeExact(journal, buffer.data(), byteCount, journalOffset))
                {
                    return std::unexpected(
                        MapHelper::makeError(ErrorCode::io, journalPath, journalOffset, "could not write edit journal"));
                }
                bufferedEntries = 0;
            }
        }
    }
    if (bufferedEntries != 0)
    {
        const auto journalOffset = (entryCount - bufferedEntries) * journalEntryBytes;
        const auto byteCount = bufferedEntries * static_cast<std::size_t>(journalEntryBytes);
        if (!MapHelper::writeExact(journal, buffer.data(), byteCount, journalOffset))
        {
            return std::unexpected(
                MapHelper::makeError(ErrorCode::io, journalPath, journalOffset, "could not write edit journal"));
        }
    }

    return entryCount;
}

bool transferJournalBlocks(int stagedDescriptor,
                           int journal,
                           std::uint64_t entryCount,
                           std::uint64_t & attempted,
                           std::size_t dataOffset)
{
    constexpr std::size_t entriesPerRead = 1024;
    constexpr std::size_t blockBytes = static_cast<std::size_t>(MapHelper::landBlockByteSize);
    std::vector<std::byte> entries(entriesPerRead * static_cast<std::size_t>(journalEntryBytes));
    std::vector<std::byte> blocks(entriesPerRead * blockBytes);
    attempted = 0;
    while (attempted < entryCount)
    {
        const auto chunkStart = attempted;
        const auto entryCountInChunk = static_cast<std::size_t>(std::min<std::uint64_t>(entriesPerRead, entryCount - chunkStart));
        const auto entriesBytes = entryCountInChunk * static_cast<std::size_t>(journalEntryBytes);
        if (!MapHelper::readExact(journal, entries.data(), entriesBytes, chunkStart * journalEntryBytes))
        {
            return false;
        }

        std::size_t runStart = 0;
        while (runStart < entryCountInChunk)
        {
            const auto firstEntryOffset = runStart * static_cast<std::size_t>(journalEntryBytes);
            const std::span<const std::byte, journalEntryBytes> firstEntry(entries.data() + firstEntryOffset,
                                                                            journalEntryBytes);
            const auto blockOffset = journalOffset(firstEntry);
            std::size_t runEnd = runStart + 1;
            while (runEnd < entryCountInChunk)
            {
                const auto entryOffset = runEnd * static_cast<std::size_t>(journalEntryBytes);
                const std::span<const std::byte, journalEntryBytes> entry(entries.data() + entryOffset, journalEntryBytes);
                if (journalOffset(entry) != blockOffset + ((runEnd - runStart) * MapHelper::landBlockByteSize))
                {
                    break;
                }
                ++runEnd;
            }

            for (std::size_t index = runStart; index < runEnd; ++index)
            {
                const auto entryOffset = index * static_cast<std::size_t>(journalEntryBytes);
                const auto sourceOffset = entryOffset + dataOffset;
                const auto destinationOffset = (index - runStart) * blockBytes;
                std::ranges::copy_n(std::next(entries.begin(), static_cast<std::ptrdiff_t>(sourceOffset)),
                                    blockBytes,
                                    std::next(blocks.begin(), static_cast<std::ptrdiff_t>(destinationOffset)));
            }
            attempted = chunkStart + runEnd;
            const auto runBytes = (runEnd - runStart) * blockBytes;
            if (!MapHelper::writeExact(stagedDescriptor, blocks.data(), runBytes, blockOffset))
            {
                return false;
            }
            runStart = runEnd;
        }
    }
    return true;
}

bool replayEditJournal(int stagedDescriptor, int journal, std::uint64_t entryCount, std::uint64_t & attempted)
{
    return transferJournalBlocks(stagedDescriptor,
                                 journal,
                                 entryCount,
                                 attempted,
                                 static_cast<std::size_t>(journalHeaderBytes + MapHelper::landBlockByteSize));
}

bool rollbackEditJournal(int stagedDescriptor, int journal, std::uint64_t entryCount)
{
    std::uint64_t attempted = 0;
    bool rollbackSucceeded = true;
    constexpr std::size_t entriesPerRead = 1024;
    constexpr std::size_t blockBytes = static_cast<std::size_t>(MapHelper::landBlockByteSize);
    const auto beforeOffset = static_cast<std::size_t>(journalHeaderBytes);
    std::vector<std::byte> entries(entriesPerRead * static_cast<std::size_t>(journalEntryBytes));
    std::vector<std::byte> blocks(entriesPerRead * blockBytes);
    while (attempted < entryCount)
    {
        const auto chunkStart = attempted;
        const auto entryCountInChunk = static_cast<std::size_t>(std::min<std::uint64_t>(entriesPerRead, entryCount - chunkStart));
        const auto entriesBytes = entryCountInChunk * static_cast<std::size_t>(journalEntryBytes);
        if (!MapHelper::readExact(journal, entries.data(), entriesBytes, chunkStart * journalEntryBytes))
        {
            rollbackSucceeded = false;
            attempted += entryCountInChunk;
            continue;
        }

        std::size_t runStart = 0;
        while (runStart < entryCountInChunk)
        {
            const auto firstEntryOffset = runStart * static_cast<std::size_t>(journalEntryBytes);
            const std::span<const std::byte, journalEntryBytes> firstEntry(entries.data() + firstEntryOffset,
                                                                            journalEntryBytes);
            const auto blockOffset = journalOffset(firstEntry);
            std::size_t runEnd = runStart + 1;
            while (runEnd < entryCountInChunk)
            {
                const auto entryOffset = runEnd * static_cast<std::size_t>(journalEntryBytes);
                const std::span<const std::byte, journalEntryBytes> entry(entries.data() + entryOffset, journalEntryBytes);
                if (journalOffset(entry) != blockOffset + ((runEnd - runStart) * MapHelper::landBlockByteSize))
                {
                    break;
                }
                ++runEnd;
            }

            for (std::size_t index = runStart; index < runEnd; ++index)
            {
                const auto entryOffset = index * static_cast<std::size_t>(journalEntryBytes);
                const auto sourceOffset = entryOffset + beforeOffset;
                const auto destinationOffset = (index - runStart) * blockBytes;
                std::ranges::copy_n(std::next(entries.begin(), static_cast<std::ptrdiff_t>(sourceOffset)),
                                    blockBytes,
                                    std::next(blocks.begin(), static_cast<std::ptrdiff_t>(destinationOffset)));
            }
            const auto runBytes = (runEnd - runStart) * blockBytes;
            if (!MapHelper::writeExact(stagedDescriptor, blocks.data(), runBytes, blockOffset))
            {
                rollbackSucceeded = false;
            }
            attempted = chunkStart + runEnd;
            runStart = runEnd;
        }
    }

    return rollbackSucceeded;
}

std::expected<void, Error> applyJournaledEdit(int stagedDescriptor,
                                              const std::filesystem::path & staging,
                                              const std::filesystem::path & output,
                                              Dimensions dimensions,
                                              BlockRegion region,
                                              const LandBlockEdit & callback,
                                              bool & failed)
{
    std::filesystem::path journalPath;
    auto journalDescriptor = MapHelper::createTemporary(output, journalPath);
    if (!journalDescriptor)
    {
        return std::unexpected(journalDescriptor.error());
    }
    const detail::TemporaryPath journalCleanup(journalPath);
    const detail::ScopedDescriptor journal(*journalDescriptor);
    auto entryCount = writeEditJournal(stagedDescriptor, staging, journal.get(), journalPath, dimensions, region, callback);
    if (!entryCount)
    {
        return std::unexpected(entryCount.error());
    }

    std::uint64_t attempted = 0;
    if (!replayEditJournal(stagedDescriptor, journal.get(), *entryCount, attempted))
    {
        const bool rollbackSucceeded = rollbackEditJournal(stagedDescriptor, journal.get(), attempted);
        if (!rollbackSucceeded)
        {
            failed = true;
        }
        return std::unexpected(MapHelper::makeError(ErrorCode::io,
                                                    staging,
                                                    0,
                                                    rollbackSucceeded ? "edit journal failed; staged changes were rolled back"
                                                                      : "edit failed and rollback was incomplete"));
    }

    return {};
}

} // namespace

std::expected<void, Error> MapEditor::editLandBlocks(BlockRegion region, const LandBlockEdit & callback)
{
    if (auto valid = validateEditRequest(impl_->committed,
                                         impl_->descriptor,
                                         impl_->failed,
                                         impl_->output,
                                         impl_->source,
                                         region,
                                         callback,
                                         impl_->dimensions);
        !valid)
    {
        return valid;
    }

    return applyJournaledEdit(
        impl_->descriptor, impl_->staging, impl_->output, impl_->dimensions, region, callback, impl_->failed);
}

std::expected<void, Error> MapEditor::editLandTiles(TileRegion region, const LandTileEdit & callback)
{
    if (auto valid = validateEditRequest(impl_->committed,
                                         impl_->descriptor,
                                         impl_->failed,
                                         impl_->output,
                                         impl_->source,
                                         region,
                                         callback,
                                         impl_->dimensions);
        !valid)
    {
        return valid;
    }

    const auto blocks = detail::blockRegionForTiles(region);
    return applyJournaledEdit(
        impl_->descriptor,
        impl_->staging,
        impl_->output,
        impl_->dimensions,
        blocks,
        [&region, &callback](BlockCoord coordinate, const LandBlock & current) -> std::expected<LandBlock, Error>
        {
            LandBlock changed = current;
            auto visited = detail::visitTilesInBlock(
                region,
                coordinate,
                [&callback, &changed, &current](TileCoord tile, std::size_t index) -> std::expected<void, Error>
                {
                    auto result = callback(tile, current.at(index));
                    if (!result)
                    {
                        return std::unexpected(result.error());
                    }
                    changed.at(index) = *result;
                    return {};
                });
            if (!visited)
            {
                return std::unexpected(visited.error());
            }
            return changed;
        },
        impl_->failed);
}

std::expected<void, Error> MapEditor::fill(LandTile value)
{
    const BlockRegion region{
        .min =
            {
                .x = 0,
                .y = 0,
            },
        .maxExclusive =
            {
                .x = impl_->dimensions.widthTiles / landBlockDimension,
                .y = impl_->dimensions.heightTiles / landBlockDimension,
            },
    };
    return fillBlocks(region, value);
}

std::expected<void, Error> MapEditor::fillBlocks(BlockRegion region, LandTile value)
{
    return editLandBlocks(region,
                          [value](BlockCoord, const LandBlock &) -> std::expected<LandBlock, Error>
                          {
                              LandBlock result{};
                              result.fill(value);
                              return result;
                          });
}

std::expected<void, Error> MapEditor::fillTiles(TileRegion region, LandTile value)
{
    return editLandTiles(region, [value](TileCoord, const LandTile &) -> std::expected<LandTile, Error> { return value; });
}

std::expected<void, Error> MapEditor::replaceIds(std::uint16_t from, std::uint16_t to)
{
    const BlockRegion region{
        .min =
            {
                .x = 0,
                .y = 0,
            },
        .maxExclusive =
            {
                .x = impl_->dimensions.widthTiles / landBlockDimension,
                .y = impl_->dimensions.heightTiles / landBlockDimension,
            },
    };
    return replaceBlockIds(region, from, to);
}

std::expected<void, Error> MapEditor::replaceZ(std::int8_t from, std::int8_t to)
{
    const BlockRegion region{
        .min =
            {
                .x = 0,
                .y = 0,
            },
        .maxExclusive =
            {
                .x = impl_->dimensions.widthTiles / landBlockDimension,
                .y = impl_->dimensions.heightTiles / landBlockDimension,
            },
    };
    return replaceBlockZ(region, from, to);
}

std::expected<void, Error> MapEditor::replaceTileIds(TileRegion region, std::uint16_t from, std::uint16_t to)
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

std::expected<void, Error> MapEditor::replaceBlockIds(BlockRegion region, std::uint16_t from, std::uint16_t to)
{
    return editLandBlocks(region,
                          [from, to](BlockCoord, const LandBlock & current) -> std::expected<LandBlock, Error>
                          {
                              LandBlock result = current;
                              for (auto & tile : result)
                              {
                                  if (tile.id == from)
                                  {
                                      tile.id = to;
                                  }
                              }
                              return result;
                          });
}

std::expected<void, Error> MapEditor::replaceTileZ(TileRegion region, std::int8_t from, std::int8_t to)
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

std::expected<void, Error> MapEditor::replaceBlockZ(BlockRegion region, std::int8_t from, std::int8_t to)
{
    return editLandBlocks(region,
                          [from, to](BlockCoord, const LandBlock & current) -> std::expected<LandBlock, Error>
                          {
                              LandBlock result = current;
                              for (auto & tile : result)
                              {
                                  if (tile.z == from)
                                  {
                                      tile.z = to;
                                  }
                              }
                              return result;
                          });
}

std::expected<void, Error> MapEditor::commit()
{
    if (impl_->committed || impl_->descriptor < 0)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::commitFailed, impl_->output, 0, "editor is no longer active"));
    }
    if (::fsync(impl_->descriptor) != 0)
    {
        const auto error = MapHelper::makeError(ErrorCode::commitFailed, impl_->staging, 0, std::strerror(errno));
        static_cast<void>(::close(impl_->descriptor));
        impl_->descriptor = -1;
        static_cast<void>(::unlink(impl_->staging.c_str()));
        impl_->committed = true;
        return std::unexpected(error);
    }
    if (::close(impl_->descriptor) != 0)
    {
        const auto error = MapHelper::makeError(ErrorCode::commitFailed, impl_->staging, 0, std::strerror(errno));
        impl_->descriptor = -1;
        static_cast<void>(::unlink(impl_->staging.c_str()));
        impl_->committed = true;
        return std::unexpected(error);
    }
    impl_->descriptor = -1;

    auto result = MapHelper::publishFile(impl_->staging, impl_->output, impl_->overwrite);
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

} // namespace uomul
