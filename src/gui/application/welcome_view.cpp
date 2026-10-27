#include "gui/application/welcome_view.h"
#include "core/app_info.h"
#include "gui/application/gui_resources.h"
#include "gui/application/welcome_greetings.h"
#include "gui/commands/command_registry.h"
#include "gui/dialogs/map_workflow_dialog.h"
#include "gui/presentation/dialog_presentation.h"
#include "gui/presentation/iconography.h"
#include "gui/presentation/strings.h"
#include "gui/presentation/theme.h"
#include "gui/workbench/workbench_state.h"
#include "gui/workflows/file_workflow.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/persistence/recent_files.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/profile/data_validation.h"
#include "gui_model/session/workbench_session.h"
#include "image_codec/image_codec.h"
#include "uomul/facet_sizes.h"
#include <GL/gl.h>
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

constexpr float welcomeContentWidth = 980.0F;
constexpr float actionButtonSpacing = 8.0F;
constexpr float fallbackBannerHeight = 180.0F;
constexpr float verticalCenterFactor = 0.5F;
constexpr float recentRowHeight = 52.0F;
constexpr float unavailableRecentRowHeight = 68.0F;
constexpr float recentRowTextPadding = 8.0F;
constexpr float welcomeFooterBottomPadding = 16.0F;
constexpr std::size_t timestampBufferSize = 32;

struct WelcomeBannerTexture
{
    GLuint handle = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool attempted = false;
};

[[nodiscard]] WelcomeBannerTexture & welcomeBannerTexture()
{
    static WelcomeBannerTexture texture;
    return texture;
}

void loadWelcomeBannerTexture()
{
    auto & texture = welcomeBannerTexture();
    if (texture.attempted)
    {
        return;
    }
    texture.attempted = true;

    const auto path = defaultGuiResourceLookup().find(GuiResourceId::welcomeBanner);
    if (!path)
    {
        return;
    }
    const auto image = image::decode(*path);
    if (!image || image->pixels.empty())
    {
        return;
    }

    glGenTextures(1, &texture.handle);
    if (texture.handle == 0U)
    {
        return;
    }
    GLint previousTexture = 0;
    GLint previousUnpackAlignment = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
    glBindTexture(GL_TEXTURE_2D, texture.handle);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGB,
                 static_cast<GLsizei>(image->width),
                 static_cast<GLsizei>(image->height),
                 0,
                 GL_RGB,
                 GL_UNSIGNED_BYTE,
                 image->pixels.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    texture.width = image->width;
    texture.height = image->height;
}

[[nodiscard]] float welcomeBannerHeight(float width)
{
    loadWelcomeBannerTexture();
    const auto & texture = welcomeBannerTexture();
    if (texture.width == 0U || texture.height == 0U || width <= 0.0F)
    {
        return fallbackBannerHeight;
    }

    return width * static_cast<float>(texture.height) / static_cast<float>(texture.width);
}

void drawWelcomeBanner()
{
    loadWelcomeBannerTexture();
    const auto & texture = welcomeBannerTexture();
    const ImVec2 targetSize = ImGui::GetContentRegionAvail();
    if (texture.handle == 0U || texture.width == 0U || texture.height == 0U || targetSize.x <= 0.0F || targetSize.y <= 0.0F)
    {
        ImGui::Dummy(targetSize);
        return;
    }

    const float imageHeight = targetSize.x * static_cast<float>(texture.height) / static_cast<float>(texture.width);
    ImGui::Image(static_cast<ImTextureID>(texture.handle), ImVec2(targetSize.x, imageHeight));
}

[[nodiscard]] std::string timestampLabel(std::string_view prefix, std::int64_t timestamp)
{
    if (timestamp <= 0)
    {
        return strings::formatTimestamp(prefix, strings::unknown());
    }

    const std::time_t time = static_cast<std::time_t>(timestamp);
    std::tm localTime{};
    if (localtime_r(&time, &localTime) == nullptr)
    {
        return strings::formatTimestamp(prefix, strings::unknown());
    }

    std::array<char, timestampBufferSize> formatted{};
    if (std::strftime(formatted.data(), formatted.size(), "%Y-%m-%d %H:%M", &localTime) == 0U)
    {
        return strings::formatTimestamp(prefix, strings::unknown());
    }
    return strings::formatTimestamp(prefix, formatted.data());
}

[[nodiscard]] std::string dimensionsLabel(const RecentFile & recent)
{
    if (!recent.dimensions)
    {
        return strings::dimensionsUnknown();
    }

    return strings::formatDimensions(recent.dimensions->widthTiles, recent.dimensions->heightTiles);
}

