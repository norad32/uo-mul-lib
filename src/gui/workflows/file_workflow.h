#pragma once

#include "gui_model/dialogs/dialog_contracts.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include <cstdint>
#include <filesystem>

namespace uo_mule::gui
{

struct WorkbenchState;

[[nodiscard]] bool createDefaultRadarcol(WorkbenchState & state, const std::filesystem::path & path);
[[nodiscard]] bool openStandaloneAsset(WorkbenchState & state,
                                       DocumentKind kind,
                                       const std::filesystem::path & primaryPath,
                                       const std::filesystem::path & companionPath = {},
                                       DocumentValidationState validation = DocumentValidationState::valid,
                                       std::uint32_t itemCount = 0);
[[nodiscard]] bool completeDialogResult(WorkbenchState & state, DialogResult result);
[[nodiscard]] bool drawFileWorkflow(WorkbenchState & state);
[[nodiscard]] bool handleFileWorkflowEscape(WorkbenchState & state);

} // namespace uo_mule::gui
