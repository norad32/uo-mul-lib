#include "gui_model/world/map_selection.h"
#include "uomul/mul_common/mul_common.h"
#include <algorithm>
#include <utility>

namespace uo_mule::gui
{
namespace
{

std::uint32_t blockCount(std::uint32_t tiles) noexcept
{
    return (tiles / uomul::mapBlockDimension) + static_cast<std::uint32_t>(tiles % uomul::mapBlockDimension != 0U);
}

} // namespace

ToolSelectionGranularity MapSelectionMask::granularity() const noexcept
{
    return granularity_;
}

std::size_t MapSelectionMask::size() const noexcept
{
    return selected_.size();
}

bool MapSelectionMask::empty() const noexcept
{
    return selected_.empty();
}

bool MapSelectionMask::contains(uomul::TileCoord coordinate) const noexcept
{
    return granularity_ == ToolSelectionGranularity::tiles && selected_.contains(tileKey(coordinate));
}

bool MapSelectionMask::contains(uomul::BlockCoord coordinate) const noexcept
{
    return granularity_ == ToolSelectionGranularity::blocks && selected_.contains(blockKey(coordinate));
}

void MapSelectionMask::clear() noexcept
{
    selected_.clear();
}

void MapSelectionMask::setGranularity(ToolSelectionGranularity granularity)
{
    if (granularity_ != granularity)
    {
        selected_.clear();
        granularity_ = granularity;
    }
}

void MapSelectionMask::replaceTileRegion(uomul::TileRegion region, uomul::Dimensions dimensions)
{
    granularity_ = ToolSelectionGranularity::tiles;
    selected_.clear();
    region.min.x = std::min(region.min.x, dimensions.widthTiles);
    region.min.y = std::min(region.min.y, dimensions.heightTiles);
    region.maxExclusive.x = std::min(region.maxExclusive.x, dimensions.widthTiles);
    region.maxExclusive.y = std::min(region.maxExclusive.y, dimensions.heightTiles);
    for (std::uint32_t y = region.min.y; y < region.maxExclusive.y; y++)
    {
        for (std::uint32_t x = region.min.x; x < region.maxExclusive.x; x++)
        {
            selected_.insert(tileKey({.x = x, .y = y}));
        }
    }
}

void MapSelectionMask::replaceBlockRegion(uomul::BlockRegion region, uomul::Dimensions dimensions)
{
    granularity_ = ToolSelectionGranularity::blocks;
    selected_.clear();
    region.min.x = std::min(region.min.x, blockCount(dimensions.widthTiles));
    region.min.y = std::min(region.min.y, blockCount(dimensions.heightTiles));
    region.maxExclusive.x = std::min(region.maxExclusive.x, blockCount(dimensions.widthTiles));
    region.maxExclusive.y = std::min(region.maxExclusive.y, blockCount(dimensions.heightTiles));
    for (std::uint32_t y = region.min.y; y < region.maxExclusive.y; y++)
    {
        for (std::uint32_t x = region.min.x; x < region.maxExclusive.x; x++)
        {
            selected_.insert(blockKey({.x = x, .y = y}));
        }
    }
}

void MapSelectionMask::selectAll(uomul::Dimensions dimensions)
{
    selected_.clear();
    if (granularity_ == ToolSelectionGranularity::tiles)
    {
        replaceTileRegion(uomul::TileRegion{.min = {}, .maxExclusive = {.x = dimensions.widthTiles, .y = dimensions.heightTiles}},
                          dimensions);
        return;
    }

    replaceBlockRegion({.min = {}, .maxExclusive = {.x = blockCount(dimensions.widthTiles),
                                                    .y = blockCount(dimensions.heightTiles)}},
                       dimensions);
}

void MapSelectionMask::invert(uomul::Dimensions dimensions)
{
    std::unordered_set<std::uint64_t> inverted;
    if (granularity_ == ToolSelectionGranularity::tiles)
    {
        for (std::uint32_t y = 0; y < dimensions.heightTiles; y++)
        {
            for (std::uint32_t x = 0; x < dimensions.widthTiles; x++)
            {
                const auto key = tileKey({.x = x, .y = y});
                if (!selected_.contains(key))
                {
                    inverted.insert(key);
                }
            }
        }
    }
    else
    {
        for (std::uint32_t y = 0; y < blockCount(dimensions.heightTiles); y++)
        {
            for (std::uint32_t x = 0; x < blockCount(dimensions.widthTiles); x++)
            {
                const auto key = blockKey({.x = x, .y = y});
                if (!selected_.contains(key))
                {
                    inverted.insert(key);
                }
            }
        }
    }
    selected_ = std::move(inverted);
}

void MapSelectionMask::add(const MapSelectionMask & other)
{
    if (granularity_ != other.granularity_)
    {
        return;
    }
    selected_.insert(other.selected_.begin(), other.selected_.end());
}

void MapSelectionMask::subtract(const MapSelectionMask & other)
{
    if (granularity_ != other.granularity_)
    {
        return;
    }
    for (const auto key : other.selected_)
    {
        selected_.erase(key);
    }
}

void MapSelectionMask::convertTilesToCoveringBlocks(uomul::Dimensions dimensions)
{
    if (granularity_ != ToolSelectionGranularity::tiles)
    {
        return;
    }

    std::unordered_set<std::uint64_t> blocks;
    for (const auto key : selected_)
    {
        const uomul::TileCoord tile{
            .x = static_cast<std::uint32_t>(key >> 32U),
            .y = static_cast<std::uint32_t>(key & 0xFFFFFFFFU),
        };
        if (tile.x < dimensions.widthTiles && tile.y < dimensions.heightTiles)
        {
            blocks.insert(blockKey({.x = tile.x / uomul::mapBlockDimension, .y = tile.y / uomul::mapBlockDimension}));
        }
    }
    granularity_ = ToolSelectionGranularity::blocks;
    selected_ = std::move(blocks);
}

std::uint64_t MapSelectionMask::tileKey(uomul::TileCoord coordinate) const noexcept
{
    return (static_cast<std::uint64_t>(coordinate.x) << 32U) | coordinate.y;
}

std::uint64_t MapSelectionMask::blockKey(uomul::BlockCoord coordinate) const noexcept
{
    return (static_cast<std::uint64_t>(coordinate.x) << 32U) | coordinate.y;
}

} // namespace uo_mule::gui
