#pragma once

#include "gui_model/world/map_selection.h"
#include "uomul/map/map.h"
#include <cstdint>
#include <optional>

namespace uo_mule::gui
{

enum class MapDisplayMode : std::uint8_t
{
    tileIds,
    z,
    combined,
};

enum class MapSelectionMode : std::uint8_t
{
    tiles,
    blocks,
};

// Session-owned map presentation and interaction state. None of this state is
// part of the saved map document or its staged Changes.
struct MapViewState
{
    void setSelectionMode(MapSelectionMode mode, uomul::Dimensions dimensions);
    void setTileSelection(uomul::TileRegion region, uomul::Dimensions dimensions);
    void setBlockSelection(uomul::BlockRegion region, uomul::Dimensions dimensions);
    void applyTileSelection(uomul::TileRegion region, SelectionCombineMode mode, uomul::Dimensions dimensions);
    void applyBlockSelection(uomul::BlockRegion region, SelectionCombineMode mode, uomul::Dimensions dimensions);
    void
    applyLassoSelection(const std::vector<uomul::TileCoord> & path, SelectionCombineMode mode, uomul::Dimensions dimensions);
    void clearHover() noexcept;
    void clearSelectedTile() noexcept;
    void clearSelection() noexcept;
    void selectAll(uomul::Dimensions dimensions);
    void invertSelection(uomul::Dimensions dimensions);
    void convertTileSelectionToBlocks(uomul::Dimensions dimensions);

    MapDisplayMode displayMode = MapDisplayMode::tileIds;
    MapSelectionMode selectionMode = MapSelectionMode::tiles;
    std::optional<uomul::TileRegion> tileSelection;
    std::optional<uomul::BlockRegion> blockSelection;
    MapSelectionMask selectionMask;
    std::optional<uomul::TileCoord> hoveredTile;
    std::optional<uomul::MapTile> hoveredValue;
    std::optional<uomul::TileCoord> selectedTile;
    std::optional<uomul::MapTile> selectedValue;
};

} // namespace uo_mule::gui
