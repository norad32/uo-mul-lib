#pragma once

#include "gui/dialogs/file_browser_dialog.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace uo_mule::gui
{

struct FileSelectorState
{
    bool visible = false;
    FileBrowserResult browserResult{};
    std::vector<FileBrowserAssetCandidate> candidates;
    std::size_t selectedCandidate = 0;
    std::string error;
};

void openFileSelector(FileSelectorState & state, FileBrowserResult result);
[[nodiscard]] FileBrowserResult selectedFileSelectorResult(const FileSelectorState & state);
[[nodiscard]] std::optional<DialogResult> drawFileSelector(FileSelectorState & state, std::uint64_t generation);
[[nodiscard]] bool applyFileSelectorCompanion(FileSelectorState & state, const std::filesystem::path & path);
void cancelFileSelector(FileSelectorState & state);

} // namespace uo_mule::gui
