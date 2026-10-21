#pragma once

#include "gui/dialogs/file_browser_dialog.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace uo_mule::gui
{

enum class FileSelectorAction : std::uint8_t
{
    none,
    accept,
    cancel,
    chooseDifferent,
    chooseCompanion,
};

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
[[nodiscard]] FileSelectorAction drawFileSelector(FileSelectorState & state);
[[nodiscard]] bool applyFileSelectorCompanion(FileSelectorState & state, const std::filesystem::path & path);
void cancelFileSelector(FileSelectorState & state);

} // namespace uo_mule::gui
