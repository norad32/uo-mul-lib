#include "gui/welcome_screen.h"
#include "gui/command_registry.h"
#include "gui/data_profile.h"
#include "gui/data_validation.h"
#include "gui/new_map_dialog.h"
#include "gui/recent_maps.h"
#include "gui/theme.h"
#include "gui/welcome_greetings.h"
#include "gui/workbench_session.h"
#include "gui/workbench_state.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <imgui.h>
#include <random>
#include <string>
#include <string_view>

namespace uo_mule::gui
{
namespace
{

constexpr int feluccaFacetIndex = 1;
constexpr int trammelFacetIndex = 2;
constexpr int ilshenarFacetIndex = 3;
constexpr int malasFacetIndex = 4;
constexpr int tokunoFacetIndex = 5;
constexpr int terMurFacetIndex = 6;
constexpr float welcomeContentWidth = 920.0F;
constexpr float actionButtonWidth = 180.0F;
constexpr float actionButtonHeight = 44.0F;
constexpr float actionButtonSpacing = 8.0F;
constexpr float bannerHeight = 180.0F;
constexpr float bannerCenterFactor = 0.5F;
constexpr float recentRowHeight = 86.0F;
constexpr std::size_t timestampBufferSize = 32;

[[nodiscard]] std::string timestampLabel(std::string_view prefix, std::int64_t timestamp)
{
    if (timestamp <= 0)
    {
        return std::string(prefix) + ": unknown";
    }

    const std::time_t time = static_cast<std::time_t>(timestamp);
    std::tm localTime{};
    if (localtime_r(&time, &localTime) == nullptr)
    {
        return std::string(prefix) + ": unknown";
    }

    std::array<char, timestampBufferSize> formatted{};
    if (std::strftime(formatted.data(), formatted.size(), "%Y-%m-%d %H:%M", &localTime) == 0U)
    {
        return std::string(prefix) + ": unknown";
    }
    return std::string(prefix) + ": " + std::string(formatted.data());
}

[[nodiscard]] const char * readinessLabel(ProfileReadinessState state) noexcept
{
    switch (state)
    {
        case ProfileReadinessState::needsProfile :
            return "No profile configured";
        case ProfileReadinessState::profileInvalid :
            return "Profile invalid";
        case ProfileReadinessState::profileIncomplete :
            return "Profile incomplete";
        case ProfileReadinessState::ready :
            return "Ready";
    }
    return "Unknown";
}

[[nodiscard]] std::string_view roleLabel(DataFileRole role) noexcept
{
    switch (role)
    {
        case DataFileRole::radarcol :
            return "Radar colors";
        case DataFileRole::texidx :
            return "Texture index";
        case DataFileRole::texmaps :
            return "Texture data";
        case DataFileRole::tiledata :
            return "Tile data";
    }
    return "Data";
}

[[nodiscard]] const char * statusLabel(DataFileStatus status) noexcept
{
    switch (status)
    {
        case DataFileStatus::found :
            return "Found";
        case DataFileStatus::foundAndValidated :
            return "Ready";
        case DataFileStatus::missing :
            return "Missing";
        case DataFileStatus::ambiguous :
            return "Ambiguous";
        case DataFileStatus::invalid :
            return "Invalid";
        case DataFileStatus::overridden :
            return "Override";
        case DataFileStatus::optional :
            return "Optional";
        case DataFileStatus::unsupported :
            return "Unsupported";
    }
    return "Unknown";
}

[[nodiscard]] ImVec4 statusColor(DataFileStatus status)
{
    if (status == DataFileStatus::foundAndValidated || status == DataFileStatus::overridden || status == DataFileStatus::found)
    {
        return theme::imguiColor(theme::sageGreen);
    }
    if (status == DataFileStatus::missing || status == DataFileStatus::optional || status == DataFileStatus::unsupported)
    {
        return theme::imguiColor(theme::amber);
    }
    return theme::imguiColor(theme::coral);
}

[[nodiscard]] std::string dimensionsLabel(const RecentMap & recent)
{
    if (!recent.dimensions)
    {
        return "Dimensions unknown";
    }

    return std::to_string(recent.dimensions->widthTiles) + " x " + std::to_string(recent.dimensions->heightTiles);
}

[[nodiscard]] std::string facetLabel(const RecentMap & recent)
{
    if (recent.facet.empty())
    {
        return "Custom or unknown facet";
    }

    std::string result = recent.facet;
    result.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(result.front())));
    return result;
}

