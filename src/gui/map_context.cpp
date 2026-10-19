#include "gui/map_context.h"
#include "gui/map_document.h"
#include "gui/map_workspace.h"
#include <optional>

namespace uo_mule::gui
{

MapContextSnapshot makeMapContextSnapshot(const MapWorkspace & workspace,
                                          const std::optional<MapDocument> & document)
{
    MapContextSnapshot snapshot;
    snapshot.hasDocument = workspace.reader() != nullptr;
    snapshot.hoveredTile = workspace.hoveredTile();
    snapshot.hoveredValue = workspace.hoveredValue();
    snapshot.selectedTile = workspace.selectedTile();
    snapshot.selectedValue = workspace.selectedValue();
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
