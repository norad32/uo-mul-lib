#pragma once

#include "gui_model/dialogs/dialog_contracts.h"
#include <filesystem>

namespace uo_mule::gui
{

struct WorkbenchState;

[[nodiscard]] bool createDefaultRadarcol(WorkbenchState & state, const std::filesystem::path & path);
[[nodiscard]] bool completeDialogResult(WorkbenchState & state, DialogResult result);
[[nodiscard]] bool drawFileWorkflow(WorkbenchState & state);
[[nodiscard]] bool handleFileWorkflowEscape(WorkbenchState & state);

} // namespace uo_mule::gui