[[nodiscard]] std::string mapLabel(const RecentFile & recent)
{
    if (recent.facet.empty())
    {
        return strings::customMap();
    }

    if (const auto facet = uomul::facetSize(recent.facet))
    {
        return facet->mapLabel;
    }

    std::string result = recent.facet;
    result.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(result.front())));
    return result;
}

[[nodiscard]] std::string recentDetails(const RecentFile & recent)
{
    if (recent.dimensions)
    {
        return mapLabel(recent) + " " + dimensionsLabel(recent);
    }
    if (recent.itemCount > 0U)
    {
        return strings::formatEntries(recent.itemCount);
    }
    return {};
}

void applyRecentMapToOpenDialog(NewFileDialogState & dialog, const RecentFile & recent)
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
    const auto facet = uomul::facetSize(recent.facet);
    if (facet)
    {
        int index = 1;
        for (const auto & knownFacet : uomul::knownFacetSizes())
        {
            if (knownFacet.name == facet->name)
            {
                dialog.openFacet = index;
                break;
            }
            index++;
        }
    }
    requestMapDialog(dialog, MapFileDialogMode::openMap);
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
    for (const auto role : knownDataFileRoles())
    {
        const auto * file = validation.find(role);
        const DataFileStatus fileStatus = file == nullptr ? DataFileStatus::missing : file->status;
        const bool configured = metadataFileConfigured(fileStatus);
        const bool missing = metadataFileMissing(fileStatus);
        theme::Color color = theme::info;
        if (configured)
        {
            color = theme::success;
        }
        else if (missing)
        {
            color = theme::error;
        }
        const std::string_view fileName = strings::dataFileName(role);
        ImGui::TextColored(theme::imguiColor(color), "%.*s", static_cast<int>(fileName.size()), fileName.data());
    }
    ImGui::EndTooltip();
}

void drawProfileSummary(const DataProfile & profile)
{
    const DataProfileValidation validation = validateDataProfile(profile);
    const std::string profileName = profile.name.empty() ? strings::noProfile() : profile.name;
    ImGui::TextColored(theme::imguiColor(theme::subheading), "%s", profileName.c_str());

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
    const char * metadataLabel = strings::notConfigured();
    theme::Color metadataColor = theme::error;
    if (allConfigured)
    {
        metadataLabel = strings::configured();
        metadataColor = theme::success;
    }
    else if (anyConfigured)
    {
        metadataLabel = strings::incomplete();
        metadataColor = theme::info;
    }
    ImGui::TextColored(theme::imguiColor(metadataColor), "%s", metadataLabel);
    drawMetadataTooltip(validation);
}

void openRecentAsset(WorkbenchState & state, const RecentFile & recent)
{
    static_cast<void>(
        openStandaloneAsset(state,
                            recent.kind,
                            recent.path,
                            recent.companionPath,
                            recent.failure.empty() ? DocumentValidationState::valid : DocumentValidationState::invalid,
                            recent.itemCount));
}

