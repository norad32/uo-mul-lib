#include "gui/dialogs/data_paths_dialog.h"
#include "gui/data_discovery.h"
#include "gui/data_profile.h"
#include "gui/data_profile_store.h"
#include "gui/data_validation.h"
#include "gui/theme.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <functional>
#include <imgui.h>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace uo_mule::gui
{

namespace
{

constexpr float dialogWidth = 760.0F;
constexpr float dialogHeight = 700.0F;
constexpr float pathInputWidth = 480.0F;
constexpr int dataFilesColumnCount = 5;
constexpr int discoveryModeCount = 3;
constexpr std::array<const char *, discoveryModeCount> discoveryModeNames{
    "Folder scan",
    "Installation scan",
    "Deep scan",
};

void copyToBuffer(std::array<char, dataPathsDialogBufferSize> & buffer, const std::filesystem::path & value)
{
    const auto text = value.string();
    const auto length = std::min(text.size(), buffer.size() - 1U);
    std::memcpy(buffer.data(), text.data(), length);
    buffer.at(length) = '\0';
}

void copyToBuffer(std::array<char, dataPathsDialogBufferSize> & buffer, std::string_view value)
{
    const auto length = std::min(value.size(), buffer.size() - 1U);
    std::memcpy(buffer.data(), value.data(), length);
    buffer.at(length) = '\0';
}

[[nodiscard]] std::filesystem::path bufferPath(const std::array<char, dataPathsDialogBufferSize> & buffer)
{
    return buffer.data();
}

void initializeOverrides(DataPathsDialogState & state)
{
    state.overrides.clear();
    for (const auto & [role, entry] : state.workingProfile.files)
    {
        if (entry.userOverride)
        {
            std::array<char, dataPathsDialogBufferSize> buffer{};
            copyToBuffer(buffer, entry.path);
            state.overrides.emplace(role, buffer);
        }
    }
}

void initializeDialogFields(DataPathsDialogState & state)
{
    copyToBuffer(state.profileName, std::string_view(state.workingProfile.name));
    copyToBuffer(state.sourceRoot, state.workingProfile.sourceRoot);
    copyToBuffer(state.outputRoot, state.workingProfile.outputRoot);
    initializeOverrides(state);
}

void refreshProfiles(DataPathsDialogState & state)
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

void scanWorkingProfile(DataPathsDialogState & state)
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

void applyPickerSelection(DataPathsDialogState & state)
{
    if (state.picker.selectedPath.empty())
    {
        return;
    }

    switch (state.picker.mode)
    {
    case DataPathPickerMode::sourceFolder:
        copyToBuffer(state.sourceRoot, state.picker.selectedPath);
        break;
    case DataPathPickerMode::outputFolder:
        copyToBuffer(state.outputRoot, state.picker.selectedPath);
        break;
    case DataPathPickerMode::fileOverride:
    {
        auto & buffer = state.overrides[state.picker.role];
        copyToBuffer(buffer, state.picker.selectedPath);
        break;
    }
    case DataPathPickerMode::none:
        break;
    }

    state.picker = {};
}

void drawPickerEntry(DataPathsDialogState & state, const std::filesystem::directory_entry & entry)
{
    std::error_code error;
    const bool directory = entry.is_directory(error);
    if (error)
    {
        return;
    }

    const auto label = entry.path().filename().string();
    if (ImGui::Selectable(label.c_str(), state.picker.selectedPath == entry.path()))
    {
        state.picker.selectedPath = entry.path();
    }

    if (directory)
    {
        ImGui::SameLine();

        if (ImGui::SmallButton(("Open##" + entry.path().string()).c_str()))
        {
            state.picker.directory = entry.path();
            state.picker.selectedPath.clear();
        }
    }
}

void drawPicker(DataPathsDialogState & state)
{
    if (state.picker.mode == DataPathPickerMode::none)
    {
        return;
    }

    ImGui::OpenPopup("Data Path Picker");
    if (!ImGui::BeginPopupModal("Data Path Picker", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    ImGui::Text("%s", state.picker.directory.string().c_str());
    ImGui::Separator();

    std::error_code error;
    for (const auto & entry : std::filesystem::directory_iterator(state.picker.directory, error))
    {
        if (state.picker.mode == DataPathPickerMode::fileOverride && !entry.is_regular_file(error) &&
            !entry.is_directory(error))
        {
            continue;
        }

        drawPickerEntry(state, entry);
    }

    if (error)
    {
        ImGui::TextColored(theme::imguiColor(theme::amber), "%s", error.message().c_str());
    }

    if (ImGui::Button("Use Selected") && !state.picker.selectedPath.empty())
    {
        applyPickerSelection(state);
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel"))
    {
        state.picker = {};
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void requestPicker(DataPathsDialogState & state, DataPathPickerMode mode, DataFileRole role = DataFileRole::map0)
{
    state.picker = {
        .mode = mode,
        .role = role,
        .directory = state.sourceRoot.front() == '\0' ? std::filesystem::current_path() : bufferPath(state.sourceRoot),
        .selectedPath = {},
    };
}

void drawPathField(const char * label,
                   std::array<char, dataPathsDialogBufferSize> & buffer,
                   const char * buttonLabel,
                   const std::function<void()> & picker)
{
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(pathInputWidth);
    ImGui::InputText((std::string("##") + label).c_str(), buffer.data(), buffer.size());
    ImGui::SameLine();

    if (ImGui::Button(buttonLabel))
    {
        picker();
    }
}

void drawProfileSection(DataPathsDialogState & state)
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

void drawSourceSection(DataPathsDialogState & state)
{
    if (!ImGui::CollapsingHeader("Source Discovery", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    drawPathField("Source folder", state.sourceRoot, "Choose Folder...", [&state]
                  { requestPicker(state, DataPathPickerMode::sourceFolder); });
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

void drawFileRows(DataPathsDialogState & state)
{
    if (!ImGui::CollapsingHeader("Recognized Files", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    if (ImGui::BeginTable("DataFiles", dataFilesColumnCount, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Role");
        ImGui::TableSetupColumn("Path");
        ImGui::TableSetupColumn("Status");
        ImGui::TableSetupColumn("Size");
        ImGui::TableSetupColumn("Action");
        ImGui::TableHeadersRow();

        for (const auto role : knownDataFileRoles())
        {
            const auto * file = state.validation.find(role);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(std::string(dataFileRoleName(role)).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(file == nullptr ? "" : file->path.string().c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(file == nullptr ? "missing" : std::string(dataFileStatusName(file->status)).c_str());
            ImGui::TableNextColumn();
            if (file != nullptr && file->fileSize)
            {
                ImGui::Text("%llu", static_cast<unsigned long long>(*file->fileSize));
            }
            ImGui::TableNextColumn();
            if (ImGui::SmallButton(("Override##" + std::string(dataFileRoleName(role))).c_str()))
            {
                requestPicker(state, DataPathPickerMode::fileOverride, role);
            }
            ImGui::SameLine();

            if (ImGui::SmallButton(("Clear##" + std::string(dataFileRoleName(role))).c_str()))
            {
                state.overrides.erase(role);
                state.workingProfile.files.erase(role);
                scanWorkingProfile(state);
            }
        }

        ImGui::EndTable();
    }
}

void drawOutputSection(DataPathsDialogState & state)
{
    if (!ImGui::CollapsingHeader("Output", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    drawPathField("Output folder", state.outputRoot, "Choose Folder...", [&state]
                  { requestPicker(state, DataPathPickerMode::outputFolder); });
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

void drawSummary(const DataPathsDialogState & state)
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

void requestDataPathsDialog(DataPathsDialogState & state, const DataProfile & profile)
{
    state.workingProfile = profile;
    state.visible = true;
    state.initialized = true;
    state.message.clear();
    state.picker = {};
    initializeDialogFields(state);
    scanWorkingProfile(state);
}

bool drawDataPathsDialog(DataPathsDialogState & state, DataProfile & activeProfile)
{
    if (!state.visible)
    {
        return false;
    }

    bool profileChanged = false;
    ImGui::SetNextWindowSize(ImVec2(dialogWidth, dialogHeight), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("UO Data Paths", &state.visible))
    {
        ImGui::End();
        return false;
    }

    drawProfileSection(state);
    drawSourceSection(state);
    drawFileRows(state);
    drawOutputSection(state);
    drawSummary(state);
    drawPicker(state);

    if (!state.message.empty())
    {
        ImGui::TextColored(theme::imguiColor(theme::leafGreen), "%s", state.message.c_str());
    }

    if (ImGui::Button("Apply"))
    {
        scanWorkingProfile(state);
        state.message = "Changes applied for this dialog session";
    }

    ImGui::SameLine();

    if (ImGui::Button("Apply and Use"))
    {
        scanWorkingProfile(state);
        activeProfile = state.workingProfile;
        profileChanged = true;
        state.visible = false;
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel"))
    {
        state.visible = false;
    }

    ImGui::End();
    return profileChanged;
}

} // namespace uo_mule::gui
