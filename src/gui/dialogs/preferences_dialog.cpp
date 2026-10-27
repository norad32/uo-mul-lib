#include "gui/dialogs/preferences_dialog.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/presentation/bounded_buffer.h"
#include "gui/presentation/dialog_presentation.h"
#include "gui/presentation/iconography.h"
#include "gui/presentation/strings.h"
#include "gui/presentation/theme.h"
#include "gui_model/dialogs/dialog_contracts.h"
#include "gui_model/dialogs/dialog_host.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/profile/data_discovery.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/profile/data_profile_store.h"
#include "gui_model/profile/data_validation.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <imgui.h>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace uo_mule::gui
{

namespace
{

constexpr float pathInputWidth = 480.0F;

[[nodiscard]] std::filesystem::path bufferPath(const std::array<char, preferencesDialogBufferSize> & buffer)
{
    return buffer.data();
}

void initializeOverrides(PreferencesDialogState & state)
{
    state.overrides.clear();
    state.metadataOverrides.clear();
    for (const auto role : knownDataFileRoles())
    {
        std::array<char, preferencesDialogBufferSize> buffer{};
        const auto path = resolvedDataFilePath(state.workingProfile, role);
        copyToBuffer(buffer, path);
        state.overrides.emplace(role, buffer);
        state.metadataOverrides.emplace(role, false);
    }
    for (const auto & [role, entry] : state.workingProfile.files)
    {
        if (entry.userOverride)
        {
            std::array<char, preferencesDialogBufferSize> buffer{};
            copyToBuffer(buffer, entry.path);
            state.overrides.insert_or_assign(role, buffer);
            state.metadataOverrides[role] = entry.userOverride;
        }
    }
}

void initializeDialogFields(PreferencesDialogState & state)
{
    copyToBuffer(state.profileName, std::string_view(state.workingProfile.name));
    copyToBuffer(state.sourceRoot, state.workingProfile.sourceRoot);
    initializeOverrides(state);
}

void refreshProfiles(PreferencesDialogState & state)
{
    ++state.profileScanCount;
    state.profiles.clear();
    std::error_code error;
    for (const auto & entry : std::filesystem::directory_iterator(dataProfilesDirectory(), error))
    {
        if (entry.is_regular_file(error) && entry.path().extension() == ".profile")
        {
            state.profiles.push_back(entry.path());
        }
    }

    std::ranges::sort(state.profiles);
}

void syncWorkingProfile(PreferencesDialogState & state)
{
    state.workingProfile.sourceRoot = bufferPath(state.sourceRoot);
    state.workingProfile.name = state.profileName.data();
    for (const auto & [role, buffer] : state.overrides)
    {
        if (buffer.front() == '\0')
        {
            state.workingProfile.files.erase(role);
            continue;
        }
        state.workingProfile.files[role] = DataFileEntry{
            .path = bufferPath(buffer),
            .userOverride = state.metadataOverrides[role],
        };
    }
}

void validateWorkingProfile(PreferencesDialogState & state)
{
    syncWorkingProfile(state);
    state.validation = validateDataProfile(state.workingProfile);
}

void drawProfilePersistenceFailure(PreferencesDialogState & state)
{
    if (!state.profilePersistenceError)
    {
        return;
    }

    ImGui::TextColored(theme::imguiColor(theme::error), "%s", state.profilePersistenceError->c_str());
    if (ImGui::Button("Retry##profile-persistence"))
    {
        state.profilePersistenceRetryRequested = true;
        if (state.profilePersistenceOperation == ProfilePersistenceOperation::save)
        {
            state.profileSaveRequested = false;
        }
        else if (state.profilePersistenceOperation == ProfilePersistenceOperation::remove)
        {
            state.profileDeleteRequested.reset();
        }
        else if (state.profilePersistenceOperation == ProfilePersistenceOperation::switchProfile)
        {
            state.profileSaveRequested = true;
            state.profileSwitchRetryRequested = true;
        }
        state.profilePersistenceError.reset();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel##profile-persistence"))
    {
        state.profileSaveRequested = false;
        state.profileDeleteRequested.reset();
        state.profilePersistenceOperation = ProfilePersistenceOperation::none;
        state.profilePersistenceError.reset();
        state.profilePersistenceRetryRequested = false;
        state.profileSwitchRetryRequested = false;
    }
}

void requestProfileSave(PreferencesDialogState & state)
{
    state.workingProfile.name = state.profileName.data();
    state.profileSaveRequested = true;
    state.profilePersistenceOperation = ProfilePersistenceOperation::save;
    state.profilePersistenceError.reset();
}

void searchMetadataFolder(PreferencesDialogState & state, const std::filesystem::path & root)
{
    copyToBuffer(state.sourceRoot, root);
    state.workingProfile.sourceRoot = root;
    const auto discovery = discoverDataFiles(root, DiscoveryMode::folder);
    applyDiscovery(state.workingProfile, discovery);
    initializeDialogFields(state);
    validateWorkingProfile(state);
}

void drawProfileSection(PreferencesDialogState & state)
{
    if (!ImGui::CollapsingHeader(strings::profile(), ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    const std::string profilesPath = strings::formatProfilesStoredIn(dataProfilesDirectory());
    ImGui::TextDisabled("%s", profilesPath.c_str());

    dialogFieldLabel(strings::name());
    dialogFieldWidth(pathInputWidth);
    ImGui::InputText("##Name", state.profileName.data(), state.profileName.size());

    if (state.profileListRefreshRequested)
    {
        refreshProfiles(state);
        state.profileListRefreshRequested = false;
    }
    dialogFieldLabel(strings::openProfile());
    dialogFieldWidth(pathInputWidth);
    if (ImGui::BeginCombo("##OpenProfile",
                          state.workingProfile.name.empty() ? strings::none() : state.workingProfile.name.c_str()))
    {
        for (const auto & profilePath : state.profiles)
        {
            const auto profileName = profilePath.stem().string();
            if (ImGui::Selectable(profileName.c_str()))
            {
                const auto loaded = loadDataProfile(profilePath);
                if (loaded.diagnostics.empty())
                {
                    state.workingProfile = loaded.profile;
                    initializeDialogFields(state);
                    validateWorkingProfile(state);
                }
            }
        }

        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (ImGui::Button(strings::refresh()))
    {
        state.profileListRefreshRequested = true;
    }

    if (ImGui::Button(iconLabel(IconId::save, strings::saveProfile()).c_str()))
    {
        requestProfileSave(state);
    }

    ImGui::SameLine();
    const bool canDeleteProfile =
        !state.workingProfile.name.empty() && std::filesystem::exists(dataProfilePath(state.workingProfile.name));
    if (canDeleteProfile)
    {
        if (ImGui::Button(strings::deleteProfile()))
        {
            state.profileDeleteRequested = state.workingProfile.name;
            state.profilePersistenceOperation = ProfilePersistenceOperation::remove;
            state.profilePersistenceError.reset();
        }
    }
    else
    {
        ImGui::BeginDisabled();
        ImGui::Button(strings::deleteProfile());
        ImGui::EndDisabled();
    }

    drawProfilePersistenceFailure(state);
}

void drawMetadataPath(PreferencesDialogState & state, DataFileRole role)
{
    const auto path = state.overrides.find(role);
    if (path == state.overrides.end())
    {
        return;
    }

    const std::string label(strings::dataFileName(role));
    ImGui::PushID(label.c_str());
    dialogFieldLabel(label.c_str());
    dialogFieldWidth(pathInputWidth);
    if (ImGui::InputText("##Path", path->second.data(), path->second.size()))
    {
        state.metadataOverrides[role] = true;
        validateWorkingProfile(state);
    }
    ImGui::SameLine();
    const std::string openMetadataPathLabel = std::string(strings::open()) + "##MetadataPath";
    if (ImGui::Button(iconLabel(IconId::fileBrowserDialog, openMetadataPathLabel).c_str()) && state.browser != nullptr)
    {
        requestFileBrowser(*state.browser,
                           {
                               .mode = FileBrowserMode::openFile,
                               .target = FileBrowserTarget::chooseDataOverride,
                               .role = role,
                               .initialPath = bufferPath(path->second),
                               .extension = ".mul",
                               .title = strings::chooseMetadataFile(),
                           });
    }
    if (role == DataFileRole::tiledata)
    {
        dialogFieldLabel(strings::tiledataFormat());
        dialogFieldWidth(pathInputWidth);
        const std::string selected(tiledataLayoutSelectionName(state.workingProfile.tiledataLayout));
        if (ImGui::BeginCombo("##TiledataLayout", selected.c_str()))
        {
            for (const auto layout :
                 {TiledataLayoutSelection::autoDetect, TiledataLayoutSelection::legacy32, TiledataLayoutSelection::extended64})
            {
                const bool active = state.workingProfile.tiledataLayout == layout;
                const std::string layoutLabel(tiledataLayoutSelectionName(layout));
                if (ImGui::Selectable(layoutLabel.c_str(), active))
                {
                    state.workingProfile.tiledataLayout = layout;
                    validateWorkingProfile(state);
                }
            }
            ImGui::EndCombo();
        }
    }

    const auto * validation = state.validation.find(role);
    const DataFileStatus status = validation == nullptr ? DataFileStatus::missing : validation->status;
    const DataFileStatus displayStatus = status == DataFileStatus::overridden ? DataFileStatus::found : status;
    const theme::Color statusColor = [displayStatus]
    {
        switch (displayStatus)
        {
            case DataFileStatus::foundAndValidated :
                return theme::success;
            case DataFileStatus::found :
                return theme::info;
            case DataFileStatus::invalid :
                return theme::error;
            case DataFileStatus::missing :
            case DataFileStatus::ambiguous :
            case DataFileStatus::unsupported :
                return theme::warning;
            case DataFileStatus::optional :
                return theme::mutedText;
            case DataFileStatus::overridden :
                return theme::info;
        }
        return theme::mutedText;
    }();
    const std::string statusLabel(strings::dataFileStatusLabel(displayStatus));
    ImGui::TextColored(theme::imguiColor(statusColor), "%s", statusLabel.c_str());
    ImGui::PopID();
}

void drawMetadataSection(PreferencesDialogState & state)
{
    if (!ImGui::CollapsingHeader(iconLabel(IconId::fileBrowserDialog, strings::metadataPaths()).c_str(),
                                 ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    dialogFieldLabel(strings::searchFolder());
    if (ImGui::Button(iconLabel(IconId::fileBrowserDialog, strings::chooseFolder()).c_str()) && state.browser != nullptr)
    {
        requestFileBrowser(*state.browser,
                           {
                               .mode = FileBrowserMode::chooseDirectory,
                               .target = FileBrowserTarget::chooseDataSearchRoot,
                               .initialPath = bufferPath(state.sourceRoot),
                               .extension = {},
                               .title = strings::searchMetadataFolder(),
                           });
    }
    ImGui::NewLine();

    for (const auto role : knownDataFileRoles())
    {
        drawMetadataPath(state, role);
    }
}

void drawSettingsSection(PreferencesDialogState & state)
{
    if (!ImGui::CollapsingHeader(iconLabel(IconId::settings, strings::settings()).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    dialogFieldLabel(strings::showWelcomeScreen());
    ImGui::Checkbox("##ShowWelcomeScreen", &state.showWelcomeOnStartup);
    dialogFieldLabel(strings::idFormat());
    dialogFieldWidth(pathInputWidth);
    const char * idFormatLabel = state.idFormat == IdFormat::decimal ? strings::decimal() : strings::hexadecimalName();
    if (ImGui::BeginCombo("##IdFormat", idFormatLabel))
    {
        if (ImGui::Selectable(strings::hexadecimalName(), state.idFormat == IdFormat::hexadecimal))
        {
            state.idFormat = IdFormat::hexadecimal;
        }
        if (ImGui::Selectable(strings::decimal(), state.idFormat == IdFormat::decimal))
        {
            state.idFormat = IdFormat::decimal;
        }
        ImGui::EndCombo();
    }
    dialogFieldLabel(strings::mapViewTiles());
    dialogFieldWidth(pathInputWidth);
    const char * mapViewTileSourceLabel =
        state.mapViewTileSource == MapViewTileSource::textures ? strings::texturesRadarcolFallback() : strings::radarcol();
    if (ImGui::BeginCombo("##MapViewTileSource", mapViewTileSourceLabel))
    {
        if (ImGui::Selectable(strings::texturesRadarcolFallback(), state.mapViewTileSource == MapViewTileSource::textures))
        {
            state.mapViewTileSource = MapViewTileSource::textures;
        }
        if (ImGui::Selectable(strings::radarcol(), state.mapViewTileSource == MapViewTileSource::radarcol))
        {
            state.mapViewTileSource = MapViewTileSource::radarcol;
        }
        ImGui::EndCombo();
    }
}

} // namespace

void requestPreferencesDialog(PreferencesDialogState & state, const DataProfile & profile, const Preferences & preferences)
{
    state.workingProfile = profile;
    state.visible = true;
    state.browser = nullptr;
    state.showWelcomeOnStartup = preferences.showWelcomeOnStartup;
    state.idFormat = preferences.idFormat;
    state.mapViewTileSource = preferences.mapViewTileSource;
    state.originalPreferences = preferences;
    state.closeRequested = false;
    state.profileSaveRequested = false;
    state.profileDeleteRequested.reset();
    state.profilePersistenceOperation = ProfilePersistenceOperation::none;
    state.profilePersistenceError.reset();
    state.profilePersistenceRetryRequested = false;
    state.profileSwitchRetryRequested = false;
    state.profileListRefreshRequested = true;
    refreshProfiles(state);
    state.profileListRefreshRequested = false;
    initializeDialogFields(state);
    syncWorkingProfile(state);
    state.validation = validateDataProfile(state.workingProfile);
    activatePreferencesDialog(state);
}

void activatePreferencesDialog(const PreferencesDialogState & state)
{
    if (state.dialogHost == nullptr)
    {
        return;
    }
    auto request = makeDialogRequest(DialogId::preferences);
    request.replaceActive = true;
    static_cast<void>(state.dialogHost->request(request));
}

void applyFileBrowserResult(PreferencesDialogState & state, const FileBrowserResult & result)
{
    switch (result.target)
    {
        case FileBrowserTarget::chooseDataSearchRoot :
            searchMetadataFolder(state, result.path);
            return;
        case FileBrowserTarget::chooseDataOverride :
        {
            auto & buffer = state.overrides[result.role];
            copyToBuffer(buffer, result.path);
            state.metadataOverrides[result.role] = true;
            break;
        }
        default :
            return;
    }
    validateWorkingProfile(state);
}

std::optional<PreferencesDialogResult> drawPreferencesDialog(PreferencesDialogState & state, FileBrowserState & browser)
{
    if (!state.visible && state.closeRequested)
    {
        if (state.dialogHost != nullptr)
        {
            state.dialogHost->cancel();
        }
        const std::string title = iconLabel(IconId::preferences, strings::preferencesDialogTitle());
        if (beginDialog(title.c_str(), nullptr, largeDialogSpec(), ImGuiWindowFlags_NoResize, ImGuiCond_Always))
        {
            ImGui::CloseCurrentPopup();
            endDialog();
        }
        state.closeRequested = false;
        return std::nullopt;
    }

    if (!state.visible)
    {
        return std::nullopt;
    }

    state.browser = &browser;
    const std::string title = iconLabel(IconId::preferences, strings::preferencesDialogTitle());
    if (!beginDialog(title.c_str(), &state.visible, largeDialogSpec(), ImGuiWindowFlags_NoResize, ImGuiCond_Always))
    {
        if (!state.visible)
        {
            state.closeRequested = true;
        }
        return std::nullopt;
    }

    dialogSubheading(strings::preferencesDialogSubheading());
    drawProfileSection(state);
    drawSettingsSection(state);
    drawMetadataSection(state);

    const std::string okLabel = iconLabel(IconId::ok, strings::ok());
    const std::string cancelLabel = iconLabel(IconId::cancel, strings::cancel());
    beginDialogFooter({okLabel.c_str(), cancelLabel.c_str()}, false);
    std::optional<PreferencesDialogResult> result;
    if (dialogButton(okLabel.c_str()))
    {
        validateWorkingProfile(state);
        Preferences updatedPreferences = state.originalPreferences;
        updatedPreferences.showWelcomeOnStartup = state.showWelcomeOnStartup;
        updatedPreferences.idFormat = state.idFormat;
        updatedPreferences.mapViewTileSource = state.mapViewTileSource;
        result = PreferencesDialogResult{
            .profile = state.workingProfile,
            .preferences = std::move(updatedPreferences),
            .applyProfile = true,
        };
        requestProfileSave(state);
        state.visible = false;
    }
    ImGui::SameLine();

    if (dialogButton(cancelLabel.c_str()))
    {
        state.visible = false;
        if (state.profilePersistenceOperation == ProfilePersistenceOperation::switchProfile)
        {
            state.profilePersistenceOperation = ProfilePersistenceOperation::none;
            state.profilePersistenceError.reset();
            state.profileSwitchRetryRequested = false;
        }
    }

    endDialogFooter();
    if (!state.visible)
    {
        ImGui::CloseCurrentPopup();
        if (state.dialogHost != nullptr)
        {
            state.dialogHost->cancel();
        }
    }
    endDialog();
    return result;
}

} // namespace uo_mule::gui
