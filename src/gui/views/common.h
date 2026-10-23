#pragma once

#include "gui/documents/map_workspace.h"
#include "uomul/map.h"
#include <string>

namespace uo_mule::gui
{

[[nodiscard]] std::string tileLabel(MapDisplayMode mode, uomul::MapTile tile);

} // namespace uo_mule::gui
