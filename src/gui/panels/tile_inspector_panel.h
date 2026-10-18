#pragma once

#include "gui/map_workspace.h"

namespace uo_mule::gui
{

struct RadarcolPanelState;

void drawTileInspectorPanel(const MapWorkspace & workspace, RadarcolPanelState & radarcolState, bool & visible);

} // namespace uo_mule::gui
