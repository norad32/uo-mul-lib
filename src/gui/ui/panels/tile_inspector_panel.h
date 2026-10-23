#pragma once

#include "gui/persistence/preferences.h"
#include "gui/providers/data_providers.h"
#include "gui/workspaces/map_workspace.h"
#include "gui/world/map_context.h"

namespace uo_mule::gui
{

void drawTileInspectorPanel(const MapWorkspace & workspace,
                            const MapContextSnapshot & context,
                            const DataProviders & providers,
                            IdFormat idFormat,
                            bool & visible);

} // namespace uo_mule::gui
