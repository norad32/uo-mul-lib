#pragma once

#include "gui_model/dialogs/dialog_contracts.h"
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
    DialogHostState * dialogHost = nullptr;
    bool recentFoldersDirty = false;
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
void persistFileBrowserRecentFolders(FileBrowserState & state);
[[nodiscard]] std::optional<DialogResult> drawFileBrowser(FileBrowserState & state, std::uint64_t generation);

} // namespace uo_mule::gui
