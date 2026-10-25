#include "gui_model/world/map_selection.h"
#include "gui_model/tools/tool_contracts.h"
#include "uomul/map/map.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

namespace uo_mule::gui
{
namespace
{

constexpr unsigned coordinateKeyShift = 32U;
constexpr double lassoSampleOffset = 0.5;

std::uint32_t blockCount(std::uint32_t tiles) noexcept
{
    return (tiles / uomul::mapBlockDimension) + static_cast<std::uint32_t>(tiles % uomul::mapBlockDimension != 0U);
}

struct LassoBounds
{
    std::uint32_t minX;
    std::uint32_t minY;
    std::uint32_t maxX;
    std::uint32_t maxY;
};

LassoBounds lassoBounds(const std::vector<uomul::TileCoord> & path, uomul::Dimensions dimensions)
{
    LassoBounds bounds{
        .minX = dimensions.widthTiles - 1U,
        .minY = dimensions.heightTiles - 1U,
        .maxX = 0U,
        .maxY = 0U,
    };
    for (const uomul::TileCoord point : path)
    {
        bounds.minX = std::min(bounds.minX, point.x);
        bounds.minY = std::min(bounds.minY, point.y);
        bounds.maxX = std::max(bounds.maxX, point.x);
        bounds.maxY = std::max(bounds.maxY, point.y);
    }
    bounds.minX = std::min(bounds.minX, dimensions.widthTiles - 1U);
    bounds.minY = std::min(bounds.minY, dimensions.heightTiles - 1U);
    bounds.maxX = std::min(bounds.maxX, dimensions.widthTiles - 1U);
    bounds.maxY = std::min(bounds.maxY, dimensions.heightTiles - 1U);
    return bounds;
}

bool pointInPolygon(const std::vector<uomul::TileCoord> & path, double x, double y)
{
    bool inside = false;
    std::size_t previous = path.size() - 1U;
    for (std::size_t current = 0; current < path.size(); current++)
    {
        const double currentY = static_cast<double>(path.at(current).y);
        const double previousY = static_cast<double>(path.at(previous).y);
        if ((currentY > y) != (previousY > y))
        {
            const double currentX = static_cast<double>(path.at(current).x);
            const double previousX = static_cast<double>(path.at(previous).x);
            const double intersection = ((previousX - currentX) * (y - currentY) / (previousY - currentY)) + currentX;
            if (x < intersection)
            {
                inside = !inside;
            }
        }
        previous = current;
    }
    return inside;
}

double orientation(double ax, double ay, double bx, double by, double cx, double cy)
{
    return ((bx - ax) * (cy - ay)) - ((by - ay) * (cx - ax));
}

bool segmentsIntersect(double aX, double aY, double bX, double bY, double cX, double cY, double dX, double dY)
{
    constexpr double epsilon = 1.0e-9;
    const double first = orientation(aX, aY, bX, bY, cX, cY);
    const double second = orientation(aX, aY, bX, bY, dX, dY);
    const double third = orientation(cX, cY, dX, dY, aX, aY);
    const double fourth = orientation(cX, cY, dX, dY, bX, bY);
    const bool firstCrosses = (first > epsilon && second < -epsilon) || (first < -epsilon && second > epsilon) ||
                              std::abs(first) <= epsilon || std::abs(second) <= epsilon;
    const bool secondCrosses = (third > epsilon && fourth < -epsilon) || (third < -epsilon && fourth > epsilon) ||
                               std::abs(third) <= epsilon || std::abs(fourth) <= epsilon;
    return firstCrosses && secondCrosses;
}

bool blockIntersectsLasso(const std::vector<uomul::TileCoord> & path,
                          std::uint32_t blockX,
                          std::uint32_t blockY,
                          uomul::Dimensions dimensions)
{
    const std::uint32_t blockMinX = blockX * uomul::mapBlockDimension;
    const std::uint32_t blockMinY = blockY * uomul::mapBlockDimension;
    const std::uint32_t blockMaxX = std::min(dimensions.widthTiles, blockMinX + uomul::mapBlockDimension);
    const std::uint32_t blockMaxY = std::min(dimensions.heightTiles, blockMinY + uomul::mapBlockDimension);
    const double minimumX = static_cast<double>(blockMinX);
    const double minimumY = static_cast<double>(blockMinY);
    const double maximumX = static_cast<double>(blockMaxX);
    const double maximumY = static_cast<double>(blockMaxY);
    bool intersects = pointInPolygon(path, minimumX, minimumY) || pointInPolygon(path, maximumX, minimumY) ||
                      pointInPolygon(path, minimumX, maximumY) || pointInPolygon(path, maximumX, maximumY);
    for (std::size_t edge = 0; edge < path.size() && !intersects; edge++)
    {
        const std::size_t next = (edge + 1U) % path.size();
        const auto & first = path.at(edge);
        const auto & second = path.at(next);
        intersects = segmentsIntersect(first.x, first.y, second.x, second.y, minimumX, minimumY, maximumX, minimumY) ||
                     segmentsIntersect(first.x, first.y, second.x, second.y, maximumX, minimumY, maximumX, maximumY) ||
                     segmentsIntersect(first.x, first.y, second.x, second.y, maximumX, maximumY, minimumX, maximumY) ||
                     segmentsIntersect(first.x, first.y, second.x, second.y, minimumX, maximumY, minimumX, minimumY);
    }
    return intersects;
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

std::optional<uomul::TileRegion> MapSelectionMask::tileBounds(uomul::Dimensions dimensions) const
{
    if (selected_.empty() || dimensions.widthTiles == 0U || dimensions.heightTiles == 0U)
    {
        return std::nullopt;
    }

    std::uint32_t minX = dimensions.widthTiles;
    std::uint32_t minY = dimensions.heightTiles;
    std::uint32_t maxExclusiveX = 0U;
    std::uint32_t maxExclusiveY = 0U;
    if (granularity_ == ToolSelectionGranularity::tiles)
    {
        forEachSelectedTile(
            [&minX, &minY, &maxExclusiveX, &maxExclusiveY, dimensions](uomul::TileCoord tile)
            {
                if (tile.x >= dimensions.widthTiles || tile.y >= dimensions.heightTiles)
                {
                    return;
                }
                minX = std::min(minX, tile.x);
                minY = std::min(minY, tile.y);
                maxExclusiveX = std::max(maxExclusiveX, tile.x + 1U);
                maxExclusiveY = std::max(maxExclusiveY, tile.y + 1U);
            });
    }
    else
    {
        forEachSelectedBlock(
            [&minX, &minY, &maxExclusiveX, &maxExclusiveY, dimensions](uomul::BlockCoord block)
            {
                const std::uint32_t blockMinX = block.x * uomul::mapBlockDimension;
                const std::uint32_t blockMinY = block.y * uomul::mapBlockDimension;
                if (blockMinX >= dimensions.widthTiles || blockMinY >= dimensions.heightTiles)
                {
                    return;
                }
                minX = std::min(minX, blockMinX);
                minY = std::min(minY, blockMinY);
                maxExclusiveX = std::max(maxExclusiveX, std::min(dimensions.widthTiles, blockMinX + uomul::mapBlockDimension));
                maxExclusiveY = std::max(maxExclusiveY, std::min(dimensions.heightTiles, blockMinY + uomul::mapBlockDimension));
            });
    }

    if (minX >= maxExclusiveX || minY >= maxExclusiveY)
    {
        return std::nullopt;
    }
    return uomul::TileRegion{
        .min = {.x = minX, .y = minY},
        .maxExclusive = {.x = maxExclusiveX, .y = maxExclusiveY},
    };
}

void MapSelectionMask::forEachSelectedTile(const std::function<void(uomul::TileCoord)> & callback) const
{
    if (!callback || granularity_ != ToolSelectionGranularity::tiles)
    {
        return;
    }

    for (const std::uint64_t key : selected_)
    {
        callback({
            .x = static_cast<std::uint32_t>(key >> coordinateKeyShift),
            .y = static_cast<std::uint32_t>(key),
        });
    }
}

void MapSelectionMask::forEachSelectedBlock(const std::function<void(uomul::BlockCoord)> & callback) const
{
    if (!callback || granularity_ != ToolSelectionGranularity::blocks)
    {
        return;
    }

    for (const std::uint64_t key : selected_)
    {
        callback({
            .x = static_cast<std::uint32_t>(key >> coordinateKeyShift),
            .y = static_cast<std::uint32_t>(key),
        });
    }
}

void MapSelectionMask::clear() noexcept
{
    selected_.clear();
}

void MapSelectionMask::selectTile(uomul::TileCoord coordinate, uomul::Dimensions dimensions)
{
    if (coordinate.x >= dimensions.widthTiles || coordinate.y >= dimensions.heightTiles)
    {
        return;
    }
    granularity_ = ToolSelectionGranularity::tiles;
    selected_.insert(tileKey(coordinate));
}

void MapSelectionMask::setGranularity(ToolSelectionGranularity granularity)
{
    if (granularity_ != granularity)
    {
        selected_.clear();
        granularity_ = granularity;
    }
}

void MapSelectionMask::convertBlocksToTiles(uomul::Dimensions dimensions)
{
    if (granularity_ != ToolSelectionGranularity::blocks)
    {
        return;
    }

    std::unordered_set<std::uint64_t> tiles;
    for (const std::uint64_t key : selected_)
    {
        const uomul::BlockCoord block{
            .x = static_cast<std::uint32_t>(key >> 32U),
            .y = static_cast<std::uint32_t>(key & 0xFFFFFFFFU),
        };
        const std::uint32_t minX = block.x * uomul::mapBlockDimension;
        const std::uint32_t minY = block.y * uomul::mapBlockDimension;
        const std::uint32_t maxX = std::min(dimensions.widthTiles, minX + uomul::mapBlockDimension);
        const std::uint32_t maxY = std::min(dimensions.heightTiles, minY + uomul::mapBlockDimension);
        for (std::uint32_t y = minY; y < maxY; y++)
        {
            for (std::uint32_t x = minX; x < maxX; x++)
            {
                tiles.insert(tileKey({.x = x, .y = y}));
            }
        }
    }
    granularity_ = ToolSelectionGranularity::tiles;
    selected_ = std::move(tiles);
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

// The rasterizer intentionally handles polygon and clipped-block geometry in one pass.
void MapSelectionMask::replaceLasso(const std::vector<uomul::TileCoord> & path, uomul::Dimensions dimensions)
{
    selected_.clear();
    if (path.empty() || dimensions.widthTiles == 0U || dimensions.heightTiles == 0U)
    {
        return;
    }

    if (granularity_ == ToolSelectionGranularity::blocks)
    {
        replaceBlockLasso(path, dimensions);
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

    replaceTileLasso(path, dimensions);
}

void MapSelectionMask::replaceBlockLasso(const std::vector<uomul::TileCoord> & path, uomul::Dimensions dimensions)
{
    const LassoBounds bounds = lassoBounds(path, dimensions);
    for (std::uint32_t blockY = bounds.minY / uomul::mapBlockDimension; blockY <= bounds.maxY / uomul::mapBlockDimension;
         blockY++)
    {
        for (std::uint32_t blockX = bounds.minX / uomul::mapBlockDimension; blockX <= bounds.maxX / uomul::mapBlockDimension;
             blockX++)
        {
            if (blockIntersectsLasso(path, blockX, blockY, dimensions))
            {
                selected_.insert(blockKey({.x = blockX, .y = blockY}));
            }
        }
    }
}

void MapSelectionMask::replaceTileLasso(const std::vector<uomul::TileCoord> & path, uomul::Dimensions dimensions)
{
    const LassoBounds bounds = lassoBounds(path, dimensions);
    for (std::uint32_t y = bounds.minY; y <= bounds.maxY; y++)
    {
        for (std::uint32_t x = bounds.minX; x <= bounds.maxX; x++)
        {
            if (pointInPolygon(path, static_cast<double>(x) + lassoSampleOffset, static_cast<double>(y) + lassoSampleOffset))
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
        const uomul::TileRegion allTiles{
            .min = {.x = 0U, .y = 0U},
            .maxExclusive = {.x = dimensions.widthTiles, .y = dimensions.heightTiles},
        };
        replaceTileRegion(allTiles, dimensions);
        return;
    }

    const uomul::BlockRegion allBlocks{
        .min = {.x = 0U, .y = 0U},
        .maxExclusive = {.x = blockCount(dimensions.widthTiles), .y = blockCount(dimensions.heightTiles)},
    };
    replaceBlockRegion(allBlocks, dimensions);
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

std::uint64_t MapSelectionMask::tileKey(uomul::TileCoord coordinate) noexcept
{
    return (static_cast<std::uint64_t>(coordinate.x) << coordinateKeyShift) | coordinate.y;
}

std::uint64_t MapSelectionMask::blockKey(uomul::BlockCoord coordinate) noexcept
{
    return (static_cast<std::uint64_t>(coordinate.x) << coordinateKeyShift) | coordinate.y;
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
        const auto width = static_cast<std::int32_t>(dimensions.widthTiles);
        const auto height = static_cast<std::int32_t>(dimensions.heightTiles);
        if (x < 0 || y < 0 || x >= width || y >= height ||
            !mask.contains(uomul::TileCoord{.x = static_cast<std::uint32_t>(x), .y = static_cast<std::uint32_t>(y)}))
        {
            return false;
        }
        const std::int32_t neighborX = x + dx;
        const std::int32_t neighborY = y + dy;
        return neighborX < 0 || neighborY < 0 || neighborX >= width || neighborY >= height ||
               !mask.contains(uomul::TileCoord{
                   .x = static_cast<std::uint32_t>(neighborX),
                   .y = static_cast<std::uint32_t>(neighborY),
               });
    }

    const std::int32_t blockWidth =
        static_cast<std::int32_t>((dimensions.widthTiles + uomul::mapBlockDimension - 1U) / uomul::mapBlockDimension);
    const std::int32_t blockHeight =
        static_cast<std::int32_t>((dimensions.heightTiles + uomul::mapBlockDimension - 1U) / uomul::mapBlockDimension);
    if (x < 0 || y < 0 || x >= blockWidth || y >= blockHeight ||
        !mask.contains(uomul::BlockCoord{.x = static_cast<std::uint32_t>(x), .y = static_cast<std::uint32_t>(y)}))
    {
        return false;
    }
    const std::int32_t neighborX = x + dx;
    const std::int32_t neighborY = y + dy;
    return neighborX < 0 || neighborY < 0 || neighborX >= blockWidth || neighborY >= blockHeight ||
           !mask.contains(uomul::BlockCoord{
               .x = static_cast<std::uint32_t>(neighborX),
               .y = static_cast<std::uint32_t>(neighborY),
           });
}

} // namespace uo_mule::gui
