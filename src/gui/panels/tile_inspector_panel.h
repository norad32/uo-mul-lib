#pragma once

#include "gui_model/documents/map_document_state.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/providers/data_providers.h"
#include "gui_model/world/map_context.h"

namespace uo_mule::gui
{

void drawTileInspectorPanel(const MapDocumentState & mapDocument,
                            const MapContextSnapshot & context,
                            const DataProviders & providers,
                            IdFormat idFormat,
                            bool iconFontAvailable,
                            bool & visible);

} // namespace uo_mule::gui
