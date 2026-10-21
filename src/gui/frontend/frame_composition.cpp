#include "gui/frontend/frame_composition.h"
#include "core/gui_log_sink.h"
#include "gui/application/workbench_state.h"
#include "gui/commands/menu_bar.h"
#include "gui/context/map_context.h"
#include "gui/context/notification_state.h"
#include "gui/documents/map_document.h"
#include "gui/documents/document_identity.h"
#include "gui/metadata/document_contracts.h"
#include "gui/dialogs/preferences_dialog.h"
#include "gui/documents/map_workspace.h"
#include "gui/frontend/welcome_screen.h"
#include "gui/frontend/theme.h"
#include "gui/profile/data_profile.h"
#include "gui/commands/command_registry.h"
#include "gui/persistence/preferences.h"
#include "gui/profile/workbench_session.h"
#include "gui/tools/edit_tools_panel.h"
#include "gui/views/block_inspector_panel.h"
#include "gui/views/log_panel.h"
#include "gui/views/map_canvas_panel.h"
#include "gui/views/map_info_panel.h"
#include "gui/views/panel_registry.h"
#include "gui/views/radarcol_panel.h"
#include "gui/views/selection_stats_panel.h"
#include "gui/views/texmaps_panel.h"
#include "gui/views/tile_inspector_panel.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <utility>
#include <string>
#include <string_view>
#include <span>
#include <optional>
#include <uomul/map.h>

