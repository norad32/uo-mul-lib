#pragma once

#include "gui/map_workspace.h"

namespace uo_mule::gui
{

struct RadarcolPanelState;
struct TexmapsPanelState;

void drawTileInspectorPanel(const MapWorkspace & workspace, RadarcolPanelState & radarcolState,
                            TexmapsPanelState & texmapsState, bool & visible);

} // namespace uo_mule::gui
