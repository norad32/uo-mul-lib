#include "gui/dialogs/preferences_dialog.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/presentation/dialog/dialog_presentation.h"
#include "gui/presentation/input/bounded_buffer.h"
#include "gui/presentation/strings/ui_strings.h"
#include "gui/presentation/theme/theme.h"
#include "gui_model/dialogs/dialog_contracts.h"
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
#include <string>
#include <string_view>
#include <system_error>

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
    if (!ImGui::CollapsingHeader("Profile", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    dialogFieldLabel("Name");
    dialogFieldWidth(pathInputWidth);
    ImGui::InputText("##Name", state.profileName.data(), state.profileName.size());
    ImGui::Text("Profiles are stored in %s", dataProfilesDirectory().string().c_str());

    refreshProfiles(state);
    dialogFieldLabel("Open Profile");
    if (ImGui::BeginCombo("##OpenProfile", state.workingProfile.name.empty() ? "None" : state.workingProfile.name.c_str()))
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

    if (ImGui::Button("New"))
    {
        state.workingProfile = DataProfile{};
        state.workingProfile.name = "New profile";
        initializeDialogFields(state);
        validateWorkingProfile(state);
    }

    ImGui::SameLine();

    if (ImGui::Button("Save Profile"))
    {
        state.workingProfile.name = state.profileName.data();
        refreshProfiles(state);
        state.message = "Profile saved";
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
    if (ImGui::Button("Browse...##MetadataPath") && state.browser != nullptr)
    {
        requestFileBrowser(*state.browser,
                           {
                               .mode = FileBrowserMode::openFile,
                               .target = FileBrowserTarget::dataOverride,
                               .role = role,
                               .initialPath = bufferPath(path->second),
                               .extension = ".mul",
                               .title = "Choose Metadata File",
                           });
    }
    const auto * validation = state.validation.find(role);
    ImGui::TextDisabled("Status: %s",
                        validation == nullptr ? "Missing"
                                              : std::string(strings::dataFileStatusLabel(validation->status)).c_str());
    ImGui::PopID();
}

void drawMetadataSection(PreferencesDialogState & state)
{
    if (!ImGui::CollapsingHeader("Metadata Paths", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    dialogFieldLabel("Search folder");
    if (ImGui::Button("Choose folder...##MetadataSearch") && state.browser != nullptr)
    {
        requestFileBrowser(*state.browser,
                           {
                               .mode = FileBrowserMode::chooseDirectory,
                               .target = FileBrowserTarget::dataSearchRoot,
                               .initialPath = bufferPath(state.sourceRoot),
                               .extension = {},
                               .title = "Search Metadata Folder",
                           });
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Search the selected folder for known metadata files.");

    for (const auto role : knownDataFileRoles())
    {
        drawMetadataPath(state, role);
    }

    if (ImGui::Button("Validate metadata"))
    {
        validateWorkingProfile(state);
        state.message = state.validation.hasErrors() ? "Metadata validation found errors" : "Metadata validation passed";
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

} // namespace

void requestPreferencesDialog(PreferencesDialogState & state, const DataProfile & profile, const Preferences & preferences)
{
    state.workingProfile = profile;
    state.visible = true;
    state.initialized = true;
    state.message.clear();
    state.browser = nullptr;
    state.showWelcomeOnStartup = preferences.showWelcomeOnStartup;
    state.idFormat = preferences.idFormat;
    state.closeRequested = false;
    state.validationRequested = false;
    initializeDialogFields(state);
    syncWorkingProfile(state);
    state.validation = validateDataProfile(state.workingProfile);
}

void applyFileBrowserResult(PreferencesDialogState & state, const FileBrowserResult & result)
{
    switch (result.target)
    {
        case FileBrowserTarget::dataSourceRoot :
            copyToBuffer(state.sourceRoot, result.path);
            break;
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

bool drawPreferencesDialog(PreferencesDialogState & state,
                           DataProfile & activeProfile,
                           Preferences & preferences,
                           FileBrowserState & browser)
{
    if (!state.visible && state.closeRequested)
    {
        const std::string title = "Preferences###dialog.preferences";
        ImGui::OpenPopup(title.c_str());
        if (ImGui::BeginPopupModal(title.c_str()))
        {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        state.closeRequested = false;
        return false;
    }

    if (!state.visible)
    {
        return false;
    }

    bool profileChanged = false;
    state.browser = &browser;
    const std::string title = "Preferences###dialog.preferences";
    if (!beginDialog(title.c_str(), &state.visible, largeDialogSpec(), ImGuiWindowFlags_NoResize))
    {
        return false;
    }

    dialogSubheading("Configure your UO data profile and application startup behavior.");
    ImGui::Separator();
    ImGui::BeginChild(
        "PreferencesContent", ImVec2(0.0F, contentHeight), ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    drawProfileSection(state);
    if (ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        dialogFieldLabel("Show Welcome screen at startup");
        ImGui::Checkbox("##ShowWelcomeScreen", &state.showWelcomeOnStartup);
        dialogFieldLabel("ID Format");
        const char * idFormatLabel = state.idFormat == IdFormat::decimal ? "Decimal" : "Hexadecimal";
        if (ImGui::BeginCombo("##IdFormat", idFormatLabel))
        {
            if (ImGui::Selectable("Hexadecimal", state.idFormat == IdFormat::hexadecimal))
            {
                state.idFormat = IdFormat::hexadecimal;
            }
            if (ImGui::Selectable("Decimal", state.idFormat == IdFormat::decimal))
            {
                state.idFormat = IdFormat::decimal;
            }
            ImGui::EndCombo();
        }
    }
    drawMetadataSection(state);
    ImGui::EndChild();

    if (!state.message.empty())
    {
        ImGui::TextColored(theme::imguiColor(theme::success), "%s", state.message.c_str());
    }

    beginDialogFooter({"Apply", "Apply and Use", "Cancel"});
    if (dialogButton("Apply"))
    {
        validateWorkingProfile(state);
        preferences.showWelcomeOnStartup = state.showWelcomeOnStartup;
        preferences.idFormat = state.idFormat;
        state.message = "Changes applied for this dialog session";
    }

    ImGui::SameLine();

    const bool profileIsMapReady = mapReady(state.workingProfile);
    if (dialogButton("Apply and Use", profileIsMapReady))
    {
        validateWorkingProfile(state);
        activeProfile = state.workingProfile;
        preferences.showWelcomeOnStartup = state.showWelcomeOnStartup;
        preferences.idFormat = state.idFormat;
        profileChanged = true;
        state.visible = false;
    }
    ImGui::SameLine();

    if (dialogButton("Cancel"))
    {
        state.visible = false;
    }

    endDialogFooter();
    if (!state.visible)
    {
        ImGui::CloseCurrentPopup();
    }
    endDialog();
    return profileChanged;
}

} // namespace uo_mule::gui
