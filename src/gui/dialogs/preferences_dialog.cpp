#include "gui/dialogs/preferences_dialog.h"
#include "gui/profile/data_discovery.h"
#include "gui/profile/data_profile.h"
#include "gui/profile/data_profile_store.h"
#include "gui/frontend/theme.h"
#include "gui/profile/data_validation.h"
#include "gui/dialogs/file_size.h"
#include "gui/persistence/preferences.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/dialogs/dialog_presentation.h"
#include "gui/profile/workbench_session.h"
#include "gui/strings/ui_strings.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <functional>
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
constexpr int dataFilesColumnCount = 4;
constexpr int discoveryModeCount = 3;
constexpr std::array<const char *, discoveryModeCount> discoveryModeNames{
    "Folder scan",
    "Installation scan",
    "Deep scan",
};

void copyToBuffer(std::array<char, preferencesDialogBufferSize> & buffer, const std::filesystem::path & value)
{
    const auto text = value.string();
    const auto length = std::min(text.size(), buffer.size() - 1U);
    std::memcpy(buffer.data(), text.data(), length);
    buffer.at(length) = '\0';
}

void copyToBuffer(std::array<char, preferencesDialogBufferSize> & buffer, std::string_view value)
{
    const auto length = std::min(value.size(), buffer.size() - 1U);
    const std::string text(value);
    std::memcpy(buffer.data(), text.data(), length);
    buffer.at(length) = '\0';
}

[[nodiscard]] std::filesystem::path bufferPath(const std::array<char, preferencesDialogBufferSize> & buffer)
{
    return buffer.data();
}

void initializeOverrides(PreferencesDialogState & state)
{
    state.overrides.clear();
    for (const auto & [role, entry] : state.workingProfile.files)
    {
        if (entry.userOverride)
        {
            std::array<char, preferencesDialogBufferSize> buffer{};
            copyToBuffer(buffer, entry.path);
            state.overrides.emplace(role, buffer);
        }
    }
}

