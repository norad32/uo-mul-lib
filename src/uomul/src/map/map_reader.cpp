#include "map_common.h"
#include "map_stats.h"
#include "uomul/map.h"
#include "uomul/mul_common/mul_common.h"
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
#include <unistd.h>
#include <utility>
#include <vector>

namespace uomul
{

using detail::MapCommon;

namespace
{

struct MapFileInfo
{
    std::uint64_t completeBlocks;
    std::uint64_t missingBlocks;
    std::uint64_t ignoredBytes;
};

[[nodiscard]] inline bool hasInvalidPartialBlock(std::uint64_t fileSize, std::uint64_t expectedSize, OpenOptions options)
{
    const bool hasTrailingPartialBlock = fileSize % MapCommon::mapBlockByteSize != 0;
    const bool partialBlockIsNotRecoverable = fileSize > expectedSize || !options.recoverTruncatedMap;
    return hasTrailingPartialBlock && partialBlockIsNotRecoverable;
}

[[nodiscard]] inline bool
readCompleteBlock(int descriptor, std::uint64_t completeBlocks, BlockCoord block, Dimensions dimensions, MapBlockBytes & raw)
{
    if (MapCommon::blockId(block, dimensions) >= completeBlocks)
    {
        return false;
    }

    const auto offset = MapCommon::blockOffset(block, dimensions);
    return detail::MulCommon::readExact(descriptor, raw.data(), raw.size(), offset);
}

[[nodiscard]] inline bool readCompleteBlockBatch(int descriptor,
                                                 std::uint64_t completeBlocks,
                                                 BlockCoord first,
                                                 std::size_t blockCount,
                                                 Dimensions dimensions,
                                                 std::vector<std::byte> & buffer)
{
    const auto firstBlockId = MapCommon::blockId(first, dimensions);
    const bool allBlocksAvailable = firstBlockId + blockCount <= completeBlocks;
    if (!allBlocksAvailable)
    {
        return false;
    }

    return MapCommon::readBlockBatch(descriptor, first, blockCount, dimensions, buffer);
}

std::expected<void, Error>
validateMapFileSize(const std::filesystem::path & path, std::uint64_t fileSize, std::uint64_t expectedSize, OpenOptions options)
{
    if (hasInvalidPartialBlock(fileSize, expectedSize, options))
    {
        return std::unexpected(detail::MulCommon::makeError(
            ErrorCode::truncatedInput, path, fileSize, "map has trailing bytes beyond complete blocks"));
    }

    if (!options.recoverTruncatedMap && fileSize != expectedSize)
    {
        return std::unexpected(detail::MulCommon::makeError(
            ErrorCode::truncatedInput, path, fileSize, "map size does not match dimensions and complete blocks"));
    }

    if (fileSize > expectedSize)
    {
        return std::unexpected(detail::MulCommon::makeError(
            ErrorCode::truncatedInput, path, expectedSize, "map contains more blocks than specified dimensions"));
    }

    return {};
}

std::expected<MapFileInfo, Error>
inspectMapFile(int descriptor, const std::filesystem::path & path, std::uint64_t expectedBlockCount, OpenOptions options)
{
    auto inspectedSize = detail::MulCommon::regularFileSize(descriptor, path, "map");
    if (!inspectedSize)
    {
        return std::unexpected(inspectedSize.error());
    }
    const auto fileSize = *inspectedSize;
    const auto expectedSize = expectedBlockCount * MapCommon::mapBlockByteSize;
    if (auto validSize = validateMapFileSize(path, fileSize, expectedSize, options); !validSize)
    {
        return std::unexpected(validSize.error());
    }

    const auto completeBlockCount = std::min(expectedBlockCount, fileSize / MapCommon::mapBlockByteSize);
    return MapFileInfo{
        .completeBlocks = completeBlockCount,
        .missingBlocks = expectedBlockCount - completeBlockCount,
        .ignoredBytes = fileSize % MapCommon::mapBlockByteSize,
    };
}

std::expected<void, Error> readRawBlock(int descriptor,
                                        std::uint64_t completeBlocks,
                                        BlockCoord block,
                                        Dimensions dimensions,
                                        const std::filesystem::path & path,
                                        MapBlockBytes & raw)
{
    const auto offset = MapCommon::blockOffset(block, dimensions);
    if (!readCompleteBlock(descriptor, completeBlocks, block, dimensions, raw))
    {
        return std::unexpected(
            detail::MulCommon::makeError(ErrorCode::truncatedInput, path, offset, "land block is missing or truncated"));
    }

    return {};
}

std::expected<std::uint64_t, Error> readableBlockOffset(int descriptor,
                                                        std::uint64_t completeBlocks,
                                                        BlockCoord block,
                                                        Dimensions dimensions,
                                                        const std::filesystem::path & path)
{
    const auto offset = MapCommon::blockOffset(block, dimensions);
    const auto missingBlock = [&path, offset]
    {
        return std::unexpected(
            detail::MulCommon::makeError(ErrorCode::truncatedInput, path, offset, "land block is missing or truncated"));
    };

    if (MapCommon::blockId(block, dimensions) >= completeBlocks)
    {
        return missingBlock();
    }

    auto fileSize = detail::MulCommon::regularFileSize(descriptor, path, "map");
    if (!fileSize)
    {
        return std::unexpected(fileSize.error());
    }

    if (*fileSize < offset + MapCommon::mapBlockByteSize)
    {
        return missingBlock();
    }
    return offset;
}

template <typename Callback>
std::expected<void, Error> readRawBlockBatch(int descriptor,
                                             std::uint64_t completeBlocks,
                                             BlockCoord first,
                                             std::size_t blockCount,
                                             Dimensions dimensions,
                                             const std::filesystem::path & path,
                                             std::vector<std::byte> & buffer,
                                             const Callback & callback)
{
    const bool batchLoaded = readCompleteBlockBatch(descriptor, completeBlocks, first, blockCount, dimensions, buffer);
    if (!batchLoaded)
    {
        for (std::size_t index = 0; index < blockCount; index++)
        {
            const BlockCoord coordinate{.x = first.x, .y = first.y + static_cast<std::uint32_t>(index)};
            MapBlockBytes raw{};
            if (auto loaded = readRawBlock(descriptor, completeBlocks, coordinate, dimensions, path, raw); !loaded)
            {
                return std::unexpected(loaded.error());
            }

            if (auto result = callback(coordinate, raw); !result)
            {
                return result;
            }
        }

        return {};
    }

    for (std::size_t index = 0; index < blockCount; index++)
    {
        MapBlockBytes raw{};
        const auto blockOffset = index * static_cast<std::size_t>(MapCommon::mapBlockByteSize);
        std::ranges::copy_n(std::next(buffer.begin(), static_cast<std::ptrdiff_t>(blockOffset)), raw.size(), raw.begin());
        const BlockCoord coordinate{.x = first.x, .y = first.y + static_cast<std::uint32_t>(index)};

        if (auto result = callback(coordinate, raw); !result)
        {
            return result;
        }
    }

    return {};
}

template <typename Region, typename Callback>
std::expected<void, Error>
validateReadRequest(Region region, const Callback & callback, Dimensions dimensions, const std::filesystem::path & path)
{
    if (auto valid = MapCommon::validateRegion(region, dimensions, path); !valid)
    {
        return valid;
    }

    if (!callback)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, path, 0, "read callback is empty"));
    }

    return {};
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
    auto count = MapCommon::blockCount(dimensions, path);
    if (!count)
    {
        return std::unexpected(count.error());
    }

    detail::ScopedDescriptor descriptor(::open(path.c_str(), O_RDONLY | O_CLOEXEC));
    if (descriptor.get() < 0)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, path, 0, std::strerror(errno)));
    }

    auto fileInfo = inspectMapFile(descriptor.get(), path, *count, options);
    if (!fileInfo)
    {
        return std::unexpected(fileInfo.error());
    }

    auto impl = std::make_unique<Impl>();
    impl->path = path;
    impl->dimensions = dimensions;
    impl->descriptor = descriptor.release();
    impl->blocks = fileInfo->completeBlocks;
    impl->missingBlocks = fileInfo->missingBlocks;
    impl->ignoredBytes = fileInfo->ignoredBytes;
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

