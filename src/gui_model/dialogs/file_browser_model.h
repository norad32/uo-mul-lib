#pragma once

#include "gui_model/dialogs/dialog_contracts.h"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
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

[[nodiscard]] bool fileBrowserMatchesFilter(const std::filesystem::path & path, std::string_view extension);
[[nodiscard]] bool isFileBrowserDirectory(const std::filesystem::path & path);
[[nodiscard]] std::filesystem::path normalizeFileBrowserDirectory(const std::filesystem::path & path);
[[nodiscard]] std::vector<std::filesystem::path> loadRecentFolders();
void saveRecentFolders(const std::vector<std::filesystem::path> & folders);
void recordRecentFolder(std::vector<std::filesystem::path> & folders, const std::filesystem::path & folder);
[[nodiscard]] FileBrowserSummary inspectFileBrowserSelection(const std::filesystem::path & path);

} // namespace uo_mule::gui
