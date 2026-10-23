#pragma once

#include "gui_model/dialogs/dialog_contracts.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace uo_mule::gui
{

struct FileBrowserSummary
{
    std::filesystem::path path;
    bool exists = false;
    bool regularFile = false;
    bool readable = false;
    std::optional<std::uintmax_t> size;
    std::string modified;
    std::string error;
    std::vector<FileBrowserMapCandidate> mapCandidates;
    std::vector<FileBrowserAssetCandidate> assetCandidates;
};

struct FileBrowserState
{
    bool visible = false;
    bool showHidden = false;
    FileBrowserRequest request{};
    std::filesystem::path directory;
    std::filesystem::path selectedPath;
    std::array<char, fileBrowserBufferSize> location{};
    std::array<char, fileBrowserBufferSize> filename{};
    std::vector<std::filesystem::path> history;
    std::size_t historyIndex = 0;
    std::vector<std::filesystem::path> recentFolders;
    FileBrowserSummary summary{};
    std::string error;
};

[[nodiscard]] bool fileBrowserMatchesFilter(const std::filesystem::path & path, std::string_view extension);
[[nodiscard]] FileBrowserSummary inspectFileBrowserSelection(const std::filesystem::path & path);
[[nodiscard]] FileBrowserAssetCandidate inspectTexmapsPair(const std::filesystem::path & indexPath,
                                                           const std::filesystem::path & dataPath);
void sortFileBrowserCandidates(std::vector<FileBrowserAssetCandidate> & candidates);

void requestFileBrowser(FileBrowserState & state, FileBrowserRequest request);
void cancelFileBrowser(FileBrowserState & state);
[[nodiscard]] std::optional<DialogResult> drawFileBrowser(FileBrowserState & state);

} // namespace uo_mule::gui