std::expected<MapBlock, Error> MapReader::readBlock(BlockCoord block) const
{
    if (auto valid = MapCommon::validateBlock(block, impl_->dimensions, impl_->path); !valid)
    {
        return std::unexpected(valid.error());
    }

    MapBlockBytes raw{};
    if (auto loaded = readRawBlock(impl_->descriptor, impl_->blocks, block, impl_->dimensions, impl_->path, raw); !loaded)
    {
        return std::unexpected(loaded.error());
    }

    return MapCommon::decodeBlock(raw);
}

std::expected<MapTile, Error> MapReader::readTile(TileCoord tile) const
{
    if (auto valid = MapCommon::validateTile(tile, impl_->dimensions, impl_->path); !valid)
    {
        return std::unexpected(valid.error());
    }

    const BlockCoord block{.x = tile.x / mapBlockDimension, .y = tile.y / mapBlockDimension};
    auto blockOffset = readableBlockOffset(impl_->descriptor, impl_->blocks, block, impl_->dimensions, impl_->path);
    if (!blockOffset)
    {
        return std::unexpected(blockOffset.error());
    }

    constexpr std::size_t tileByteSize = static_cast<std::size_t>(MapCommon::mapTileByteSize);
    std::array<std::byte, tileByteSize> raw{};
    const auto tileOffset =
        *blockOffset + MapCommon::mapHeaderByteSize + (detail::tileIndexInBlock(tile) * MapCommon::mapTileByteSize);
    if (!detail::MulCommon::readExact(impl_->descriptor, raw.data(), raw.size(), tileOffset))
    {
        return std::unexpected(detail::MulCommon::makeError(
            ErrorCode::truncatedInput, impl_->path, tileOffset, "land tile is missing or truncated"));
    }

    return detail::decodeTile(raw);
}