void applyRecentMapToOpenDialog(NewMapDialogState & dialog, const RecentMap & recent)
{
    dialog.mode = MapFileDialogMode::openMap;
    dialog.activePopup = MapFileDialogMode::none;
    dialog.openWithoutReview = recent.dimensions.has_value();
    std::ranges::fill(dialog.openPath, '\0');
    const std::string path = recent.path.string();
    std::copy_n(path.c_str(), std::min(path.size(), dialog.openPath.size() - 1U), dialog.openPath.data());

    if (!recent.dimensions)
    {
        return;
    }

    dialog.openWidth = recent.dimensions->widthTiles;
    dialog.openHeight = recent.dimensions->heightTiles;
    dialog.openFacet = 0;
    if (recent.facet == "felucca")
    {
        dialog.openFacet = feluccaFacetIndex;
    }
    else if (recent.facet == "trammel")
    {
        dialog.openFacet = trammelFacetIndex;
    }
    else if (recent.facet == "ilshenar")
    {
        dialog.openFacet = ilshenarFacetIndex;
    }
    else if (recent.facet == "malas")
    {
        dialog.openFacet = malasFacetIndex;
    }
    else if (recent.facet == "tokuno")
    {
        dialog.openFacet = tokunoFacetIndex;
    }
    else if (recent.facet == "ter-mur")
    {
        dialog.openFacet = terMurFacetIndex;
    }
}

void drawProfileSummary(const DataProfile & profile)
{
    const DataProfileValidation validation = validateDataProfile(profile);
    ImGui::TextUnformatted("Data profile");
    ImGui::TextDisabled("%s", profile.name.empty() ? "Unnamed profile" : profile.name.c_str());

    if (!profile.sourceRoot.empty())
    {
        ImGui::TextDisabled("Source: %s", profile.sourceRoot.string().c_str());
    }
    if (!profile.outputRoot.empty())
    {
        ImGui::TextDisabled("Output: %s", profile.outputRoot.string().c_str());
    }

    for (const auto role : {DataFileRole::radarcol, DataFileRole::texidx, DataFileRole::texmaps, DataFileRole::tiledata})
    {
        const auto * file = validation.find(role);
        if (file == nullptr)
        {
            continue;
        }
        const std::string_view roleName = roleLabel(role);
        ImGui::TextColored(statusColor(file->status),
                           "%.*s: %s",
                           static_cast<int>(roleName.size()),
                           roleName.data(),
                           statusLabel(file->status));
    }
}

void drawRecentMap(WorkbenchState & state, const RecentMap & recent, std::size_t index)
{
    ImGui::PushID(static_cast<int>(index));
    ImGui::BeginChild("###welcome.recent-row",
                      ImVec2(0.0F, recentRowHeight),
                      ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const bool rowHovered = ImGui::IsWindowHovered();
    if (rowHovered)
    {
        const ImVec2 rowMin = ImGui::GetWindowPos();
        const ImVec2 rowMax = ImVec2(rowMin.x + ImGui::GetWindowSize().x, rowMin.y + ImGui::GetWindowSize().y);
        ImGui::GetWindowDrawList()->AddRectFilled(rowMin, rowMax, theme::packedColor(theme::brassWash));
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::SetTooltip("%s", recent.path.string().c_str());
    }

    const bool unavailable = !recent.failure.empty();
    ImGui::TextUnformatted(recent.displayName.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s", dimensionsLabel(recent).c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s", facetLabel(recent).c_str());
    const std::string modifiedLabel = timestampLabel("Modified", recent.lastModified);
    const std::string openedLabel = timestampLabel("Opened", recent.lastOpened);
    ImGui::TextDisabled("%s | %s", modifiedLabel.c_str(), openedLabel.c_str());
    if (unavailable)
    {
        ImGui::TextColored(theme::imguiColor(theme::coral), "Unavailable: %s", recent.failure.c_str());
    }

    const bool rowDoubleClicked = rowHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    ImGui::EndChild();
    if (rowDoubleClicked)
    {
        applyRecentMapToOpenDialog(state.menuBarState.fileDialog, recent);
    }
    ImGui::PopID();
}

void selectGreetingIfNeeded(WorkbenchState & state)
{
    if (state.welcomeGreetingSelected)
    {
        return;
    }

    static std::mt19937 generator(std::random_device{}());
    state.welcomeGreeting = chooseWelcomeGreeting(generator);
    state.welcomeGreetingSelected = true;
}

} // namespace

