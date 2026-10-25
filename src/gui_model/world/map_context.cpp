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
    snapshot.hasDocument = mapDocument.changes().reader_.has_value();
    snapshot.hoveredTile = mapDocument.viewState().hoveredTile_;
    snapshot.hoveredValue = mapDocument.viewState().hoveredValue_;
    snapshot.selectedTile = mapDocument.viewState().selectedTile_;
    snapshot.selectedValue = mapDocument.viewState().selectedValue_;
    snapshot.hoveredBlock = mapDocument.viewState().hoveredTile_
                                ? std::optional<uomul::BlockCoord>(tileToBlock(*mapDocument.viewState().hoveredTile_))
                                : std::nullopt;
    snapshot.selectedBlock = mapDocument.viewState().selectedTile_
                                 ? std::optional<uomul::BlockCoord>(tileToBlock(*mapDocument.viewState().selectedTile_))
                                 : std::nullopt;
    snapshot.activeTile = snapshot.hoveredTile ? snapshot.hoveredTile : snapshot.selectedTile;
    snapshot.activeValue = snapshot.hoveredValue ? snapshot.hoveredValue : snapshot.selectedValue;
    snapshot.activeBlock =
        snapshot.activeTile ? std::optional<uomul::BlockCoord>(tileToBlock(*snapshot.activeTile)) : std::nullopt;
    snapshot.selectionMode = mapDocument.viewState().selectionMode_;
    snapshot.tileSelection = mapDocument.viewState().tileSelection_;
    snapshot.blockSelection = mapDocument.viewState().blockSelection_;
    snapshot.selectionBounds = mapDocument.viewState().selectionMask_.tileBounds(mapDocument.changes().dimensions_);
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
