#include "map_helper.hpp"
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

    const int descriptor = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (descriptor < 0)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, path, 0, std::strerror(errno)));
    }

    struct stat fileStatus{};
    if (::fstat(descriptor, &fileStatus) != 0)
    {
        const auto error = MapHelper::makeError(ErrorCode::io, path, 0, std::strerror(errno));
        static_cast<void>(::close(descriptor));
        return std::unexpected(error);
    }
    if (fileStatus.st_size < 0)
    {
        const auto error = MapHelper::makeError(ErrorCode::io, path, 0, "could not determine map size");
        static_cast<void>(::close(descriptor));
        return std::unexpected(error);
    }

    const auto fileSize = static_cast<std::uint64_t>(fileStatus.st_size);
    const auto expectedSize = *count * MapHelper::landBlockByteSize;
    const auto completeBlockCount = fileSize / MapHelper::landBlockByteSize;
    if (fileSize % MapHelper::landBlockByteSize != 0 && (fileSize > expectedSize || !options.recoverTruncatedMap))
    {
        const auto error =
            MapHelper::makeError(ErrorCode::truncatedInput, path, fileSize, "map has trailing bytes beyond complete blocks");
        static_cast<void>(::close(descriptor));
        return std::unexpected(error);
    }

    if (!options.recoverTruncatedMap && fileSize != expectedSize)
    {
        const auto error = MapHelper::makeError(
            ErrorCode::truncatedInput, path, fileSize, "map size does not match dimensions and complete blocks");
        static_cast<void>(::close(descriptor));
        return std::unexpected(error);
    }

    if (fileSize > expectedSize)
    {
        const auto error = MapHelper::makeError(
            ErrorCode::truncatedInput, path, expectedSize, "map contains more blocks than specified dimensions");
        static_cast<void>(::close(descriptor));
        return std::unexpected(error);
    }

    if (!S_ISREG(fileStatus.st_mode))
    {
        const auto error = MapHelper::makeError(ErrorCode::io, path, 0, "map input must be a regular file");
        static_cast<void>(::close(descriptor));
        return std::unexpected(error);
    }

    auto impl = std::make_unique<Impl>();
    impl->path = path;
    impl->dimensions = dimensions;
    impl->descriptor = descriptor;
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
    const auto offset = MapHelper::blockOffset(block, impl_->dimensions);
    if (MapHelper::blockId(block, impl_->dimensions) >= impl_->blocks ||
        !MapHelper::readExact(impl_->descriptor, raw.data(), raw.size(), offset))
    {
        return std::unexpected(
            MapHelper::makeError(ErrorCode::truncatedInput, impl_->path, offset, "land block is missing or truncated"));
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

    return block->at(((tile.y % landBlockDimension) * MapHelper::blockLocalWidth) + (tile.x % landBlockDimension));
}

std::expected<void, Error> MapReader::readLandBlocks(BlockRegion region, const LandBlockRead & callback) const
{
    if (auto valid = MapHelper::validateRegion(region, impl_->dimensions, impl_->path); !valid)
    {
        return valid;
    }
    if (!callback)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, impl_->path, 0, "read callback is empty"));
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
    if (auto valid = MapHelper::validateRegion(region, impl_->dimensions, impl_->path); !valid)
    {
        return valid;
    }

    LandBlockBytes raw{};
    if (!callback)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, impl_->path, 0, "read callback is empty"));
    }
    for (std::uint32_t x = region.min.x; x < region.maxExclusive.x; ++x)
    {
        for (std::uint32_t y = region.min.y; y < region.maxExclusive.y; ++y)
        {
            const BlockCoord coordinate{.x = x, .y = y};
            const auto offset = MapHelper::blockOffset(coordinate, impl_->dimensions);
            if (MapHelper::blockId(coordinate, impl_->dimensions) >= impl_->blocks ||
                !MapHelper::readExact(impl_->descriptor, raw.data(), raw.size(), offset))
            {
                return std::unexpected(
                    MapHelper::makeError(ErrorCode::truncatedInput, impl_->path, offset, "land block is missing or truncated"));
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
    if (auto valid = MapHelper::validateRegion(region, impl_->dimensions, impl_->path); !valid)
    {
        return valid;
    }
    if (!callback)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, impl_->path, 0, "read callback is empty"));
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
            const auto maxX = std::min(region.maxExclusive.x, (blockX * landBlockDimension) + MapHelper::blockLocalWidth);
            const auto maxY = std::min(region.maxExclusive.y, (blockY * landBlockDimension) + MapHelper::blockLocalWidth);
            for (std::uint32_t x = minX; x < maxX; ++x)
            {
                for (std::uint32_t y = minY; y < maxY; ++y)
                {
                    auto const result =
                        callback(TileCoord{.x = x, .y = y},
                                 block->at(((y % landBlockDimension) * MapHelper::blockLocalWidth) + (x % landBlockDimension)));
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

} // namespace uomul