void drawWelcomeScreen(WorkbenchState & state)
{
    selectGreetingIfNeeded(state);
    state.session.refreshRecentMaps();

    const ImGuiViewport * viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);
    constexpr auto welcomeWindowFlags = static_cast<ImGuiWindowFlags>(
        static_cast<unsigned int>(ImGuiWindowFlags_NoTitleBar) | static_cast<unsigned>(ImGuiWindowFlags_NoResize) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoDecoration) | static_cast<unsigned int>(ImGuiWindowFlags_NoBackground) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoScrollbar) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoScrollWithMouse) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoSavedSettings));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
    if (!ImGui::Begin("Map Welcome###welcome.map", nullptr, welcomeWindowFlags))
    {
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    const float contentWidth = std::min(welcomeContentWidth, ImGui::GetContentRegionAvail().x);
    const float left = std::max(0.0F, (ImGui::GetContentRegionAvail().x - contentWidth) * 0.5F);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + left);
    ImGui::BeginChild("###welcome.content",
                      ImVec2(contentWidth, 0.0F),
                      ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::BeginChild("###welcome.banner",
                      ImVec2(0.0F, bannerHeight),
                      ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const char * bannerText = "Artwork banner";
    const ImVec2 bannerTextSize = ImGui::CalcTextSize(bannerText);
    ImGui::SetCursorPos(ImVec2((ImGui::GetWindowSize().x - bannerTextSize.x) * bannerCenterFactor,
                               (ImGui::GetWindowSize().y - bannerTextSize.y) * bannerCenterFactor));
    ImGui::TextColored(theme::imguiColor(theme::mutedText), "%s", bannerText);
    ImGui::EndChild();
    ImGui::Separator();

    ImGui::Spacing();
    ImGui::TextColored(theme::imguiColor(theme::brass), "%s", state.welcomeGreeting.c_str());
    ImGui::Spacing();
    ImGui::Separator();

    ImGui::Spacing();
    ImGui::TextUnformatted("Recent Files");
    ImGui::Spacing();

    if (state.session.recentMaps().empty())
    {
        ImGui::BeginChild("###welcome.recent-empty",
                          ImVec2(0.0F, recentRowHeight),
                          ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::TextDisabled("No files opened yet");
        ImGui::TextDisabled("Open a file to add it to this list.");
        ImGui::EndChild();
    }
    else
    {
        for (std::size_t index = 0; index < state.session.recentMaps().size(); ++index)
        {
            drawRecentMap(state, state.session.recentMaps().at(index), index);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextUnformatted("Profile & Status");
    if (ImGui::BeginTable("###welcome.details", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableNextColumn();
        drawProfileSummary(state.activeProfile);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Safe by default");
        ImGui::TextWrapped("Source muls are read-only. Edits remain staged until you explicitly save them to an output path.");
        ImGui::Spacing();
        ImGui::TextDisabled("Map readiness: %s", readinessLabel(state.session.profileState()));
        ImGui::EndTable();
    }

    const float footerHeight = actionButtonHeight + ImGui::GetStyle().ItemSpacing.y;
    const float availableHeight = ImGui::GetContentRegionAvail().y;
    if (availableHeight > footerHeight)
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + availableHeight - footerHeight);
    }
    ImGui::Separator();
    ImGui::Spacing();
    const float actionRowWidth = (actionButtonWidth * 2.0F) + actionButtonSpacing;
    const bool stackActions = ImGui::GetContentRegionAvail().x < actionRowWidth + actionButtonWidth;
    const float actionWidth = stackActions ? ImGui::GetContentRegionAvail().x : actionButtonWidth;
    if (ImGui::Button("Setup", ImVec2(actionWidth, actionButtonHeight)))
    {
        static_cast<void>(state.commands.execute(CommandId::dataPaths, state));
    }
    if (!stackActions)
    {
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - actionRowWidth + actionButtonSpacing);
    }
    if (ImGui::Button("Open Map", ImVec2(actionWidth, actionButtonHeight)))
    {
        static_cast<void>(state.commands.execute(CommandId::fileOpen, state));
    }
    if (!stackActions)
    {
        ImGui::SameLine();
    }
    if (ImGui::Button("Create Map", ImVec2(actionWidth, actionButtonHeight)))
    {
        static_cast<void>(state.commands.execute(CommandId::fileNew, state));
    }

    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace uo_mule::gui
