#include "gui_model/documents/map_view_state.h"
#include <algorithm>
#include <utility>

namespace uo_mule::gui
{

namespace
{

[[nodiscard]] std::uint32_t clampCoordinate(std::uint32_t coordinate, std::uint32_t limit) noexcept
{
    return std::min(coordinate, limit);
}

[[nodiscard]] uomul::TileRegion clampTileRegion(uomul::TileRegion region, uomul::Dimensions dimensions) noexcept
{
    region.min.x = clampCoordinate(region.min.x, dimensions.widthTiles);
    region.min.y = clampCoordinate(region.min.y, dimensions.heightTiles);
    region.maxExclusive.x = clampCoordinate(region.maxExclusive.x, dimensions.widthTiles);
    region.maxExclusive.y = clampCoordinate(region.maxExclusive.y, dimensions.heightTiles);
    region.maxExclusive.x = std::max(region.min.x, region.maxExclusive.x);
    region.maxExclusive.y = std::max(region.min.y, region.maxExclusive.y);
    return region;
}

[[nodiscard]] uomul::BlockRegion clampBlockRegion(uomul::BlockRegion region, uomul::Dimensions dimensions) noexcept
{
    const std::uint32_t blockWidth = (dimensions.widthTiles / uomul::mapBlockDimension) +
                                     static_cast<std::uint32_t>(dimensions.widthTiles % uomul::mapBlockDimension != 0);
    const std::uint32_t blockHeight = (dimensions.heightTiles / uomul::mapBlockDimension) +
                                      static_cast<std::uint32_t>(dimensions.heightTiles % uomul::mapBlockDimension != 0);
    region.min.x = clampCoordinate(region.min.x, blockWidth);
    region.min.y = clampCoordinate(region.min.y, blockHeight);
    region.maxExclusive.x = clampCoordinate(region.maxExclusive.x, blockWidth);
    region.maxExclusive.y = clampCoordinate(region.maxExclusive.y, blockHeight);
    region.maxExclusive.x = std::max(region.min.x, region.maxExclusive.x);
    region.maxExclusive.y = std::max(region.min.y, region.maxExclusive.y);
    return region;
}

} // namespace

void MapViewState::setSelectionMode(MapSelectionMode mode, uomul::Dimensions dimensions)
{
    if (selectionMode_ == mode)
    {
        return;
    }

    if (mode == MapSelectionMode::blocks && selectionMask_.granularity() == ToolSelectionGranularity::tiles)
    {
        selectionMask_.convertTilesToCoveringBlocks(dimensions);
        tileSelection_.reset();
        blockSelection_.reset();
        selectionMode_ = mode;
        return;
    }

    if (mode == MapSelectionMode::tiles && selectionMask_.granularity() == ToolSelectionGranularity::blocks)
    {
        selectionMask_.convertBlocksToTiles(dimensions);
        tileSelection_.reset();
        blockSelection_.reset();
        selectionMode_ = mode;
        return;
    }

    selectionMode_ = mode;
    tileSelection_.reset();
    blockSelection_.reset();
    selectionMask_.setGranularity(mode == MapSelectionMode::tiles ? ToolSelectionGranularity::tiles
                                                                  : ToolSelectionGranularity::blocks);
}

void MapViewState::setTileSelection(uomul::TileRegion region, uomul::Dimensions dimensions)
{
    tileSelection_ = clampTileRegion(region, dimensions);
    blockSelection_.reset();
    selectionMode_ = MapSelectionMode::tiles;
    selectionMask_.replaceTileRegion(*tileSelection_, dimensions);
}

void MapViewState::setBlockSelection(uomul::BlockRegion region, uomul::Dimensions dimensions)
{
    blockSelection_ = clampBlockRegion(region, dimensions);
    tileSelection_.reset();
    selectionMode_ = MapSelectionMode::blocks;
    selectionMask_.replaceBlockRegion(*blockSelection_, dimensions);
}

void MapViewState::applyTileSelection(uomul::TileRegion region, SelectionCombineMode mode, uomul::Dimensions dimensions)
{
    const uomul::TileRegion clamped = clampTileRegion(region, dimensions);
    if (mode == SelectionCombineMode::replace)
    {
        setTileSelection(clamped, dimensions);
        return;
    }

    MapSelectionMask selection;
    selection.replaceTileRegion(clamped, dimensions);
    selectionMode_ = MapSelectionMode::tiles;
    tileSelection_.reset();
    blockSelection_.reset();
    if (mode == SelectionCombineMode::add)
    {
        selectionMask_.add(selection);
    }
    else
    {
        selectionMask_.subtract(selection);
    }
}

void MapViewState::applyBlockSelection(uomul::BlockRegion region, SelectionCombineMode mode, uomul::Dimensions dimensions)
{
    const uomul::BlockRegion clamped = clampBlockRegion(region, dimensions);
    if (mode == SelectionCombineMode::replace)
    {
        setBlockSelection(clamped, dimensions);
        return;
    }

    MapSelectionMask selection;
    selection.replaceBlockRegion(clamped, dimensions);
    selectionMode_ = MapSelectionMode::blocks;
    tileSelection_.reset();
    blockSelection_.reset();
    if (mode == SelectionCombineMode::add)
    {
        selectionMask_.add(selection);
    }
    else
    {
        selectionMask_.subtract(selection);
    }
}

void MapViewState::applyLassoSelection(const std::vector<uomul::TileCoord> & path,
                                       SelectionCombineMode mode,
                                       uomul::Dimensions dimensions)
{
    MapSelectionMask selection;
    selection.setGranularity(selectionMask_.granularity());
    selection.replaceLasso(path, dimensions);
    if (mode == SelectionCombineMode::replace)
    {
        selectionMask_ = std::move(selection);
    }
    else if (mode == SelectionCombineMode::add)
    {
        selectionMask_.add(selection);
    }
    else
    {
        selectionMask_.subtract(selection);
    }
    selectionMode_ =
        selectionMask_.granularity() == ToolSelectionGranularity::tiles ? MapSelectionMode::tiles : MapSelectionMode::blocks;
    tileSelection_.reset();
    blockSelection_.reset();
}

void MapViewState::clearHover() noexcept
{
    hoveredTile_.reset();
    hoveredValue_.reset();
}

void MapViewState::clearSelectedTile() noexcept
{
    selectedTile_.reset();
    selectedValue_.reset();
}

void MapViewState::clearSelection() noexcept
{
    tileSelection_.reset();
    blockSelection_.reset();
    selectionMask_.clear();
}

void MapViewState::selectAll(uomul::Dimensions dimensions)
{
    tileSelection_.reset();
    blockSelection_.reset();
    selectionMask_.selectAll(dimensions);
    selectionMode_ =
        selectionMask_.granularity() == ToolSelectionGranularity::tiles ? MapSelectionMode::tiles : MapSelectionMode::blocks;
}

void MapViewState::invertSelection(uomul::Dimensions dimensions)
{
    selectionMask_.invert(dimensions);
    tileSelection_.reset();
    blockSelection_.reset();
}

void MapViewState::convertTileSelectionToBlocks(uomul::Dimensions dimensions)
{
    selectionMask_.convertTilesToCoveringBlocks(dimensions);
    tileSelection_.reset();
    blockSelection_.reset();
    selectionMode_ = MapSelectionMode::blocks;
}

} // namespace uo_mule::gui
