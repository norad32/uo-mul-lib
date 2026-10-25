#pragma once

namespace uo_mule::gui
{

struct WorkbenchState;

[[nodiscard]] bool drawFileWorkflow(WorkbenchState & state);
[[nodiscard]] bool handleFileWorkflowEscape(WorkbenchState & state);

} // namespace uo_mule::gui
