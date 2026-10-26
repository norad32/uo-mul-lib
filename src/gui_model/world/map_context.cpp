#include "gui_model/world/map_context.h"
#include "gui_model/documents/map_document.h"
#include "gui_model/documents/map_document_state.h"
#include "uomul/map/map.h"
#include <optional>

namespace uo_mule::gui
{

MapContextSnapshot makeMapContextSnapshot(const MapDocumentState & mapDocument, const std::optional<MapDocument> & document)
{
    MapContextSnapshot snapshot;
    snapshot.hasDocument = mapDocument.changes().reader.has_value();
    snapshot.hoveredTile = mapDocument.viewState().hoveredTile;
    snapshot.hoveredValue = mapDocument.viewState().hoveredValue;
    snapshot.selectedTile = mapDocument.viewState().selectedTile;
    snapshot.selectedValue = mapDocument.viewState().selectedValue;
    snapshot.hoveredBlock = mapDocument.viewState().hoveredTile
                                ? std::optional<uomul::BlockCoord>(tileToBlock(*mapDocument.viewState().hoveredTile))
                                : std::nullopt;
    snapshot.selectedBlock = mapDocument.viewState().selectedTile
                                 ? std::optional<uomul::BlockCoord>(tileToBlock(*mapDocument.viewState().selectedTile))
                                 : std::nullopt;
    snapshot.activeTile = snapshot.hoveredTile ? snapshot.hoveredTile : snapshot.selectedTile;
    snapshot.activeValue = snapshot.hoveredValue ? snapshot.hoveredValue : snapshot.selectedValue;
    snapshot.activeBlock =
        snapshot.activeTile ? std::optional<uomul::BlockCoord>(tileToBlock(*snapshot.activeTile)) : std::nullopt;
    snapshot.selectionMode = mapDocument.viewState().selectionMode;
    snapshot.tileSelection = mapDocument.viewState().tileSelection;
    snapshot.blockSelection = mapDocument.viewState().blockSelection;
    snapshot.selectionBounds = mapDocument.viewState().selectionMask.tileBounds(mapDocument.changes().dimensions);
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
