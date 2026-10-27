#pragma once

#include "gui_model/dialogs/file_browser_model.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace uo_mule::gui
{

struct DialogHostState;

struct FileBrowserRowView
{
    std::filesystem::path path;
    FileBrowserEntryMetadata metadata;
};

struct FileBrowserState
{
    DialogHostState * dialogHost = nullptr;
    bool recentFoldersDirty = false;
    bool visible = false;
    bool showHidden = false;
    bool createFolderVisible = false;
    FileBrowserRequest request{};
    std::filesystem::path directory;
    std::filesystem::path selectedPath;
    std::array<char, fileBrowserBufferSize> location{};
    std::array<char, fileBrowserBufferSize> filename{};
    std::array<char, fileBrowserBufferSize> newFolderName{};
    std::vector<std::filesystem::path> history;
    std::size_t historyIndex = 0;
    std::vector<FileBrowserRowView> listingRows;
    bool listingDirty = true;
    std::size_t listingScanCount = 0;
    std::vector<std::filesystem::path> recentFolders;
    FileBrowserSummary summary{};
    std::string error;
};

void requestFileBrowser(FileBrowserState & state, FileBrowserRequest request);
void cancelFileBrowser(FileBrowserState & state);
void persistFileBrowserRecentFolders(FileBrowserState & state);
[[nodiscard]] std::optional<DialogResult> drawFileBrowser(FileBrowserState & state, std::uint64_t generation);

} // namespace uo_mule::gui
