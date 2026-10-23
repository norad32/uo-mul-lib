#pragma once

#include "gui/dialogs/file_browser_dialog.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/profile/data_discovery.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/profile/data_validation.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace uo_mule::gui
{

inline constexpr std::size_t preferencesDialogBufferSize = 1024;

struct PreferencesDialogState
{
    bool visible = false;
    bool initialized = false;
    bool closeRequested = false;
    bool showWelcomeOnStartup = true;
    IdFormat idFormat = IdFormat::hexadecimal;
    std::array<char, preferencesDialogBufferSize> profileName{};
    std::array<char, preferencesDialogBufferSize> sourceRoot{};
    std::array<char, preferencesDialogBufferSize> outputRoot{};
    std::map<DataFileRole, std::array<char, preferencesDialogBufferSize>> overrides;
    std::map<DataFileRole, bool> metadataOverrides;
    std::vector<std::filesystem::path> profiles;
    DataProfile workingProfile;
    DataDiscoveryResult discovery;
    DataProfileValidation validation;
    FileBrowserState * browser = nullptr;
    DiscoveryMode discoveryMode = DiscoveryMode::folder;
    bool validationRequested = false;
    bool profileSaveRequested = false;
    std::string message;
};

void requestPreferencesDialog(PreferencesDialogState & state, const DataProfile & profile, const Preferences & preferences);
[[nodiscard]] bool drawPreferencesDialog(PreferencesDialogState & state,
                                         DataProfile & activeProfile,
                                         Preferences & preferences,
                                         FileBrowserState & browser);
void applyFileBrowserResult(PreferencesDialogState & state, const FileBrowserResult & result);

} // namespace uo_mule::gui
