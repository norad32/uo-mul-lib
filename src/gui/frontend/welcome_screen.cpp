#include "gui/frontend/welcome_screen.h"
#include "core/facet_sizes.h"
#include "gui/application/workbench_state.h"
#include "gui/commands/command_registry.h"
#include "gui/context/notification_state.h"
#include "gui/dialogs/dialog_presentation.h"
#include "gui/dialogs/new_map_dialog.h"
#include "gui/documents/document_identity.h"
#include "gui/frontend/theme.h"
#include "gui/frontend/welcome_greetings.h"
#include "gui/metadata/document_contracts.h"
#include "gui/persistence/preferences.h"
#include "gui/persistence/recent_files.h"
#include "gui/profile/data_profile.h"
#include "gui/profile/data_validation.h"
#include "gui/profile/workbench_session.h"
#include "gui/providers/data_providers.h"
#include "gui/strings/ui_strings.h"
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
#include <utility>

namespace uo_mule::gui
{
namespace
{

constexpr float welcomeContentWidth = 720.0F;
constexpr float actionButtonSpacing = 8.0F;
constexpr float bannerHeight = 180.0F;
constexpr float bannerCenterFactor = 0.5F;
constexpr float verticalCenterFactor = 0.5F;
constexpr float recentRowHeight = 52.0F;
constexpr float unavailableRecentRowHeight = 68.0F;
constexpr float recentRowTextPadding = 8.0F;
constexpr float welcomeFooterBottomPadding = 16.0F;
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

[[nodiscard]] std::string dimensionsLabel(const RecentFile & recent)
{
    if (!recent.dimensions)
    {
        return "Dimensions unknown";
    }

    return std::to_string(recent.dimensions->widthTiles) + " x " + std::to_string(recent.dimensions->heightTiles);
}

[[nodiscard]] std::string mapLabel(const RecentFile & recent)
{
    if (recent.facet.empty())
    {
        return "Custom";
    }

    if (const auto facet = uo_mule::core::facetSize(recent.facet))
    {
        return facet->mapLabel;
    }

    std::string result = recent.facet;
    result.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(result.front())));
    return result;
}

void applyRecentMapToOpenDialog(NewMapDialogState & dialog, const RecentFile & recent)
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
    const auto facet = uo_mule::core::facetSize(recent.facet);
    if (facet)
    {
        int index = 1;
        for (const auto & knownFacet : uo_mule::core::knownFacetSizes())
        {
            if (knownFacet.name == facet->name)
            {
                dialog.openFacet = index;
                break;
            }
            index++;
        }
    }
}

[[nodiscard]] bool metadataFileConfigured(DataFileStatus status) noexcept
{
    return status == DataFileStatus::found || status == DataFileStatus::foundAndValidated ||
           status == DataFileStatus::overridden;
}

[[nodiscard]] bool metadataFileMissing(DataFileStatus status) noexcept
{
    return status == DataFileStatus::missing || status == DataFileStatus::unsupported;
}

void drawMetadataTooltip(const DataProfileValidation & validation)
{
    if (!ImGui::IsItemHovered())
    {
        return;
    }

    ImGui::BeginTooltip();
    for (const auto role : {DataFileRole::radarcol, DataFileRole::texidx, DataFileRole::texmaps})
    {
        const auto * file = validation.find(role);
        const DataFileStatus fileStatus = file == nullptr ? DataFileStatus::missing : file->status;
        const bool configured = metadataFileConfigured(fileStatus);
        const bool missing = metadataFileMissing(fileStatus);
        theme::Color color = theme::peach;
        const char * label = "incomplete";
        if (configured)
        {
            color = theme::success;
            label = "configured";
        }
        else if (missing)
        {
            color = theme::error;
            label = "not configured";
        }
        const std::string_view fileName = strings::dataFileName(role);
        ImGui::TextColored(theme::imguiColor(color), "%.*s: %s", static_cast<int>(fileName.size()), fileName.data(), label);
    }
    ImGui::EndTooltip();
}

