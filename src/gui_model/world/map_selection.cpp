#include "gui_model/world/map_selection.h"
#include "uomul/mul_common/mul_common.h"
#include <algorithm>
#include <cmath>
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

void MapSelectionMask::replaceLasso(const std::vector<uomul::TileCoord> & path, uomul::Dimensions dimensions)
{
    selected_.clear();
    if (path.empty() || dimensions.widthTiles == 0U || dimensions.heightTiles == 0U)
    {
        return;
    }

    if (granularity_ == ToolSelectionGranularity::blocks)
    {
        for (const uomul::TileCoord point : path)
        {
            if (point.x < dimensions.widthTiles && point.y < dimensions.heightTiles)
            {
                selected_.insert(blockKey({.x = point.x / uomul::mapBlockDimension,
                                           .y = point.y / uomul::mapBlockDimension}));
            }
        }
        return;
    }

    if (path.size() < 3U)
    {
        for (const uomul::TileCoord point : path)
        {
            if (point.x < dimensions.widthTiles && point.y < dimensions.heightTiles)
            {
                selected_.insert(tileKey(point));
            }
        }
        return;
    }

    std::uint32_t minX = dimensions.widthTiles - 1U;
    std::uint32_t minY = dimensions.heightTiles - 1U;
    std::uint32_t maxX = 0U;
    std::uint32_t maxY = 0U;
    for (const uomul::TileCoord point : path)
    {
        minX = std::min(minX, point.x);
        minY = std::min(minY, point.y);
        maxX = std::max(maxX, point.x);
        maxY = std::max(maxY, point.y);
    }
    minX = std::min(minX, dimensions.widthTiles - 1U);
    minY = std::min(minY, dimensions.heightTiles - 1U);
    maxX = std::min(maxX, dimensions.widthTiles - 1U);
    maxY = std::min(maxY, dimensions.heightTiles - 1U);

    for (std::uint32_t y = minY; y <= maxY; y++)
    {
        for (std::uint32_t x = minX; x <= maxX; x++)
        {
            const double sampleX = static_cast<double>(x) + 0.5;
            const double sampleY = static_cast<double>(y) + 0.5;
            bool inside = false;
            std::size_t previous = path.size() - 1U;
            for (std::size_t current = 0; current < path.size(); current++)
            {
                const double currentY = static_cast<double>(path.at(current).y);
                const double previousY = static_cast<double>(path.at(previous).y);
                const bool crosses = (currentY > sampleY) != (previousY > sampleY);
                if (crosses)
                {
                    const double currentX = static_cast<double>(path.at(current).x);
                    const double previousX = static_cast<double>(path.at(previous).x);
                    const double intersection = ((previousX - currentX) * (sampleY - currentY) /
                                                 (previousY - currentY)) + currentX;
                    if (sampleX < intersection)
                    {
                        inside = !inside;
                    }
                }
                previous = current;
            }
            if (inside)
            {
                selected_.insert(tileKey({.x = x, .y = y}));
            }
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

bool selectionCellHasExposedEdge(const MapSelectionMask & mask,
                                 std::int32_t x,
                                 std::int32_t y,
                                 std::int32_t dx,
                                 std::int32_t dy,
                                 uomul::Dimensions dimensions) noexcept
{
    if (mask.granularity() == ToolSelectionGranularity::tiles)
    {
        if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(dimensions.widthTiles) ||
            y >= static_cast<std::int32_t>(dimensions.heightTiles) ||
            !mask.contains(uomul::TileCoord{.x = static_cast<std::uint32_t>(x), .y = static_cast<std::uint32_t>(y)}))
        {
            return false;
        }
        const std::int32_t neighborX = x + dx;
        const std::int32_t neighborY = y + dy;
        return neighborX < 0 || neighborY < 0 || neighborX >= static_cast<std::int32_t>(dimensions.widthTiles) ||
               neighborY >= static_cast<std::int32_t>(dimensions.heightTiles) ||
               !mask.contains(uomul::TileCoord{.x = static_cast<std::uint32_t>(neighborX),
                                               .y = static_cast<std::uint32_t>(neighborY)});
    }

    const std::int32_t blockWidth = static_cast<std::int32_t>((dimensions.widthTiles + uomul::mapBlockDimension - 1U) /
                                                               uomul::mapBlockDimension);
    const std::int32_t blockHeight = static_cast<std::int32_t>((dimensions.heightTiles + uomul::mapBlockDimension - 1U) /
                                                                uomul::mapBlockDimension);
    if (x < 0 || y < 0 || x >= blockWidth || y >= blockHeight ||
        !mask.contains(uomul::BlockCoord{.x = static_cast<std::uint32_t>(x), .y = static_cast<std::uint32_t>(y)}))
    {
        return false;
    }
    const std::int32_t neighborX = x + dx;
    const std::int32_t neighborY = y + dy;
    return neighborX < 0 || neighborY < 0 || neighborX >= blockWidth || neighborY >= blockHeight ||
           !mask.contains(uomul::BlockCoord{.x = static_cast<std::uint32_t>(neighborX),
                                            .y = static_cast<std::uint32_t>(neighborY)});
}

} // namespace uo_mule::gui
