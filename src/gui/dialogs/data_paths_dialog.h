#pragma once

#include "gui/data_discovery.h"
#include "gui/data_profile.h"
#include "gui/data_validation.h"
#include "gui/dialogs/file_browser_dialog.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace uo_mule::gui
{

inline constexpr std::size_t dataPathsDialogBufferSize = 1024;

struct DataPathsDialogState
{
    bool visible = false;
    bool initialized = false;
    std::array<char, dataPathsDialogBufferSize> profileName{};
    std::array<char, dataPathsDialogBufferSize> sourceRoot{};
    std::array<char, dataPathsDialogBufferSize> outputRoot{};
    std::map<DataFileRole, std::array<char, dataPathsDialogBufferSize>> overrides;
    std::vector<std::filesystem::path> profiles;
    DataProfile workingProfile;
    DataDiscoveryResult discovery;
    DataProfileValidation validation;
    FileBrowserState * browser = nullptr;
    DiscoveryMode discoveryMode = DiscoveryMode::folder;
    std::string message;
};

void requestDataPathsDialog(DataPathsDialogState & state, const DataProfile & profile);
[[nodiscard]] bool drawDataPathsDialog(DataPathsDialogState & state, DataProfile & activeProfile, FileBrowserState & browser);
void applyFileBrowserResult(DataPathsDialogState & state, const FileBrowserResult & result);

} // namespace uo_mule::gui
