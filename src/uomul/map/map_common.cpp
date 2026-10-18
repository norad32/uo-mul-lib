#include "map_common.h"
#include "uomul/map.h"
#include "uomul/mul_common/mul_common.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <sys/types.h>

namespace uomul::detail
{

namespace
{

[[nodiscard]] inline bool hasInvalidMapDimensions(Dimensions dimensions)
{
    return dimensions.widthTiles == 0 || dimensions.heightTiles == 0 || dimensions.widthTiles % mapBlockDimension != 0 ||
           dimensions.heightTiles % mapBlockDimension != 0;
}

[[nodiscard]] inline bool exceedsSupportedMapFileSize(std::uint64_t blockCount)
{
    return blockCount > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()) / MapCommon::mapBlockByteSize;
}

template <typename Region>
[[nodiscard]] inline bool regionExceedsDimensions(Region region, Dimensions dimensions, std::uint32_t unit)
{
    return region.min.x > region.maxExclusive.x || region.min.y > region.maxExclusive.y ||
           region.maxExclusive.x > dimensions.widthTiles / unit || region.maxExclusive.y > dimensions.heightTiles / unit;
}

template <typename Region>
std::expected<void, Error> validateRegionBounds(Region region,
                                                Dimensions dimensions,
                                                std::uint32_t unit,
                                                const std::filesystem::path & path,
                                                const char * message)
{
    if (regionExceedsDimensions(region, dimensions, unit))
    {
        return std::unexpected(MulCommon::makeError(ErrorCode::invalidCoordinate, path, 0, message));
    }

    return {};
}

} // namespace

std::expected<std::uint64_t, Error> MapCommon::blockCount(Dimensions dimensions, const std::filesystem::path & path)
{
    if (hasInvalidMapDimensions(dimensions))
    {
        return std::unexpected(
            MulCommon::makeError(ErrorCode::invalidDimensions, path, 0, "dimensions must be positive multiples of 8"));
    }

    const auto width = static_cast<std::uint64_t>(dimensions.widthTiles / mapBlockDimension);
    const auto height = static_cast<std::uint64_t>(dimensions.heightTiles / mapBlockDimension);
    const auto blocks = width * height;

    if (exceedsSupportedMapFileSize(blocks))
    {
        return std::unexpected(
            MulCommon::makeError(ErrorCode::invalidDimensions, path, 0, "map file size exceeds supported offset range"));
    }

    return blocks;
}

std::expected<void, Error> MapCommon::validateBlock(BlockCoord block, Dimensions dimensions, const std::filesystem::path & path)
{
    if (block.x >= dimensions.widthTiles / mapBlockDimension || block.y >= dimensions.heightTiles / mapBlockDimension)
    {
        return std::unexpected(
            MulCommon::makeError(ErrorCode::invalidCoordinate, path, 0, "block coordinate is outside map dimensions"));
    }

    return {};
}

std::expected<void, Error> MapCommon::validateTile(TileCoord tile, Dimensions dimensions, const std::filesystem::path & path)
{
    if (tile.x >= dimensions.widthTiles || tile.y >= dimensions.heightTiles)
    {
        return std::unexpected(
            MulCommon::makeError(ErrorCode::invalidCoordinate, path, 0, "tile coordinate is outside map dimensions"));
    }

    return {};
}

std::uint64_t MapCommon::blockId(BlockCoord block, Dimensions dimensions)
{
    return (static_cast<std::uint64_t>(block.x) * (dimensions.heightTiles / mapBlockDimension)) + block.y;
}

std::uint64_t MapCommon::blockOffset(BlockCoord block, Dimensions dimensions)
{
    return blockId(block, dimensions) * mapBlockByteSize;
}

bool MapCommon::readBlockBatch(int descriptor,
                               BlockCoord first,
                               std::size_t blockCount,
                               Dimensions dimensions,
                               std::span<std::byte> buffer)
{
    const auto blockByteSize = static_cast<std::size_t>(mapBlockByteSize);
    if (blockCount > buffer.size() / blockByteSize)
    {
        return false;
    }

    const auto byteCount = blockCount * blockByteSize;
    return MulCommon::readExact(descriptor, buffer.data(), byteCount, blockOffset(first, dimensions));
}

MapBlock MapCommon::decodeBlock(const MapBlockBytes & raw)
{
    MapBlock block{};

    for (std::size_t index = 0; index < block.size(); index++)
    {
        const auto offset = static_cast<std::size_t>(mapHeaderByteSize) + (index * static_cast<std::size_t>(mapTileByteSize));
        const auto tileBytes = std::span<const std::byte, mapTileByteSize>(
            std::next(raw.begin(), static_cast<std::ptrdiff_t>(offset)), static_cast<std::size_t>(mapTileByteSize));
        block.at(index) = decodeTile(tileBytes);
    }

    return block;
}

void MapCommon::encodeBlock(MapBlockBytes & raw, const MapBlock & block)
{
    for (std::size_t index = 0; index < block.size(); index++)
    {
        const auto offset = static_cast<std::size_t>(mapHeaderByteSize) + (index * static_cast<std::size_t>(mapTileByteSize));
        const auto encoded = encodeTile(block.at(index));
        std::ranges::copy(encoded, std::next(raw.begin(), static_cast<std::ptrdiff_t>(offset)));
    }
}

std::expected<void, Error>
MapCommon::validateRegion(BlockRegion region, Dimensions dimensions, const std::filesystem::path & path)
{
    return validateRegionBounds(
        region, dimensions, mapBlockDimension, path, "block region is invalid or exceeds map dimensions");
}

std::expected<void, Error>
MapCommon::validateRegion(TileRegion region, Dimensions dimensions, const std::filesystem::path & path)
{
    return validateRegionBounds(region, dimensions, 1, path, "tile region is invalid or exceeds map dimensions");
}

} // namespace uomul::detail
