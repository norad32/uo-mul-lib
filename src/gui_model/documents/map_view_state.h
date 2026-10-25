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

    MapDisplayMode displayMode_ = MapDisplayMode::tileIds;
    MapSelectionMode selectionMode_ = MapSelectionMode::tiles;
    std::optional<uomul::TileRegion> tileSelection_;
    std::optional<uomul::BlockRegion> blockSelection_;
    MapSelectionMask selectionMask_;
    std::optional<uomul::TileCoord> hoveredTile_;
    std::optional<uomul::MapTile> hoveredValue_;
    std::optional<uomul::TileCoord> selectedTile_;
    std::optional<uomul::MapTile> selectedValue_;
};

} // namespace uo_mule::gui