void activateRecentFile(WorkbenchState & state, const RecentFile & recent)
{
    if (recent.kind == DocumentKind::map)
    {
        applyRecentMapToOpenDialog(state.menuBarState.fileDialog, recent);
        return;
    }

    state.deferWorkflowAction([&state, recent] { openRecentAsset(state, recent); });
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
    const ImVec2 rowStart = ImGui::GetCursorPos();
    [&]
    {
        static_cast<void>(
            ImGui::InvisibleButton("##welcome.recent.activate", ImVec2(ImGui::GetContentRegionAvail().x, rowHeight)));
        if (ImGui::IsItemFocused())
        {
            state.welcomeRecentKeyboardSelection = index;
        }
    }();
    ImGui::SetCursorPos(rowStart);
    const std::size_t lineCount = unavailable ? 3U : 2U;
    const float lineHeight = ImGui::GetTextLineHeight();
    const float contentHeight =
        (static_cast<float>(lineCount) * lineHeight) + (static_cast<float>(lineCount - 1U) * ImGui::GetStyle().ItemSpacing.y);
    ImGui::SetCursorPosY(std::max(0.0F, (rowHeight - contentHeight) * verticalCenterFactor));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + recentRowTextPadding);
    const float textStartX = ImGui::GetCursorPosX();

    const bool rowHovered = ImGui::IsWindowHovered();
    if (rowHovered)
    {
        const ImVec2 rowMin = ImGui::GetWindowPos();
        const ImVec2 rowMax = ImVec2(rowMin.x + ImGui::GetWindowSize().x, rowMin.y + ImGui::GetWindowSize().y);
        const theme::Color rowColor =
            rowHovered && ImGui::IsMouseDown(ImGuiMouseButton_Left) ? theme::activeSurface : theme::hoveredSurface;
        ImGui::GetWindowDrawList()->AddRectFilled(rowMin, rowMax, theme::packedColor(rowColor));
    }
    else if (state.welcomeRecentKeyboardSelection == index)
    {
        const ImVec2 rowMin = ImGui::GetWindowPos();
        const ImVec2 rowMax = ImVec2(rowMin.x + ImGui::GetWindowSize().x, rowMin.y + ImGui::GetWindowSize().y);
        ImGui::GetWindowDrawList()->AddRectFilled(rowMin, rowMax, theme::packedColor(theme::activeSurface));
    }
    else
    {
        const ImVec2 rowMin = ImGui::GetWindowPos();
        const ImVec2 rowMax = ImVec2(rowMin.x + ImGui::GetWindowSize().x, rowMin.y + ImGui::GetWindowSize().y);
        ImGui::GetWindowDrawList()->AddRectFilled(rowMin, rowMax, theme::packedColor(theme::panelBackground));
    }
    if (rowHovered)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::SetTooltip("%s", recent.path.string().c_str());
    }

    ImGui::TextColored(theme::imguiColor(theme::subheading), "%s", recent.displayName.c_str());
    ImGui::SameLine();
    const std::string kindLabel(documentKindName(recent.kind));
    ImGui::TextUnformatted(kindLabel.c_str());
    const std::string details = recentDetails(recent);
    if (!details.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", details.c_str());
    }
    ImGui::SetCursorPosX(textStartX);
    const std::string modifiedLabel = timestampLabel(strings::modified(), recent.lastModified);
    const std::string openedLabel = timestampLabel(strings::opened(), recent.lastOpened);
    ImGui::TextDisabled("%s | %s", modifiedLabel.c_str(), openedLabel.c_str());
    if (unavailable)
    {
        const std::string unavailableLabel = strings::formatUnavailable(recent.failure);
        ImGui::TextColored(theme::imguiColor(theme::error), "%s", unavailableLabel.c_str());
    }

    const bool rowDoubleClicked = rowHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    ImGui::EndChild();
    if (rowDoubleClicked)
    {
        activateRecentFile(state, recent);
    }
    ImGui::PopID();
}

void updateRecentKeyboardSelection(WorkbenchState & state, std::size_t recentCount)
{
    if (recentCount == 0U || !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
        state.welcomeRecentKeyboardSelection.reset();
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Tab, false) && !state.welcomeRecentKeyboardSelection)
    {
        state.welcomeRecentKeyboardSelection = 0U;
    }
    else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false))
    {
        const std::size_t current = state.welcomeRecentKeyboardSelection.value_or(0U);
        state.welcomeRecentKeyboardSelection = std::min(current + 1U, recentCount - 1U);
    }
    else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false))
    {
        const std::size_t current = state.welcomeRecentKeyboardSelection.value_or(0U);
        state.welcomeRecentKeyboardSelection = current == 0U ? 0U : current - 1U;
    }
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