namespace uo_mule::gui
{
namespace
{

constexpr float dockLeftWidthRatio = 0.20F;
constexpr float dockRightWidthRatio = 0.24F;
constexpr float statusBarHeight = 24.0F;
constexpr float documentTabBarHeight = 32.0F;
constexpr float notificationLeftMargin = 16.0F;
constexpr float notificationTopMargin = 48.0F;
constexpr float notificationMinimumWidth = 280.0F;
constexpr float notificationMaximumWidth = 480.0F;
constexpr float notificationMaximumHeightRatio = 0.5F;
constexpr std::size_t mapCanvasDockIndex = 0;
constexpr std::size_t tileInspectorDockIndex = 1;
constexpr std::size_t blockInspectorDockIndex = 2;
constexpr std::size_t selectionStatisticsDockIndex = 3;
constexpr std::size_t mapInformationDockIndex = 4;
constexpr std::size_t editToolsDockIndex = 5;
constexpr std::size_t texmapsDockIndex = 6;
constexpr std::size_t radarcolDockIndex = 7;
constexpr std::size_t shortcutCount = 6;
constexpr std::size_t graveShortcutIndex = 0;
constexpr std::size_t saveAsShortcutIndex = 1;
constexpr std::size_t openShortcutIndex = 2;
constexpr std::size_t newShortcutIndex = 3;
constexpr std::size_t quitShortcutIndex = 4;
constexpr std::size_t goToShortcutIndex = 5;

constexpr std::array<DockWindowDescriptor, 8> dockWindows{
        DockWindowDescriptor{.panel = PanelId::mapCanvas, .windowName = "Map Canvas###view.map-canvas"},
    DockWindowDescriptor{.panel = PanelId::tileInspector, .windowName = "Tile Inspector###view.tile-inspector"},
    DockWindowDescriptor{.panel = PanelId::blockInspector, .windowName = "Block Inspector###view.block-inspector"},
    DockWindowDescriptor{.panel = PanelId::selectionStatistics, .windowName = "Selection Statistics###view.selection-statistics"},
    DockWindowDescriptor{.panel = PanelId::mapInformation, .windowName = "Map Information###view.map-information"},
    DockWindowDescriptor{.panel = PanelId::editTools, .windowName = "Edit Tools###view.edit-tools"},
    DockWindowDescriptor{.panel = PanelId::texmaps, .windowName = "Texmaps###view.texmaps"},
    DockWindowDescriptor{.panel = PanelId::radarcol, .windowName = "Radarcol###view.radarcol"},
};

void drawDocumentTabs(WorkbenchState & state);

void drawDockSpace(WorkbenchState & state, float bottomPanelHeight)
{
    const ImGuiViewport * viewport = ImGui::GetMainViewport();
    const float dockSpaceHeight = std::max(0.0F, viewport->WorkSize.y - bottomPanelHeight - statusBarHeight);
    ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, dockSpaceHeight), ImGuiCond_Always);
    constexpr auto dockSpaceWindowFlags = static_cast<ImGuiWindowFlags>(
        static_cast<unsigned int>(ImGuiWindowFlags_NoDocking) | static_cast<unsigned int>(ImGuiWindowFlags_NoTitleBar) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoCollapse) | static_cast<unsigned int>(ImGuiWindowFlags_NoResize) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoMove) | static_cast<unsigned int>(ImGuiWindowFlags_NoBringToFrontOnFocus) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoNavFocus) | static_cast<unsigned int>(ImGuiWindowFlags_NoBackground) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoSavedSettings) | static_cast<unsigned int>(ImGuiWindowFlags_NoScrollbar) |
    static_cast<unsigned int>(ImGuiWindowFlags_NoScrollWithMouse));
    ImGui::Begin("BackgroundDockSpace###workbench.dock-space", nullptr, dockSpaceWindowFlags);
    drawDocumentTabs(state);
    ImGui::SetCursorPosY(documentTabBarHeight);
    const ImVec2 dockPosition = ImGui::GetCursorScreenPos();
    const float availableDockHeight = std::max(0.0F, viewport->WorkSize.y - bottomPanelHeight - statusBarHeight -
                                                        (dockPosition.y - viewport->WorkPos.y));
    constexpr ImGuiDockNodeFlags dockSpaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;
    const ImGuiID dockSpaceId = ImGui::GetID("BackgroundDockSpace");
    const ImVec2 expectedDockPosition = dockPosition;
    const ImGuiDockNode * existingDockNode = ImGui::DockBuilderGetNode(dockSpaceId);
    const bool dockPositionChanged = existingDockNode != nullptr &&
                                     (std::abs(existingDockNode->Pos.x - expectedDockPosition.x) > 0.5F ||
                                      std::abs(existingDockNode->Pos.y - expectedDockPosition.y) > 0.5F);
    if (state.layoutResetRequested || existingDockNode == nullptr || dockPositionChanged)
    {
        state.layoutResetRequested = false;
        ImGui::DockBuilderRemoveNode(dockSpaceId);
        ImGui::DockBuilderAddNode(dockSpaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodePos(dockSpaceId, expectedDockPosition);
        ImGui::DockBuilderSetNodeSize(dockSpaceId, ImVec2(viewport->WorkSize.x, availableDockHeight));
        ImGuiID leftNode = 0;
        ImGuiID centerNode = 0;
        ImGuiID rightNode = 0;
        ImGui::DockBuilderSplitNode(dockSpaceId, ImGuiDir_Left, dockLeftWidthRatio, &leftNode, &centerNode);
        ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Right, dockRightWidthRatio, &rightNode, &centerNode);
        ImGui::DockBuilderDockWindow(dockWindows.at(mapCanvasDockIndex).windowName, centerNode);
        ImGui::DockBuilderDockWindow(dockWindows.at(tileInspectorDockIndex).windowName, leftNode);
        ImGui::DockBuilderDockWindow(dockWindows.at(blockInspectorDockIndex).windowName, leftNode);
        ImGui::DockBuilderDockWindow(dockWindows.at(selectionStatisticsDockIndex).windowName, leftNode);
        ImGui::DockBuilderDockWindow(dockWindows.at(mapInformationDockIndex).windowName, leftNode);
        ImGui::DockBuilderDockWindow(dockWindows.at(editToolsDockIndex).windowName, rightNode);
        ImGui::DockBuilderDockWindow(dockWindows.at(texmapsDockIndex).windowName, rightNode);
        ImGui::DockBuilderDockWindow(dockWindows.at(radarcolDockIndex).windowName, rightNode);
        ImGui::DockBuilderFinish(dockSpaceId);
    }
    ImGui::DockSpace(dockSpaceId, ImVec2(viewport->WorkSize.x, availableDockHeight), dockSpaceFlags);
    ImGui::End();
}

