#include "gui_model/documents/map_view_state.h"
#include "gui_model/tools/tool_contracts.h"
#include "gui_model/world/map_selection.h"
#include "uomul/map/map.h"
#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

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
    if (selectionMode == mode)
    {
        return;
    }

    if (mode == MapSelectionMode::blocks && selectionMask.granularity() == ToolSelectionGranularity::tiles)
    {
        selectionMask.convertTilesToCoveringBlocks(dimensions);
        tileSelection.reset();
        blockSelection.reset();
        selectionMode = mode;
        return;
    }

    if (mode == MapSelectionMode::tiles && selectionMask.granularity() == ToolSelectionGranularity::blocks)
    {
        selectionMask.convertBlocksToTiles(dimensions);
        tileSelection.reset();
        blockSelection.reset();
        selectionMode = mode;
        return;
    }

    selectionMode = mode;
    tileSelection.reset();
    blockSelection.reset();
    selectionMask.setGranularity(mode == MapSelectionMode::tiles ? ToolSelectionGranularity::tiles
                                                                 : ToolSelectionGranularity::blocks);
}

void MapViewState::setTileSelection(uomul::TileRegion region, uomul::Dimensions dimensions)
{
    tileSelection = clampTileRegion(region, dimensions);
    blockSelection.reset();
    selectionMode = MapSelectionMode::tiles;
    selectionMask.replaceTileRegion(*tileSelection, dimensions);
}

void MapViewState::setBlockSelection(uomul::BlockRegion region, uomul::Dimensions dimensions)
{
    blockSelection = clampBlockRegion(region, dimensions);
    tileSelection.reset();
    selectionMode = MapSelectionMode::blocks;
    selectionMask.replaceBlockRegion(*blockSelection, dimensions);
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
    selectionMode = MapSelectionMode::tiles;
    tileSelection.reset();
    blockSelection.reset();
    if (mode == SelectionCombineMode::add)
    {
        selectionMask.add(selection);
    }
    else
    {
        selectionMask.subtract(selection);
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
    selectionMode = MapSelectionMode::blocks;
    tileSelection.reset();
    blockSelection.reset();
    if (mode == SelectionCombineMode::add)
    {
        selectionMask.add(selection);
    }
    else
    {
        selectionMask.subtract(selection);
    }
}

void MapViewState::applyLassoSelection(const std::vector<uomul::TileCoord> & path,
                                       SelectionCombineMode mode,
                                       uomul::Dimensions dimensions)
{
    MapSelectionMask selection;
    selection.setGranularity(selectionMask.granularity());
    selection.replaceLasso(path, dimensions);
    if (mode == SelectionCombineMode::replace)
    {
        selectionMask = std::move(selection);
    }
    else if (mode == SelectionCombineMode::add)
    {
        selectionMask.add(selection);
    }
    else
    {
        selectionMask.subtract(selection);
    }
    selectionMode =
        selectionMask.granularity() == ToolSelectionGranularity::tiles ? MapSelectionMode::tiles : MapSelectionMode::blocks;
    tileSelection.reset();
    blockSelection.reset();
}

void MapViewState::clearHover() noexcept
{
    hoveredTile.reset();
    hoveredValue.reset();
}

void MapViewState::clearSelectedTile() noexcept
{
    selectedTile.reset();
    selectedValue.reset();
}

void MapViewState::clearSelection() noexcept
{
    tileSelection.reset();
    blockSelection.reset();
    selectionMask.clear();
}

void MapViewState::selectAll(uomul::Dimensions dimensions)
{
    tileSelection.reset();
    blockSelection.reset();
    selectionMask.selectAll(dimensions);
    selectionMode =
        selectionMask.granularity() == ToolSelectionGranularity::tiles ? MapSelectionMode::tiles : MapSelectionMode::blocks;
}

void MapViewState::invertSelection(uomul::Dimensions dimensions)
{
    selectionMask.invert(dimensions);
    tileSelection.reset();
    blockSelection.reset();
}

void MapViewState::convertTileSelectionToBlocks(uomul::Dimensions dimensions)
{
    selectionMask.convertTilesToCoveringBlocks(dimensions);
    tileSelection.reset();
    blockSelection.reset();
    selectionMode = MapSelectionMode::blocks;
}

} // namespace uo_mule::gui
