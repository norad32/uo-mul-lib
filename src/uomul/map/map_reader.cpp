#include "map_helper.h"
#include "uomul/map.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <expected>
#include <fcntl.h>
#include <filesystem>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace uomul
{

using detail::MapHelper;

namespace
{

std::expected<void, Error> readRawLandBlock(int descriptor,
                                            std::uint64_t completeBlocks,
                                            BlockCoord block,
                                            Dimensions dimensions,
                                            const std::filesystem::path & path,
                                            LandBlockBytes & raw)
{
    const auto offset = MapHelper::blockOffset(block, dimensions);
    if (MapHelper::blockId(block, dimensions) >= completeBlocks ||
        !MapHelper::readExact(descriptor, raw.data(), raw.size(), offset))
    {
        return std::unexpected(
            MapHelper::makeError(ErrorCode::truncatedInput, path, offset, "land block is missing or truncated"));
    }
    return {};
}

template <typename Region, typename Callback>
std::expected<void, Error>
validateReadRequest(Region region, const Callback & callback, Dimensions dimensions, const std::filesystem::path & path)
{
    if (auto valid = MapHelper::validateRegion(region, dimensions, path); !valid)
    {
        return valid;
    }
    if (!callback)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, path, 0, "read callback is empty"));
    }
    return {};
}

template <typename Visitor>
std::expected<void, Error> visitBlocks(BlockRegion region, Visitor visitor)
{
    for (std::uint32_t x = region.min.x; x < region.maxExclusive.x; ++x)
    {
        for (std::uint32_t y = region.min.y; y < region.maxExclusive.y; ++y)
        {
            auto result = visitor(BlockCoord{.x = x, .y = y});
            if (!result)
            {
                return result;
            }
        }
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
    auto count = MapHelper::blockCount(dimensions, path);
    if (!count)
    {
        return std::unexpected(count.error());
    }

    detail::ScopedDescriptor descriptor(::open(path.c_str(), O_RDONLY | O_CLOEXEC));
    if (descriptor.get() < 0)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, path, 0, std::strerror(errno)));
    }

    struct stat fileStatus{};
    if (::fstat(descriptor.get(), &fileStatus) != 0)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, path, 0, std::strerror(errno)));
    }
    if (fileStatus.st_size < 0)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, path, 0, "could not determine map size"));
    }

    const auto fileSize = static_cast<std::uint64_t>(fileStatus.st_size);
    const auto expectedSize = *count * MapHelper::landBlockByteSize;
    const auto completeBlockCount = fileSize / MapHelper::landBlockByteSize;
    if (fileSize % MapHelper::landBlockByteSize != 0 && (fileSize > expectedSize || !options.recoverTruncatedMap))
    {
        return std::unexpected(
            MapHelper::makeError(ErrorCode::truncatedInput, path, fileSize, "map has trailing bytes beyond complete blocks"));
    }

    if (!options.recoverTruncatedMap && fileSize != expectedSize)
    {
        return std::unexpected(MapHelper::makeError(
            ErrorCode::truncatedInput, path, fileSize, "map size does not match dimensions and complete blocks"));
    }

    if (fileSize > expectedSize)
    {
        return std::unexpected(MapHelper::makeError(
            ErrorCode::truncatedInput, path, expectedSize, "map contains more blocks than specified dimensions"));
    }

    if (!S_ISREG(fileStatus.st_mode))
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, path, 0, "map input must be a regular file"));
    }

    auto impl = std::make_unique<Impl>();
    impl->path = path;
    impl->dimensions = dimensions;
    impl->descriptor = descriptor.release();
    impl->blocks = std::min(*count, completeBlockCount);
    impl->missingBlocks = *count - impl->blocks;
    impl->ignoredBytes = fileSize % MapHelper::landBlockByteSize;
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
    if (auto valid = MapHelper::validateBlock(block, impl_->dimensions, impl_->path); !valid)
    {
        return std::unexpected(valid.error());
    }

    LandBlockBytes raw{};
    if (auto loaded = readRawLandBlock(impl_->descriptor, impl_->blocks, block, impl_->dimensions, impl_->path, raw); !loaded)
    {
        return std::unexpected(loaded.error());
    }
    return MapHelper::decodeBlock(raw);
}

std::expected<LandTile, Error> MapReader::readLandTile(TileCoord tile) const
{
    if (auto valid = MapHelper::validateTile(tile, impl_->dimensions, impl_->path); !valid)
    {
        return std::unexpected(valid.error());
    }

    auto block = readLandBlock({.x = tile.x / landBlockDimension, .y = tile.y / landBlockDimension});
    if (!block)
    {
        return std::unexpected(block.error());
    }

    return block->at(detail::tileIndexInBlock(tile));
}

std::expected<void, Error> MapReader::readLandBlocks(BlockRegion region, const LandBlockRead & callback) const
{
    if (auto valid = validateReadRequest(region, callback, impl_->dimensions, impl_->path); !valid)
    {
        return valid;
    }

    return visitBlocks(region,
                       [this, &callback](BlockCoord coordinate) -> std::expected<void, Error>
                       {
                           auto block = readLandBlock(coordinate);
                           if (!block)
                           {
                               return std::unexpected(block.error());
                           }
                           return callback(coordinate, *block);
                       });
}

std::expected<void, Error> MapReader::readRawLandBlocks(BlockRegion region, const LandBlockBytesRead & callback) const
{
    if (auto valid = validateReadRequest(region, callback, impl_->dimensions, impl_->path); !valid)
    {
        return valid;
    }

    LandBlockBytes raw{};
    return visitBlocks(region,
                       [this, &callback, &raw](BlockCoord coordinate) -> std::expected<void, Error>
                       {
                           if (auto loaded = readRawLandBlock(
                                   impl_->descriptor, impl_->blocks, coordinate, impl_->dimensions, impl_->path, raw);
                               !loaded)
                           {
                               return std::unexpected(loaded.error());
                           }
                           return callback(coordinate, raw);
                       });
}

std::expected<void, Error> MapReader::readLandTiles(TileRegion region, const LandTileRead & callback) const
{
    if (auto valid = validateReadRequest(region, callback, impl_->dimensions, impl_->path); !valid)
    {
        return valid;
    }

    const auto blocks = detail::blockRegionForTiles(region);

    for (std::uint32_t blockX = blocks.min.x; blockX < blocks.maxExclusive.x; ++blockX)
    {
        for (std::uint32_t blockY = blocks.min.y; blockY < blocks.maxExclusive.y; ++blockY)
        {
            auto block = readLandBlock({.x = blockX, .y = blockY});
            if (!block)
            {
                return std::unexpected(block.error());
            }
            auto visited = detail::visitTilesInBlock(region,
                                                     BlockCoord{.x = blockX, .y = blockY},
                                                     [&callback, &block](TileCoord tile, std::size_t index)
                                                     { return callback(tile, block->at(index)); });
            if (!visited)
            {
                return visited;
            }
        }
    }

    return {};
}

} // namespace uomul