void drawProfileSummary(const DataProfile & profile)
{
    const DataProfileValidation validation = validateDataProfile(profile);
    const std::string profileName = profile.name.empty() ? "No profile" : profile.name;
    ImGui::TextColored(theme::imguiColor(theme::lavender), "%s", profileName.c_str());

    const std::array<DataFileRole, 3> metadataRoles{DataFileRole::radarcol, DataFileRole::texidx, DataFileRole::texmaps};
    const bool anyConfigured = std::ranges::any_of(metadataRoles,
                                                   [&validation](DataFileRole role)
                                                   {
                                                       const auto * file = validation.find(role);
                                                       return file != nullptr && metadataFileConfigured(file->status);
                                                   });
    const bool allConfigured = std::ranges::all_of(metadataRoles,
                                                   [&validation](DataFileRole role)
                                                   {
                                                       const auto * file = validation.find(role);
                                                       return file != nullptr && metadataFileConfigured(file->status);
                                                   });
    const char * metadataLabel = "Not configured";
    theme::Color metadataColor = theme::error;
    if (allConfigured)
    {
        metadataLabel = "Configured";
        metadataColor = theme::success;
    }
    else if (anyConfigured)
    {
        metadataLabel = "Incomplete";
        metadataColor = theme::peach;
    }
    ImGui::TextUnformatted("Metadata: ");
    ImGui::SameLine(0.0F, 0.0F);
    ImGui::TextColored(theme::imguiColor(metadataColor), "%s", metadataLabel);
    drawMetadataTooltip(validation);
}

void openRecentAsset(WorkbenchState & state, const RecentFile & recent)
{
    DocumentSummary summary;
    summary.identity.kind = recent.kind;
    if (recent.kind == DocumentKind::texmapPair)
    {
        summary.identity.source = makeDocumentSourceIdentity({recent.path, recent.companionPath});
    }
    else
    {
        summary.identity.source = makeDocumentSourceIdentity({recent.path});
    }
    summary.displayName = recent.displayName;
    const DocumentId id = state.session.openDocument(std::move(summary));
    if (id == 0)
    {
        return;
    }
    const auto activated = state.session.activateDocument(id, state.workspace);
    if (!activated)
    {
        state.notifications.add(NotificationSeverity::error, activated.error().message);
    }
}

