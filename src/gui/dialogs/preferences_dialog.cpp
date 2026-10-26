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
#include "gui_model/session/workbench_session.h"
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
constexpr float contentHeight = 560.0F;

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
    state.validationRequested = true;
}

void searchMetadataFolder(PreferencesDialogState & state, const std::filesystem::path & root)
{
    copyToBuffer(state.sourceRoot, root);
    state.workingProfile.sourceRoot = root;
    state.discovery = discoverDataFiles(root, DiscoveryMode::folder);
    applyDiscovery(state.workingProfile, state.discovery);
    initializeDialogFields(state);
    validateWorkingProfile(state);
    state.message = "Metadata search completed";
}

void drawProfileSection(PreferencesDialogState & state)
{
    if (!ImGui::CollapsingHeader(strings::profile(), ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    dialogFieldLabel(strings::name());
    dialogFieldWidth(pathInputWidth);
    ImGui::InputText("##Name", state.profileName.data(), state.profileName.size());
    const std::string profilesPath = strings::formatProfilesStoredIn(dataProfilesDirectory());
    ImGui::TextUnformatted(profilesPath.c_str());

    refreshProfiles(state);
    dialogFieldLabel(strings::openProfile());
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

    if (ImGui::Button(strings::newProfile()))
    {
        state.workingProfile = DataProfile{};
        state.workingProfile.name = "New profile";
        initializeDialogFields(state);
        validateWorkingProfile(state);
    }

    ImGui::SameLine();

    if (ImGui::Button(iconLabel(IconId::save, strings::saveProfile()).c_str()))
    {
        state.workingProfile.name = state.profileName.data();
        state.profileSaveRequested = true;
        refreshProfiles(state);
        state.message = "Profile saved";
    }

    ImGui::SameLine();
    const bool canDeleteProfile =
        !state.workingProfile.name.empty() && std::filesystem::exists(dataProfilePath(state.workingProfile.name));
    if (canDeleteProfile)
    {
        if (ImGui::Button(strings::deleteProfile()))
        {
            state.profileDeleteRequested = state.workingProfile.name;
            state.visible = false;
        }
    }
    else
    {
        ImGui::BeginDisabled();
        ImGui::Button(strings::deleteProfile());
        ImGui::EndDisabled();
    }
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
    ImGui::InputText(
        "##Path", path->second.data(), path->second.size(), ImGuiInputTextFlags_ElideLeft | ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (ImGui::Button(iconLabel(IconId::fileBrowserDialog, "Browse...##MetadataPath").c_str()) && state.browser != nullptr)
    {
        requestFileBrowser(*state.browser,
                           {
                               .mode = FileBrowserMode::openFile,
                               .target = FileBrowserTarget::dataOverride,
                               .role = role,
                               .initialPath = bufferPath(path->second),
                               .extension = ".mul",
                               .title = strings::chooseMetadataFile(),
                           });
    }
    const auto * validation = state.validation.find(role);
    ImGui::TextDisabled(strings::statusFormat(),
                        validation == nullptr ? strings::missing()
                                              : std::string(strings::dataFileStatusLabel(validation->status)).c_str());
    if (role == DataFileRole::tiledata)
    {
        ImGui::SameLine();
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
    ImGui::PopID();
}

void drawMetadataSection(PreferencesDialogState & state)
{
    if (!ImGui::CollapsingHeader(iconLabel(IconId::fileBrowserDialog, "Metadata Paths").c_str(),
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
                               .target = FileBrowserTarget::dataSearchRoot,
                               .initialPath = bufferPath(state.sourceRoot),
                               .extension = {},
                               .title = strings::searchMetadataFolder(),
                           });
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", strings::searchMetadataHint());

    for (const auto role : knownDataFileRoles())
    {
        drawMetadataPath(state, role);
    }

    if (ImGui::Button(iconLabel(IconId::warning, strings::validateMetadata()).c_str()))
    {
        validateWorkingProfile(state);
        state.message =
            state.validation.hasErrors() ? strings::metadataValidationErrors() : strings::metadataValidationPassed();
    }
    if (!state.validationRequested)
    {
        return;
    }
    for (const auto & diagnostic : state.validation.diagnostics)
    {
        ImGui::BulletText("%s", diagnostic.message.c_str());
    }
}

void drawSettingsSection(PreferencesDialogState & state)
{
    if (!ImGui::CollapsingHeader(iconLabel(IconId::settings, "Settings").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    dialogFieldLabel(strings::showWelcomeScreen());
    ImGui::Checkbox("##ShowWelcomeScreen", &state.showWelcomeOnStartup);
    dialogFieldLabel("ID Format");
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
    state.message.clear();
    state.browser = nullptr;
    state.showWelcomeOnStartup = preferences.showWelcomeOnStartup;
    state.idFormat = preferences.idFormat;
    state.mapViewTileSource = preferences.mapViewTileSource;
    state.originalPreferences = preferences;
    state.closeRequested = false;
    state.validationRequested = false;
    state.profileSaveRequested = false;
    state.profileDeleteRequested.reset();
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
    static_cast<void>(state.dialogHost->request(std::move(request)));
}

void applyFileBrowserResult(PreferencesDialogState & state, const FileBrowserResult & result)
{
    switch (result.target)
    {
        case FileBrowserTarget::dataSearchRoot :
            searchMetadataFolder(state, result.path);
            return;
        case FileBrowserTarget::dataOverride :
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
        if (beginDialog(title.c_str(), nullptr, largeDialogSpec(), ImGuiWindowFlags_NoResize))
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
    if (!beginDialog(title.c_str(), &state.visible, largeDialogSpec(), ImGuiWindowFlags_NoResize))
    {
        return std::nullopt;
    }

    dialogSubheading(strings::preferencesDialogSubheading());
    ImGui::Separator();
    ImGui::BeginChild(
        "PreferencesContent", ImVec2(0.0F, contentHeight), ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    drawProfileSection(state);
    drawSettingsSection(state);
    drawMetadataSection(state);
    ImGui::EndChild();

    if (!state.message.empty())
    {
        ImGui::TextColored(theme::imguiColor(theme::success), "%s", state.message.c_str());
    }

    beginDialogFooter({strings::apply(), strings::applyAndUse(), strings::cancel()});
    std::optional<PreferencesDialogResult> result;
    if (dialogButton(strings::apply()))
    {
        validateWorkingProfile(state);
        Preferences updatedPreferences = state.originalPreferences;
        updatedPreferences.showWelcomeOnStartup = state.showWelcomeOnStartup;
        updatedPreferences.idFormat = state.idFormat;
        updatedPreferences.mapViewTileSource = state.mapViewTileSource;
        result = PreferencesDialogResult{
            .profile = state.workingProfile,
            .preferences = std::move(updatedPreferences),
            .applyProfile = false,
        };
        state.message = strings::changesApplied();
    }

    ImGui::SameLine();

    const bool profileIsMapReady = mapReady(state.workingProfile);
    if (dialogButton(strings::applyAndUse(), profileIsMapReady))
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
        state.profileSaveRequested = true;
        state.visible = false;
    }
    ImGui::SameLine();

    if (dialogButton(iconLabel(IconId::cancel, strings::cancel()).c_str()))
    {
        state.visible = false;
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
