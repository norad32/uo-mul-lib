#pragma once

#include <filesystem>

namespace uo_mule::gui
{

struct WorkbenchState;

[[nodiscard]] bool createDefaultRadarcol(WorkbenchState & state, const std::filesystem::path & path);
[[nodiscard]] bool drawFileWorkflow(WorkbenchState & state);
[[nodiscard]] bool handleFileWorkflowEscape(WorkbenchState & state);

} // namespace uo_mule::gui
