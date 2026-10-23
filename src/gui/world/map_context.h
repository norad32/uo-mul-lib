#pragma once

#include "gui/documents/map_document.h"
#include "gui/workspaces/map_workspace.h"
#include <optional>

namespace uo_mule::gui
{

struct MapContextSnapshot
{
    DocumentId documentId = 0;
    bool hasDocument = false;
    std::filesystem::path sourcePath;
    uomul::Dimensions dimensions{};
    bool dirty = false;
    std::optional<uomul::TileCoord> hoveredTile;
    std::optional<uomul::MapTile> hoveredValue;
    std::optional<uomul::TileCoord> selectedTile;
    std::optional<uomul::MapTile> selectedValue;
    std::optional<uomul::BlockCoord> hoveredBlock;
    std::optional<uomul::BlockCoord> selectedBlock;
    std::optional<uomul::TileCoord> activeTile;
    std::optional<uomul::MapTile> activeValue;
    std::optional<uomul::BlockCoord> activeBlock;
    MapSelectionMode selectionMode = MapSelectionMode::tiles;
    std::optional<uomul::TileRegion> tileSelection;
    std::optional<uomul::BlockRegion> blockSelection;
};

[[nodiscard]] MapContextSnapshot makeMapContextSnapshot(const MapWorkspace & workspace,
                                                        const std::optional<MapDocument> & document);

} // namespace uo_mule::gui
