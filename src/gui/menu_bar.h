#pragma once

#include "gui/workbench_state.h"

namespace uo_mule::gui
{

[[nodiscard]] bool drawMenuBar(WorkbenchState & state, bool showMainMenu = true);
void requestWindowClose(WorkbenchState & state);

} // namespace uo_mule::gui