void drawMapPanels(WorkbenchState & state,
                   const DataProfile & activeProfile,
                   MapCanvasPanelState & canvasState,
                   BlockInspectorPanelState & blockInspectorState,
                   SelectionStatsPanelState & selectionStatsState,
                   EditToolsPanelState & editToolsState,
                   RadarcolPanelState & radarcolState,
                   TexmapsPanelState & texmapsState)
{
    MapWorkspace & workspace = state.workspace;
    state.providers.sync(activeProfile);
    syncRadarcolPanel(radarcolState, activeProfile);
    syncTexmapsPanel(texmapsState, activeProfile);

    if (state.panelRegistry.find(PanelId::mapCanvas)->visible)
    {
        drawMapCanvasPanel(workspace,
                           radarcolState,
                           canvasState,
                           [&state] { static_cast<void>(state.commands.execute(CommandId::fileOpen, state)); },
                           [&state] { static_cast<void>(state.commands.execute(CommandId::fileNew, state)); },
                           [&state](const MapCanvasStatus & status) { state.lastHoveredCanvasStatus = status; },
                           state.panelRegistry.find(PanelId::mapCanvas)->visible);
    }
    else
    {
        workspace.clearHover();
    }
    const MapContextSnapshot context = state.session.contextSnapshot(workspace);

    if (state.panelRegistry.find(PanelId::tileInspector)->visible)
    {
        drawTileInspectorPanel(workspace, context, state.providers, state.panelRegistry.find(PanelId::tileInspector)->visible);
    }
    if (state.panelRegistry.find(PanelId::blockInspector)->visible)
    {
        drawBlockInspectorPanel(workspace, context, state.providers, blockInspectorState, state.panelRegistry.find(PanelId::blockInspector)->visible);
    }
    if (state.panelRegistry.find(PanelId::selectionStatistics)->visible)
    {
        drawSelectionStatsPanel(workspace, context, selectionStatsState, state.panelRegistry.find(PanelId::selectionStatistics)->visible);
    }
    if (state.panelRegistry.find(PanelId::mapInformation)->visible)
    {
        drawMapInfoPanel(workspace, state.panelRegistry.find(PanelId::mapInformation)->visible);
    }
    if (state.panelRegistry.find(PanelId::editTools)->visible)
    {
        drawEditToolsPanel(workspace, editToolsState, state.panelRegistry.find(PanelId::editTools)->visible);
    }
    if (state.panelRegistry.find(PanelId::radarcol)->visible)
    {
        drawRadarcolPanel(radarcolState, workspace.editMode(), state.panelRegistry.find(PanelId::radarcol)->visible, state.fileBrowser);
    }
    if (state.panelRegistry.find(PanelId::texmaps)->visible)
    {
        drawTexmapsPanel(texmapsState, state.panelRegistry.find(PanelId::texmaps)->visible, state.fileBrowser);
    }
}

[[nodiscard]] const char * readinessLabel(ProfileReadinessState state) noexcept
{
    switch (state)
    {
        case ProfileReadinessState::needsProfile:
            return "No profile configured";
        case ProfileReadinessState::profileInvalid:
            return "Profile invalid";
        case ProfileReadinessState::profileIncomplete:
            return "Profile incomplete: radarcol.mul required";
        case ProfileReadinessState::ready:
            return "Ready";
    }
    return "Unknown";
}