void initializeDialogFields(PreferencesDialogState & state)
{
    copyToBuffer(state.profileName, std::string_view(state.workingProfile.name));
    copyToBuffer(state.sourceRoot, state.workingProfile.sourceRoot);
    copyToBuffer(state.outputRoot, state.workingProfile.outputRoot);
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

void scanWorkingProfile(PreferencesDialogState & state)
{
    state.workingProfile.sourceRoot = bufferPath(state.sourceRoot);
    state.workingProfile.outputRoot = bufferPath(state.outputRoot);
    state.workingProfile.name = state.profileName.data();
    state.discovery = {};
    state.validation = {};

    if (!state.workingProfile.sourceRoot.empty())
    {
        state.discovery = discoverDataFiles(state.workingProfile.sourceRoot, state.discoveryMode);
        applyDiscovery(state.workingProfile, state.discovery);
    }

    for (const auto & [role, buffer] : state.overrides)
    {
        state.workingProfile.files[role] = DataFileEntry{
            .path = bufferPath(buffer),
            .userOverride = true,
        };
    }

    state.validation = validateDataProfile(state.workingProfile);
}

void drawPathField(const char * label,
                   std::array<char, preferencesDialogBufferSize> & buffer,
                   const std::function<void()> & picker)
{
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(pathInputWidth);
    ImGui::InputText("##Path", buffer.data(), buffer.size(), ImGuiInputTextFlags_ElideLeft | ImGuiInputTextFlags_ReadOnly);
    if (ImGui::IsItemClicked())
    {
        picker();
    }

    ImGui::PopID();
}

void drawProfileSection(PreferencesDialogState & state)
{
    if (!ImGui::CollapsingHeader("Profile", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    ImGui::SetNextItemWidth(pathInputWidth);
    ImGui::InputText("Name", state.profileName.data(), state.profileName.size());
    ImGui::Text("Profiles are stored in %s", dataProfilesDirectory().string().c_str());

    refreshProfiles(state);
    if (ImGui::BeginCombo("Open Profile", state.workingProfile.name.empty() ? "None" : state.workingProfile.name.c_str()))
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
                    scanWorkingProfile(state);
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
        scanWorkingProfile(state);
    }

    ImGui::SameLine();

    if (ImGui::Button("Save Profile"))
    {
        state.workingProfile.name = state.profileName.data();
        saveDataProfile(state.workingProfile, dataProfilePath(state.workingProfile.name));
        refreshProfiles(state);
        state.message = "Profile saved";
    }
}

void drawSourceSection(PreferencesDialogState & state)
{
    if (!ImGui::CollapsingHeader("Source Discovery", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    drawPathField("Source folder", state.sourceRoot, [&state]
                  {
                      if (state.browser != nullptr)
                      {
                          requestFileBrowser(*state.browser,
                                             {.mode = FileBrowserMode::chooseDirectory,
                                              .target = FileBrowserTarget::dataSourceRoot,
                                              .initialPath = bufferPath(state.sourceRoot),
                                              .extension = {},
                                              .title = "Choose Source Folder",});
                      }
                  });
    ImGui::TextUnformatted("Folder scan checks the selected root and documented UO layout folders.");

    int discoveryModeIndex = 0;
    if (state.discoveryMode == DiscoveryMode::installation)
    {
        discoveryModeIndex = 1;
    }
    else if (state.discoveryMode == DiscoveryMode::deep)
    {
        discoveryModeIndex = 2;
    }

    if (ImGui::Combo("Scope", &discoveryModeIndex, discoveryModeNames.data(), discoveryModeCount))
    {
        state.discoveryMode = DiscoveryMode::folder;

        if (discoveryModeIndex == 1)
        {
            state.discoveryMode = DiscoveryMode::installation;
        }
        else if (discoveryModeIndex == 2)
        {
            state.discoveryMode = DiscoveryMode::deep;
        }
    }

    if (ImGui::Button("Scan / Re-scan"))
    {
        scanWorkingProfile(state);
    }
}

void drawFileRow(PreferencesDialogState & state, DataFileRole role)
{
    const auto * file = state.validation.find(role);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    const std::string label(strings::dataFileName(role));
    ImGui::TextUnformatted(label.c_str());
    ImGui::TableNextColumn();
    std::filesystem::path path;
    if (file != nullptr)
    {
        path = file->path;
    }
    ImGui::TextUnformatted(path.empty() ? "Not found" : path.string().c_str());
    if (ImGui::IsItemHovered() && !path.empty())
    {
        const std::string purpose(strings::dataFilePurpose(role));
        ImGui::SetTooltip("%s\n%s", path.string().c_str(), purpose.c_str());
    }
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(file == nullptr ? "Missing" : std::string(strings::dataFileStatusLabel(file->status)).c_str());
    ImGui::TableNextColumn();
    if (file != nullptr && file->fileSize)
    {
        ImGui::TextUnformatted(formatFileSize(*file->fileSize).c_str());
    }
}

void drawFileRows(PreferencesDialogState & state)
{
    if (!ImGui::CollapsingHeader("Metadata Paths", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    if (ImGui::BeginTable("DataFiles", dataFilesColumnCount, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Role");
        ImGui::TableSetupColumn("Path");
        ImGui::TableSetupColumn("Status");
        ImGui::TableSetupColumn("Size");
        ImGui::TableHeadersRow();

        for (const auto role : knownDataFileRoles())
        {
            drawFileRow(state, role);
        }

        ImGui::EndTable();
    }
}

void drawOutputSection(PreferencesDialogState & state)
{
    if (!ImGui::CollapsingHeader("Output", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    drawPathField("Output folder", state.outputRoot, [&state]
                  {
                      if (state.browser != nullptr)
                      {
                          requestFileBrowser(*state.browser,
                                             {.mode = FileBrowserMode::chooseDirectory,
                                              .target = FileBrowserTarget::dataOutputRoot,
                                              .initialPath = bufferPath(state.outputRoot),
                                              .extension = {},
                                              .title = "Choose Output Folder",});
                      }
                  });
    ImGui::Checkbox("Create if missing", &state.workingProfile.createOutputIfMissing);
    ImGui::BeginDisabled();
    ImGui::Checkbox("Use source folder", &state.workingProfile.useSourceFolderForOutput);
    ImGui::EndDisabled();
    const auto policy = state.workingProfile.outputCollisionPolicy;
    ImGui::Text("Collision policy: %s", std::string(outputCollisionPolicyName(policy)).c_str());

    ImGui::SeparatorText("Preview");
    if (state.workingProfile.outputRoot.empty())
    {
        ImGui::TextUnformatted("Choose an output folder to preview generated paths.");
    }
    else
    {
        for (const auto role : knownDataFileRoles())
        {
            const auto input = resolvedDataFilePath(state.workingProfile, role);
            if (!input.empty())
            {
                ImGui::Text("%s -> %s", input.filename().string().c_str(),
                            (state.workingProfile.outputRoot / input.filename()).string().c_str());
            }
        }
    }

    if (ImGui::Button("Refresh Preview"))
    {
        scanWorkingProfile(state);
    }
}

void drawSummary(const PreferencesDialogState & state)
{
    if (!ImGui::CollapsingHeader("Validation Summary", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    const auto errors = std::count_if(state.validation.diagnostics.begin(),
                                      state.validation.diagnostics.end(),
                                      [](const Diagnostic & diagnostic)
                                      { return diagnostic.severity == Diagnostic::Severity::error; });
    const auto warnings = std::count_if(state.validation.diagnostics.begin(),
                                        state.validation.diagnostics.end(),
                                        [](const Diagnostic & diagnostic)
                                        { return diagnostic.severity == Diagnostic::Severity::warning; });
    ImGui::Text("Errors: %zu  Warnings: %zu", errors, warnings);
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
    state.closeRequested = false;
    initializeDialogFields(state);
    scanWorkingProfile(state);
}

void applyFileBrowserResult(PreferencesDialogState & state, const FileBrowserResult & result)
{
    switch (result.target)
    {
    case FileBrowserTarget::dataSourceRoot:
        copyToBuffer(state.sourceRoot, result.path);
        break;
    case FileBrowserTarget::dataOutputRoot:
        copyToBuffer(state.outputRoot, result.path);
        break;
    case FileBrowserTarget::dataOverride:
    {
        auto & buffer = state.overrides[result.role];
        copyToBuffer(buffer, result.path);
        break;
    }
    default:
        return;
    }
    scanWorkingProfile(state);
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

    ImGui::TextUnformatted("Configure your UO data profile and application startup behavior.");
    ImGui::Separator();
    ImGui::BeginChild("PreferencesContent",
                      ImVec2(0.0F, contentHeight),
                      ImGuiChildFlags_None,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);
    drawProfileSection(state);
    drawSourceSection(state);
    drawFileRows(state);
    drawOutputSection(state);
    drawSummary(state);
    ImGui::EndChild();

    if (!state.message.empty())
    {
        ImGui::TextColored(theme::imguiColor(theme::leafGreen), "%s", state.message.c_str());
    }

    ImGui::Checkbox("Show welcome screen on startup", &state.showWelcomeOnStartup);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Show the welcome screen after a valid profile is loaded when the application starts.");
    }

    beginDialogFooter();
    if (dialogButton("Apply"))
    {
        scanWorkingProfile(state);
        preferences.showWelcomeOnStartup = state.showWelcomeOnStartup;
        savePreferences(preferences);
        state.message = "Changes applied for this dialog session";
    }

    ImGui::SameLine();

    const bool profileIsMapReady = mapReady(state.workingProfile);
    if (dialogButton("Apply and Use", profileIsMapReady))
    {
        scanWorkingProfile(state);
        activeProfile = state.workingProfile;
        preferences.showWelcomeOnStartup = state.showWelcomeOnStartup;
        savePreferences(preferences);
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