std::expected<void, Error> MapReader::readBlocks(BlockRegion region, const MapBlockRead & callback) const
{
    if (auto valid = validateReadRequest(region, callback, impl_->dimensions, impl_->path); !valid)
    {
        return valid;
    }

    return readRawBlocks(region,
                         [&callback](BlockCoord coordinate, const MapBlockBytes & raw) -> std::expected<void, Error>
                         { return callback(coordinate, MapCommon::decodeBlock(raw)); });
}

std::expected<void, Error> MapReader::readRawBlocks(BlockRegion region, const MapBlockBytesRead & callback) const
{
    if (auto valid = validateReadRequest(region, callback, impl_->dimensions, impl_->path); !valid)
    {
        return valid;
    }

    const auto blocksPerRead =
        std::min(detail::maximumBlocksPerIoBatch, static_cast<std::size_t>(region.maxExclusive.y - region.min.y));
    std::vector<std::byte> buffer(blocksPerRead * static_cast<std::size_t>(MapCommon::mapBlockByteSize));
    for (std::uint32_t x = region.min.x; x < region.maxExclusive.x; x++)
    {
        for (std::uint32_t y = region.min.y; y < region.maxExclusive.y;)
        {
            const auto blocksRemaining = static_cast<std::size_t>(region.maxExclusive.y - y);
            const auto blockCount = std::min(blocksPerRead, blocksRemaining);
            const BlockCoord first{.x = x, .y = y};
            if (auto read = readRawBlockBatch(
                    impl_->descriptor, impl_->blocks, first, blockCount, impl_->dimensions, impl_->path, buffer, callback);
                !read)
            {
                return read;
            }
            y += static_cast<std::uint32_t>(blockCount);
        }
    }

    return {};
}

std::expected<MapStats, Error> MapReader::stats() const
{
    return statsTiles(
        {.min = {.x = 0, .y = 0}, .maxExclusive = {.x = impl_->dimensions.widthTiles, .y = impl_->dimensions.heightTiles}});
}

std::expected<MapStats, Error> MapReader::statsTiles(TileRegion region) const
{
    if (auto valid = MapCommon::validateRegion(region, impl_->dimensions, impl_->path); !valid)
    {
        return std::unexpected(valid.error());
    }

    return detail::collectStats(*this, region);
}

std::expected<MapStats, Error> MapReader::statsBlocks(BlockRegion region) const
{
    if (auto valid = MapCommon::validateRegion(region, impl_->dimensions, impl_->path); !valid)
    {
        return std::unexpected(valid.error());
    }

    const TileRegion tiles{
        .min = {.x = region.min.x * mapBlockDimension, .y = region.min.y * mapBlockDimension},
        .maxExclusive = {.x = region.maxExclusive.x * mapBlockDimension, .y = region.maxExclusive.y * mapBlockDimension},
    };

    return detail::collectStats(*this, tiles);
}

std::expected<void, Error> MapReader::readTiles(TileRegion region, const MapTileRead & callback) const
{
    if (auto valid = validateReadRequest(region, callback, impl_->dimensions, impl_->path); !valid)
    {
        return valid;
    }

    const auto blocks = detail::blockRegionForTiles(region);
    return readRawBlocks(blocks,
                         [&callback, region](BlockCoord coordinate, const MapBlockBytes & raw) -> std::expected<void, Error>
                         {
                             const auto block = MapCommon::decodeBlock(raw);
                             return detail::visitTilesInBlock(region,
                                                              coordinate,
                                                              [&callback, &block](TileCoord tile, std::size_t index)
                                                              { return callback(tile, block.at(index)); });
                         });
}

} // namespace uomul
