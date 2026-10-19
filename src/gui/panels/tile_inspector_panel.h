#pragma once

#include "gui/map_workspace.h"
#include "gui/map_context.h"
#include "gui/data_providers.h"

namespace uo_mule::gui
{

void drawTileInspectorPanel(const MapWorkspace & workspace,
                            const MapContextSnapshot & context,
                            const DataProviders & providers,
                            bool & visible);

} // namespace uo_mule::gui
