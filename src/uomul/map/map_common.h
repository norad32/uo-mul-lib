#pragma once

#include "uomul/map.h"
#include "uomul/mul_common/mul_common.h"
#include <algorithm>
#include <array>
#include <bit>
#include <expected>
#include <filesystem>
#include <limits>
#include <span>
#include <string>

namespace uomul::detail
{

inline constexpr std::size_t maximumBlocksPerIoBatch = 4096;

[[nodiscard]] constexpr BlockRegion fullBlockRegion(Dimensions dimensions)
{
    return {
        .min = {.x = 0, .y = 0},
        .maxExclusive = {.x = dimensions.widthTiles / mapBlockDimension, .y = dimensions.heightTiles / mapBlockDimension},
    };
}

[[nodiscard]] constexpr BlockRegion blockRegionForTiles(TileRegion region)
{
    return {
        .min = {.x = region.min.x / mapBlockDimension, .y = region.min.y / mapBlockDimension},
        .maxExclusive =
            {
                .x = (region.maxExclusive.x / mapBlockDimension) + (region.maxExclusive.x % mapBlockDimension == 0 ? 0U : 1U),
                .y = (region.maxExclusive.y / mapBlockDimension) + (region.maxExclusive.y % mapBlockDimension == 0 ? 0U : 1U),
            },
    };
}

[[nodiscard]] constexpr std::size_t tileIndexInBlock(TileCoord tile)
{
    return ((tile.y % mapBlockDimension) * mapBlockDimension) + (tile.x % mapBlockDimension);
}

[[nodiscard]] constexpr TileRegion clipTilesToBlock(TileRegion region, BlockCoord block)
{
    const auto blockMinX = block.x * mapBlockDimension;
    const auto blockMinY = block.y * mapBlockDimension;
    return {
        .min = {.x = std::max(region.min.x, blockMinX), .y = std::max(region.min.y, blockMinY)},
        .maxExclusive =
            {
                .x = std::min(region.maxExclusive.x, blockMinX + mapBlockDimension),
                .y = std::min(region.maxExclusive.y, blockMinY + mapBlockDimension),
            },
    };
}

template <typename Visitor>
[[nodiscard]] constexpr std::expected<void, Error> visitTilesInBlock(TileRegion region, BlockCoord block, Visitor visitor)
{
    const auto tiles = clipTilesToBlock(region, block);
    for (std::uint32_t x = tiles.min.x; x < tiles.maxExclusive.x; x++)
    {
        for (std::uint32_t y = tiles.min.y; y < tiles.maxExclusive.y; y++)
        {
            const TileCoord tile{.x = x, .y = y};
            auto result = visitor(tile, tileIndexInBlock(tile));
            if (!result)
            {
                return std::unexpected(result.error());
            }
        }
    }
    return {};
}

[[nodiscard]] constexpr MapTile decodeTile(std::span<const std::byte, 3> bytes)
{
    const auto low = std::to_integer<std::uint8_t>(bytes.front());
    const auto high = std::to_integer<std::uint8_t>(bytes.subspan(1).front());
    const auto id = static_cast<std::uint16_t>(
        static_cast<std::uint32_t>(low) |
        (static_cast<std::uint32_t>(high) << static_cast<unsigned>(std::numeric_limits<unsigned char>::digits)));
    return MapTile{
        .id = id,
        .z = std::bit_cast<std::int8_t>(std::to_integer<std::uint8_t>(bytes.back())),
    };
}

[[nodiscard]] constexpr std::array<std::byte, 3> encodeTile(MapTile tile)
{
    return {
        static_cast<std::byte>(tile.id & std::numeric_limits<std::uint8_t>::max()),
        static_cast<std::byte>(static_cast<std::uint32_t>(tile.id) >>
                               static_cast<unsigned>(std::numeric_limits<unsigned char>::digits)),
        static_cast<std::byte>(static_cast<std::uint8_t>(tile.z)),
    };
}

class MapCommon
{
public:
    static constexpr std::uint64_t mapBlockByteSize = mapBlockByteCount;
    static constexpr std::uint64_t mapTileByteSize = 3;
    static constexpr std::uint64_t mapHeaderByteSize = 4;

    [[nodiscard]] static std::expected<std::uint64_t, Error> blockCount(Dimensions dimensions,
                                                                        const std::filesystem::path & path);
    [[nodiscard]] static std::expected<void, Error>
    validateBlock(BlockCoord block, Dimensions dimensions, const std::filesystem::path & path);
    [[nodiscard]] static std::expected<void, Error>
    validateTile(TileCoord tile, Dimensions dimensions, const std::filesystem::path & path);
    [[nodiscard]] static std::uint64_t blockId(BlockCoord block, Dimensions dimensions);
    [[nodiscard]] static std::uint64_t blockOffset(BlockCoord block, Dimensions dimensions);
    [[nodiscard]] static bool readBlockBatch(int descriptor,
                                             BlockCoord first,
                                             std::size_t blockCount,
                                             Dimensions dimensions,
                                             std::span<std::byte> buffer);
    [[nodiscard]] static MapBlock decodeBlock(const MapBlockBytes & raw);
    static void encodeBlock(MapBlockBytes & raw, const MapBlock & block);
    [[nodiscard]] static std::expected<void, Error>
    validateRegion(BlockRegion region, Dimensions dimensions, const std::filesystem::path & path);
    [[nodiscard]] static std::expected<void, Error>
    validateRegion(TileRegion region, Dimensions dimensions, const std::filesystem::path & path);
};

} // namespace uomul::detail
