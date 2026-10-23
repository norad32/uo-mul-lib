#include "gui/world/map_context.h"
#include "gui/documents/map_document.h"
#include "gui/workspaces/map_workspace.h"
#include "uomul/map.h"
#include <optional>

namespace uo_mule::gui
{

MapContextSnapshot makeMapContextSnapshot(const MapWorkspace & workspace, const std::optional<MapDocument> & document)
{
    MapContextSnapshot snapshot;
    snapshot.hasDocument = workspace.reader() != nullptr;
    snapshot.hoveredTile = workspace.hoveredTile();
    snapshot.hoveredValue = workspace.hoveredValue();
    snapshot.selectedTile = workspace.selectedTile();
    snapshot.selectedValue = workspace.selectedValue();
    snapshot.hoveredBlock =
        workspace.hoveredTile() ? std::optional<uomul::BlockCoord>(tileToBlock(*workspace.hoveredTile())) : std::nullopt;
    snapshot.selectedBlock =
        workspace.selectedTile() ? std::optional<uomul::BlockCoord>(tileToBlock(*workspace.selectedTile())) : std::nullopt;
    snapshot.activeTile = snapshot.hoveredTile ? snapshot.hoveredTile : snapshot.selectedTile;
    snapshot.activeValue = snapshot.hoveredValue ? snapshot.hoveredValue : snapshot.selectedValue;
    snapshot.activeBlock =
        snapshot.activeTile ? std::optional<uomul::BlockCoord>(tileToBlock(*snapshot.activeTile)) : std::nullopt;
    snapshot.selectionMode = workspace.selectionMode();
    snapshot.tileSelection = workspace.tileSelection();
    snapshot.blockSelection = workspace.blockSelection();
    if (document)
    {
        snapshot.documentId = document->id;
        snapshot.sourcePath = document->sourcePath;
        snapshot.dimensions = document->dimensions;
        snapshot.dirty = document->dirty;
    }
    return snapshot;
}

} // namespace uo_mule::gui
