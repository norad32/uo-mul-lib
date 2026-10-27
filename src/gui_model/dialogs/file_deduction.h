#pragma once

#include "gui_model/dialogs/dialog_contracts.h"
#include <filesystem>
#include <optional>
#include <vector>

namespace uo_mule::gui
{

struct FileBrowserSummary;

void deduceFileCandidates(FileBrowserSummary & summary, const std::filesystem::path & path);
[[nodiscard]] FileBrowserAssetCandidate inspectTexmapsSet(const std::filesystem::path & indexPath,
                                                          const std::filesystem::path & dataPath);
[[nodiscard]] FileBrowserAssetCandidate inspectTexmapsCompanion(const std::filesystem::path & selectedPath,
                                                                const std::filesystem::path & companionPath);
[[nodiscard]] std::optional<FileBrowserAssetCandidate>
selectConfidentFileBrowserCandidate(const std::vector<FileBrowserAssetCandidate> & candidates);
void sortFileBrowserCandidates(std::vector<FileBrowserAssetCandidate> & candidates);

} // namespace uo_mule::gui
