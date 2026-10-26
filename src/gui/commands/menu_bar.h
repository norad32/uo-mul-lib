#pragma once

#include "gui/workbench/workbench_state.h"

namespace uo_mule::gui
{

[[nodiscard]] bool drawMenuBar(WorkbenchState & state, bool showMainMenu = true, bool drawDialogs = true);
void requestWindowClose(WorkbenchState & state);

} // namespace uo_mule::gui
