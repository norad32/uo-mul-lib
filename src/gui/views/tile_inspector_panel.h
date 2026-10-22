#pragma once

#include "gui/documents/map_workspace.h"
#include "gui/context/map_context.h"
#include "gui/providers/data_providers.h"
#include "gui/persistence/preferences.h"

namespace uo_mule::gui
{

void drawTileInspectorPanel(const MapWorkspace & workspace,
                             const MapContextSnapshot & context,
                             const DataProviders & providers,
                             IdFormat idFormat,
                             bool & visible);

} // namespace uo_mule::gui