void drawStatusBar(const WorkbenchState & state, float bottomPanelHeight)
{
    const ImGuiViewport * viewport = ImGui::GetMainViewport();
    const float statusY = viewport->WorkPos.y + viewport->WorkSize.y - bottomPanelHeight - statusBarHeight;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, statusY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, statusBarHeight), ImGuiCond_Always);
    constexpr auto statusFlags = static_cast<ImGuiWindowFlags>(
        static_cast<unsigned int>(ImGuiWindowFlags_NoDecoration) | static_cast<unsigned int>(ImGuiWindowFlags_NoDocking) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoMove) | static_cast<unsigned int>(ImGuiWindowFlags_NoResize) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoSavedSettings) | static_cast<unsigned int>(ImGuiWindowFlags_NoScrollbar));
    ImGui::Begin("Workbench Status###workbench.status", nullptr, statusFlags);
    bool hasCanvasStatus = false;
    if (state.lastHoveredCanvasStatus)
    {
        hasCanvasStatus = true;
        const MapCanvasStatus & canvasStatus = *state.lastHoveredCanvasStatus;
        if (!canvasStatus.hasDocument || !canvasStatus.tile)
        {
            ImGui::Text("%s: No tile under pointer  Zoom %.3gx", canvasStatus.canvasId.c_str(), canvasStatus.zoom);
        }
        else
        {
            const uomul::BlockCoord block = tileToBlock(*canvasStatus.tile);
            ImGui::Text("%s: Tile (%u, %u)  Block (%u, %u)  Zoom %.3gx",
                        canvasStatus.canvasId.c_str(), canvasStatus.tile->x, canvasStatus.tile->y, block.x, block.y, canvasStatus.zoom);
        }
    }
    if (hasCanvasStatus)
    {
        ImGui::SameLine();
    }
    const std::string profileStatus = "Profile: " + (state.activeProfile.name.empty() ? std::string("None") : state.activeProfile.name);
    const std::string mapStatus = "Map: " + (state.workspace.reader() == nullptr ? std::string("None") : state.workspace.mapPath().string());
    const std::string stateStatus = "State: " + std::string(state.workspace.dirty() ? "Staged changes" : "Clean");
    const std::string readinessStatus = "Data: " + std::string(readinessLabel(state.session.profileState()));
    const float rightContentWidth = ImGui::CalcTextSize(profileStatus.c_str()).x + ImGui::GetStyle().ItemSpacing.x +
                                    ImGui::CalcTextSize(mapStatus.c_str()).x + ImGui::GetStyle().ItemSpacing.x +
                                    ImGui::CalcTextSize(stateStatus.c_str()).x + ImGui::GetStyle().ItemSpacing.x +
                                    ImGui::CalcTextSize(readinessStatus.c_str()).x;
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - rightContentWidth));
    ImGui::TextUnformatted(profileStatus.c_str());
    ImGui::SameLine();
    ImGui::TextUnformatted(mapStatus.c_str());
    ImGui::SameLine();
    ImGui::TextUnformatted(stateStatus.c_str());
    ImGui::SameLine();
    ImGui::TextUnformatted(readinessStatus.c_str());
    ImGui::End();
}

void drawNotifications(const NotificationState & notifications)
{
    if (notifications.entries.empty())
    {
        return;
    }
    const ImGuiViewport * viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + notificationLeftMargin, viewport->WorkPos.y + notificationTopMargin), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(notificationMinimumWidth, 0.0F),
                                        ImVec2(notificationMaximumWidth, viewport->WorkSize.y * notificationMaximumHeightRatio));
    if (ImGui::Begin("Notifications###workbench.notifications", nullptr,
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        for (const auto & notification : notifications.entries)
        {
            ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
            if (notification.severity == NotificationSeverity::error)
            {
                color = theme::imguiColor(theme::coral);
            }
            else if (notification.severity == NotificationSeverity::warning)
            {
                color = theme::imguiColor(theme::amber);
            }
            else if (notification.severity == NotificationSeverity::success)
            {
                color = theme::imguiColor(theme::sageGreen);
            }
            ImGui::TextColored(color, "%s", notification.message.c_str());
        }
    }
    ImGui::End();
}

void drawStartupScreen(WorkbenchState & state)
{
    const ImGuiViewport * viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);
    if (!ImGui::Begin("Configure Preferences###startup.configure-profile", nullptr,
                      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::End();
        return;
    }
    ImGui::TextUnformatted("Configure Preferences");
    ImGui::Separator();
    ImGui::TextUnformatted("A validated radarcol.mul is required before map actions are enabled.");
    ImGui::Text("Profile status: %s", readinessLabel(state.session.profileState()));
    if (ImGui::Button("Open Preferences"))
    {
        requestPreferencesDialog(state.menuBarState.preferencesDialog, state.activeProfile, state.preferences);
    }
    ImGui::End();
}