void drawRecentFile(WorkbenchState & state, const RecentFile & recent, std::size_t index)
{
    const bool unavailable = !recent.failure.empty();
    ImGui::PushID(static_cast<int>(index));
    ImGui::BeginChild("###welcome.recent-row",
                      ImVec2(0.0F, unavailable ? unavailableRecentRowHeight : recentRowHeight),
                      ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const float rowHeight = unavailable ? unavailableRecentRowHeight : recentRowHeight;
    const std::size_t lineCount = unavailable ? 3U : 2U;
    const float lineHeight = ImGui::GetTextLineHeight();
    const float contentHeight = (static_cast<float>(lineCount) * lineHeight) +
                                (static_cast<float>(lineCount - 1U) * ImGui::GetStyle().ItemSpacing.y);
    ImGui::SetCursorPosY(std::max(0.0F, (rowHeight - contentHeight) * verticalCenterFactor));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + recentRowTextPadding);
    const float textStartX = ImGui::GetCursorPosX();

    const bool rowHovered = ImGui::IsWindowHovered();
    if (rowHovered)
    {
        const ImVec2 rowMin = ImGui::GetWindowPos();
        const ImVec2 rowMax = ImVec2(rowMin.x + ImGui::GetWindowSize().x, rowMin.y + ImGui::GetWindowSize().y);
        const theme::Color rowColor =
            rowHovered && ImGui::IsMouseDown(ImGuiMouseButton_Left) ? theme::surface2 : theme::surface1;
        ImGui::GetWindowDrawList()->AddRectFilled(rowMin, rowMax, theme::packedColor(rowColor));
    }
    else
    {
        const ImVec2 rowMin = ImGui::GetWindowPos();
        const ImVec2 rowMax = ImVec2(rowMin.x + ImGui::GetWindowSize().x, rowMin.y + ImGui::GetWindowSize().y);
        ImGui::GetWindowDrawList()->AddRectFilled(rowMin, rowMax, theme::packedColor(theme::surface0));
    }
    if (rowHovered)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::SetTooltip("%s", recent.path.string().c_str());
    }

    ImGui::TextColored(theme::imguiColor(theme::lavender), "%s", recent.displayName.c_str());
    if (recent.kind == DocumentKind::map)
    {
        ImGui::SameLine();
        ImGui::TextUnformatted("Map");
        ImGui::SameLine();
        ImGui::TextUnformatted(mapLabel(recent).c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%s", dimensionsLabel(recent).c_str());
    }
    else
    {
        ImGui::SameLine();
        const std::string kindLabel(documentKindName(recent.kind));
        ImGui::TextUnformatted(kindLabel.c_str());
        if (recent.kind == DocumentKind::radarcol)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("%u entries", recent.itemCount);
        }
    }
    ImGui::SetCursorPosX(textStartX);
    const std::string modifiedLabel = timestampLabel("Modified", recent.lastModified);
    const std::string openedLabel = timestampLabel("Opened", recent.lastOpened);
    ImGui::TextDisabled("%s | %s", modifiedLabel.c_str(), openedLabel.c_str());
    if (unavailable)
    {
        ImGui::TextColored(theme::imguiColor(theme::error), "Unavailable: %s", recent.failure.c_str());
    }

    const bool rowDoubleClicked = rowHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    ImGui::EndChild();
    if (rowDoubleClicked)
    {
        if (recent.kind == DocumentKind::map)
        {
            applyRecentMapToOpenDialog(state.menuBarState.fileDialog, recent);
        }
        else
        {
            openRecentAsset(state, recent);
        }
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
    state.session.refreshRecentFiles();
    state.providers.sync(state.activeProfile);

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
    if (!ImGui::Begin("Welcome###welcome.map", nullptr, welcomeWindowFlags))
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
    ImGui::TextColored(theme::imguiColor(theme::peach), "%s", state.welcomeGreeting.c_str());
    ImGui::Spacing();
    ImGui::Separator();

    ImGui::Spacing();
    dialogHeading("Recent Files");
    ImGui::Spacing();

    if (state.session.recentFiles().empty())
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
        for (std::size_t index = 0; index < state.session.recentFiles().size(); index++)
        {
            drawRecentFile(state, state.session.recentFiles().at(index), index);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    dialogHeading("Profile");
    if (ImGui::BeginTable("###welcome.details", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableNextColumn();
        drawProfileSummary(state.activeProfile);
        ImGui::TableNextColumn();
        ImGui::EndTable();
    }

    if (ImGui::Button("Preferences"))
    {
        static_cast<void>(state.commands.execute(CommandId::dataPaths, state));
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Configure the active data profile and application preferences.");
    }

    const float actionButtonHeight = ImGui::GetFrameHeight();
    const float actionButtonTextWidth = std::max(ImGui::CalcTextSize("Open").x, ImGui::CalcTextSize("Create Map").x);
    const float actionButtonWidth = actionButtonTextWidth + (ImGui::GetStyle().FramePadding.x * 2.0F);
    const float footerHeight = actionButtonHeight + ImGui::GetStyle().ItemSpacing.y + welcomeFooterBottomPadding;
    const float availableHeight = ImGui::GetContentRegionAvail().y;
    if (availableHeight > footerHeight)
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + availableHeight - footerHeight);
    }
    ImGui::Separator();
    ImGui::Spacing();
    const float actionRowWidth = (actionButtonWidth * 2.0F) + actionButtonSpacing;
    const bool showWelcomeOnStartup = state.preferences.showWelcomeOnStartup;
    if (ImGui::Checkbox("Show on startup", &state.preferences.showWelcomeOnStartup) &&
        state.preferences.showWelcomeOnStartup != showWelcomeOnStartup)
    {
        savePreferences(state.preferences);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Show the welcome screen after a valid profile is loaded when the application starts.");
    }

    const float startupOptionWidth = ImGui::CalcTextSize("Show on startup").x + (ImGui::GetStyle().FramePadding.x * 2.0F);
    const bool stackActions = ImGui::GetContentRegionAvail().x < actionRowWidth + startupOptionWidth;
    const float actionWidth = stackActions ? ImGui::GetContentRegionAvail().x : actionButtonWidth;
    if (!stackActions)
    {
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - actionRowWidth);
    }
    if (ImGui::Button("Open", ImVec2(actionWidth, actionButtonHeight)))
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
    ImGui::Dummy(ImVec2(0.0F, welcomeFooterBottomPadding));

    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace uo_mule::gui
