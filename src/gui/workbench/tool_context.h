#pragma once

#include "gui_model/tools/tool_contracts.h"

namespace uo_mule::gui
{

struct WorkbenchState;

[[nodiscard]] ToolContext toolContext(const WorkbenchState & state);

} // namespace uo_mule::gui