void drawEmptyDocumentState(const WorkbenchState & state)
{
    const ImGuiViewport * viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + documentTabBarHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - documentTabBarHeight), ImGuiCond_Always);
    if (!ImGui::Begin("Document###workbench.empty-document", nullptr,
                      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::End();
        return;
    }

    if (!state.session.hasDocuments())
    {
        ImGui::TextUnformatted("No document is open");
        ImGui::TextUnformatted("Use File > Open... or File > Create Map... to begin.");
    }
    else if (const auto active = state.session.activeDocumentIdentity())
    {
        const std::string kindName(documentKindName(active->kind));
        ImGui::Text("%s is active, but has no map view yet.", kindName.c_str());
        ImGui::TextUnformatted("This document type is represented as a placeholder until its view is implemented.");
    }
    ImGui::End();
}

bool drawDocumentTab(WorkbenchState & state, const DocumentRecord & document, std::optional<DocumentId> activeId)
{
    bool open = true;
    const std::string label = documentTabLabel(document.summary);
    const bool selected = activeId && *activeId == document.summary.identity.id;
    const bool visible = ImGui::BeginTabItem(label.c_str(), &open, selected ? ImGuiTabItemFlags_SetSelected : 0);
    if (visible)
    {
        if (!selected)
        {
            const auto activated = state.session.activateDocument(document.summary.identity.id, state.workspace);
            if (!activated)
            {
                state.notifications.add(NotificationSeverity::error, activated.error().message);
            }
        }
        ImGui::EndTabItem();
    }
    if (open)
    {
        return false;
    }

    if (document.summary.dirty)
    {
        if (!selected)
        {
            const auto activated = state.session.activateDocument(document.summary.identity.id, state.workspace);
            if (!activated)
            {
                state.notifications.add(NotificationSeverity::error, activated.error().message);
                return true;
            }
        }
        static_cast<void>(state.commands.execute(CommandId::fileClose, state));
        return true;
    }

    const auto closed = state.session.closeDocument(document.summary.identity.id, state.workspace);
    if (!closed)
    {
        state.notifications.add(NotificationSeverity::error, closed.error().message);
    }
    return true;
}

void drawDocumentTabs(WorkbenchState & state)
{
    if (ImGui::BeginTabBar("###workbench.document-tab-bar", ImGuiTabBarFlags_Reorderable))
    {
        const auto activeId = state.session.activeDocumentId();
        for (const auto & document : state.session.documents())
        {
            if (drawDocumentTab(state, document, activeId))
            {
                break;
            }
        }
        ImGui::EndTabBar();
    }
}

void dispatchShortcuts(WorkbenchState & state)
{
    if (ImGui::GetIO().WantTextInput || ImGui::GetIO().WantCaptureKeyboard)
    {
        return;
    }
    const std::array<std::pair<ImGuiKey, std::string_view>, shortcutCount> shortcuts{
        std::pair{ImGuiKey_GraveAccent, "`"},
        std::pair{ImGuiKey_S, "Ctrl+Shift+S"},
        std::pair{ImGuiKey_O, "Ctrl+O"},
        std::pair{ImGuiKey_N, "Ctrl+N"},
        std::pair{ImGuiKey_Q, "Ctrl+Q"},
        std::pair{ImGuiKey_G, "Ctrl+G"},
    };
    if (ImGui::IsKeyPressed(shortcuts.at(graveShortcutIndex).first, false))
    {
        static_cast<void>(state.commands.executeShortcut(shortcuts.at(graveShortcutIndex).second, state));
    }
    else if (ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(shortcuts.at(saveAsShortcutIndex).first, false))
    {
        static_cast<void>(state.commands.executeShortcut(shortcuts.at(saveAsShortcutIndex).second, state));
    }
    else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(shortcuts.at(openShortcutIndex).first, false))
    {
        static_cast<void>(state.commands.executeShortcut(shortcuts.at(openShortcutIndex).second, state));
    }
    else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(shortcuts.at(newShortcutIndex).first, false))
    {
        static_cast<void>(state.commands.executeShortcut(shortcuts.at(newShortcutIndex).second, state));
    }
    else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(shortcuts.at(quitShortcutIndex).first, false))
    {
        static_cast<void>(state.commands.executeShortcut(shortcuts.at(quitShortcutIndex).second, state));
    }
    else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(shortcuts.at(goToShortcutIndex).first, false))
    {
        static_cast<void>(state.commands.executeShortcut(shortcuts.at(goToShortcutIndex).second, state));
    }
}

} // namespace

