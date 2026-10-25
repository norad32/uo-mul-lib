#pragma once

#include "gui_model/documents/map_document_state.h"
#include "uomul/map/map.h"
#include <string>

namespace uo_mule::gui
{

[[nodiscard]] std::string tileLabel(MapDisplayMode mode, uomul::MapTile tile);

} // namespace uo_mule::gui
