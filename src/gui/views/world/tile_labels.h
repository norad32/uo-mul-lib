#pragma once

#include "gui_model/workspaces/map_workspace.h"
#include "uomul/map/map.h"
#include <string>

namespace uo_mule::gui
{

[[nodiscard]] std::string tileLabel(MapDisplayMode mode, uomul::MapTile tile);

} // namespace uo_mule::gui
