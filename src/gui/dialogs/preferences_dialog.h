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
#include <optional>
#include <string>
#include <vector>

namespace uo_mule::gui
{

struct DialogHostState;

inline constexpr std::size_t preferencesDialogBufferSize = 1024;

struct PreferencesDialogState
{
    DialogHostState * dialogHost = nullptr;
    bool visible = false;
    bool closeRequested = false;
    bool showWelcomeOnStartup = true;
    IdFormat idFormat = IdFormat::hexadecimal;
    MapViewTileSource mapViewTileSource = MapViewTileSource::textures;
    Preferences originalPreferences;
    std::array<char, preferencesDialogBufferSize> profileName{};
    std::array<char, preferencesDialogBufferSize> sourceRoot{};
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
    std::optional<std::string> profileDeleteRequested;
    std::string message;
};

struct PreferencesDialogResult
{
    DataProfile profile;
    Preferences preferences;
    bool applyProfile = false;
};

void requestPreferencesDialog(PreferencesDialogState & state, const DataProfile & profile, const Preferences & preferences);
void activatePreferencesDialog(const PreferencesDialogState & state);
[[nodiscard]] std::optional<PreferencesDialogResult> drawPreferencesDialog(PreferencesDialogState & state,
                                                                           FileBrowserState & browser);
void applyFileBrowserResult(PreferencesDialogState & state, const FileBrowserResult & result);

} // namespace uo_mule::gui
