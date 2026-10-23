#pragma once

#include "gui_model/persistence/preferences.h"
#include "gui_model/providers/data_providers.h"
#include "gui_model/workspaces/map_workspace.h"
#include "gui_model/world/map_context.h"

namespace uo_mule::gui
{

void drawTileInspectorPanel(const MapWorkspace & workspace,
                            const MapContextSnapshot & context,
                            const DataProviders & providers,
                            IdFormat idFormat,
                            bool & visible);

} // namespace uo_mule::gui
