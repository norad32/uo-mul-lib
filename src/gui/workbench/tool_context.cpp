#include "gui/workbench/tool_context.h"
#include "gui/workbench/workbench_state.h"
#include "gui_model/documents/document_contracts.h"

namespace uo_mule::gui
{

ToolContext toolContext(const WorkbenchState & state)
{
    const DocumentRecord * active = state.session.activeDocumentRecord();
    const bool mapOpen = active != nullptr && active->summary.identity.kind == DocumentKind::map &&
                         state.mapDocuments.active().changes().reader.has_value();
    return {
        .mapDocumentOpen = mapOpen,
        .mapEditable = mapOpen,
        .selectionGranularity = state.toolState.selectionGranularity(),
    };
}

} // namespace uo_mule::gui