std::span<const DockWindowDescriptor> defaultDockWindows() noexcept
{
    return dockWindows;
}

std::string documentTabLabel(const DocumentSummary & summary)
{
    std::string label(documentKindName(summary.identity.kind));
    label += ": ";
    label += summary.displayName.empty() ? "Untitled" : summary.displayName;
    if (summary.dirty)
    {
        label += " *";
    }
    if (summary.access == DocumentAccess::readOnly)
    {
        label += " [Read-only]";
    }
    return label;
}

void drawWorkbenchFrame(GLFWwindow * window, core::GuiLogSink & logSink, WorkbenchState & state)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    bool welcomeShell = state.session.startupState() == StartupState::mapWelcome && state.workspace.reader() == nullptr &&
                        !state.session.hasDocuments();
    bool profileChanged = welcomeShell ? false : drawMenuBar(state, true);

    if (!state.pendingClipboardText.empty())
    {
        glfwSetClipboardString(window, state.pendingClipboardText.c_str());
        state.pendingClipboardText.clear();
    }
    if (profileChanged)
    {
        state.preferences.activeDataProfile = state.activeProfile.name;
        savePreferences(state.preferences);
        state.session.updateProfile(state.activeProfile);
        if (state.session.mapReady() && !state.preferences.showWelcomeOnStartup)
        {
            state.session.setStartupState(StartupState::workbench);
        }
        if (state.workspace.reader() == nullptr)
        {
            state.welcomeGreetingSelected = false;
            state.welcomeGreeting.clear();
        }
    }

    welcomeShell = state.session.startupState() == StartupState::mapWelcome && state.workspace.reader() == nullptr;
    if (state.nativeWelcomeWindowMode != welcomeShell)
    {
        glfwSetWindowAttrib(window, GLFW_DECORATED, welcomeShell ? GLFW_FALSE : GLFW_TRUE);
        glfwSetWindowAttrib(window, GLFW_RESIZABLE, welcomeShell ? GLFW_FALSE : GLFW_TRUE);
        state.nativeWelcomeWindowMode = welcomeShell;
    }
    dispatchShortcuts(state);

    const float bottomPanelHeight = welcomeShell ? 0.0F : drawLogPanel(logSink, state.preferences, state.logEntries, state.followLogScroll);
    const bool noMapLoaded = state.workspace.reader() == nullptr;
    state.session.syncDocument(state.workspace);
    if (state.session.startupState() == StartupState::configureProfile)
    {
        drawStartupScreen(state);
    }
    else if (welcomeShell)
    {
        state.recentMapRecordedPath.clear();
        drawWelcomeScreen(state);
        profileChanged = drawMenuBar(state, false);
        static_cast<void>(profileChanged);
    }
    else
    {
        state.welcomeGreetingSelected = false;
        state.welcomeGreeting.clear();
        drawDockSpace(state, bottomPanelHeight);
        if (state.workspace.reader() != nullptr)
        {
            drawMapPanels(state,
                          state.activeProfile,
                          state.mapCanvasState,
                          state.blockInspectorState,
                          state.selectionStatsState,
                          state.editToolsState,
                          state.radarcolState,
                          state.texmapsState);
        }
        else
        {
            drawEmptyDocumentState(state);
        }
        if (state.recentMapRecordedPath != state.workspace.mapPath())
        {
            state.session.recordRecentMap(state.workspace.mapPath(), state.workspace.dimensions());
            state.recentMapRecordedPath = state.workspace.mapPath();
        }
    }
    if (!noMapLoaded)
    {
        drawStatusBar(state, bottomPanelHeight);
    }
    drawNotifications(state.notifications);
}

} // namespace uo_mule::gui
