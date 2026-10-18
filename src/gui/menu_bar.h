#pragma once

#include "gui/dialogs/data_paths_dialog.h"
#include "gui/workbench_state.h"

namespace uo_mule::gui
{

[[nodiscard]] bool drawMenuBar(WorkbenchState & state);
void requestWindowClose(WorkbenchState & state);

} // namespace uo_mule::gui
