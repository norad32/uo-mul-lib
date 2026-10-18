#pragma once

#include "gui/map_workspace.h"
#include "uomul/map.h"
#include "uomul/mul_common/mul_common.h"
#include <imgui.h>
#include <string>

namespace uo_mule::gui
{

[[nodiscard]] std::string formatError(const uomul::Error & error);
[[nodiscard]] std::string tileLabel(MapDisplayMode mode, uomul::MapTile tile);
void drawOutlinedText(ImDrawList & drawList, ImVec2 position, const std::string & text);

} // namespace uo_mule::gui