void drawWelcomeView(WorkbenchState & state)
{
    selectGreetingIfNeeded(state);

    const ImGuiViewport * viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(welcomeWindowWidth, welcomeWindowHeight), ImGuiCond_Always);
    constexpr auto welcomeWindowFlags = static_cast<ImGuiWindowFlags>(
        static_cast<unsigned int>(ImGuiWindowFlags_NoTitleBar) | static_cast<unsigned>(ImGuiWindowFlags_NoResize) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoDecoration) | static_cast<unsigned int>(ImGuiWindowFlags_NoBackground) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoScrollbar) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoScrollWithMouse) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoSavedSettings));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
    const std::string windowTitle = std::string(strings::welcome()) + "###welcome.map";
    if (!ImGui::Begin(windowTitle.c_str(), nullptr, welcomeWindowFlags))
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
                      ImVec2(0.0F, welcomeBannerHeight(contentWidth)),
                      ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    drawWelcomeBanner();
    ImGui::EndChild();
    ImGui::Separator();

    ImGui::Spacing();
    ImGui::TextColored(theme::imguiColor(theme::peach), "%s", state.welcomeGreeting.c_str());
    ImGui::Spacing();
    ImGui::Separator();

    ImGui::Spacing();
    dialogHeading(iconLabel(IconId::bookmark, strings::recentFiles()).c_str());
    ImGui::Spacing();

    updateRecentKeyboardSelection(state, state.session.recentFiles().size());

    if (state.session.recentFiles().empty())
    {
        ImGui::BeginChild("###welcome.recent-empty",
                          ImVec2(0.0F, recentRowHeight),
                          ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::TextDisabled("%s", strings::noFilesOpened());
        ImGui::TextDisabled("%s", strings::openFileToAdd());
        ImGui::EndChild();
    }
    else
    {
        for (std::size_t index = 0; index < state.session.recentFiles().size(); index++)
        {
            drawRecentFile(state, state.session.recentFiles().at(index), index);
        }

        if (state.welcomeRecentKeyboardSelection && ImGui::IsKeyPressed(ImGuiKey_Enter, false))
        {
            const std::size_t index = *state.welcomeRecentKeyboardSelection;
            if (index < state.session.recentFiles().size())
            {
                activateRecentFile(state, state.session.recentFiles().at(index));
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    dialogHeading(iconLabel(IconId::account, strings::profile()).c_str());
    if (ImGui::BeginTable("###welcome.details", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableNextColumn();
        drawProfileSummary(state.activeProfile);
        ImGui::TableNextColumn();
        const ImVec2 separatorStart = ImGui::GetCursorScreenPos();
        const float separatorHeight = ImGui::GetTextLineHeightWithSpacing();
        ImGui::Dummy(ImVec2(1.0F, separatorHeight));
        ImGui::GetWindowDrawList()->AddLine(separatorStart,
                                            ImVec2(separatorStart.x, separatorStart.y + separatorHeight),
                                            theme::packedOpaqueColor(theme::border));
        ImGui::SameLine();
        const std::string versionLabel = strings::formatVersion(core::applicationVersion());
        ImGui::TextColored(theme::imguiColor(theme::mauve), "%s", versionLabel.c_str());
        ImGui::EndTable();
    }

    const std::string preferencesLabel = iconLabel(IconId::preferences, strings::preferences());
    if (ImGui::Button(preferencesLabel.c_str()))
    {
        state.deferWorkflowAction([&state] { static_cast<void>(state.commands.execute(CommandId::dataPaths, state)); });
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", strings::configurePreferences());
    }

    const float actionButtonHeight = ImGui::GetFrameHeight();
    const std::string openLabel = iconLabel(IconId::folderOpen, strings::openEllipsis());
    const std::string newLabel = iconLabel(IconId::fileDocumentPlus, strings::createNewEllipsis());
    const float actionButtonTextWidth =
        std::max(ImGui::CalcTextSize(openLabel.c_str()).x, ImGui::CalcTextSize(newLabel.c_str()).x);
    const float actionButtonWidth = actionButtonTextWidth + (ImGui::GetStyle().FramePadding.x * 2.0F);
    ImGui::Separator();
    ImGui::Spacing();
    const float actionRowWidth = (actionButtonWidth * 2.0F) + actionButtonSpacing;
    bool showWelcomeOnStartup = state.preferences.showWelcomeOnStartup;
    if (ImGui::Checkbox(strings::showOnStartup(), &showWelcomeOnStartup) &&
        showWelcomeOnStartup != state.preferences.showWelcomeOnStartup)
    {
        state.pendingPreferenceChanges.showWelcomeOnStartup = showWelcomeOnStartup;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", strings::showOnStartupTooltip());
    }

    const float startupOptionWidth =
        ImGui::CalcTextSize(strings::showOnStartup()).x + (ImGui::GetStyle().FramePadding.x * 2.0F);
    const bool stackActions = ImGui::GetContentRegionAvail().x < actionRowWidth + startupOptionWidth;
    const float actionWidth = stackActions ? ImGui::GetContentRegionAvail().x : actionButtonWidth;
    if (!stackActions)
    {
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - actionRowWidth);
    }
    if (ImGui::Button(openLabel.c_str(), ImVec2(actionWidth, actionButtonHeight)))
    {
        state.deferWorkflowAction([&state] { static_cast<void>(state.commands.execute(CommandId::fileOpen, state)); });
    }
    if (!stackActions)
    {
        ImGui::SameLine();
    }
    if (ImGui::Button(newLabel.c_str(), ImVec2(actionWidth, actionButtonHeight)))
    {
        state.deferWorkflowAction([&state] { static_cast<void>(state.commands.execute(CommandId::fileNew, state)); });
    }
    ImGui::Dummy(ImVec2(0.0F, welcomeFooterBottomPadding));

    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace uo_mule::gui
