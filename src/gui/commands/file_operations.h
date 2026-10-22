#pragma once

namespace uo_mule::gui
{

struct WorkbenchState;

[[nodiscard]] bool drawFileOperations(WorkbenchState & state);
[[nodiscard]] bool handleFileOperationsEscape(WorkbenchState & state);

} // namespace uo_mule::gui
