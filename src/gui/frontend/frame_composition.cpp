#include "gui/frontend/frame_composition.h"
#include "gui/frontend/document_surface.h"
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
#include "gui/strings/ui_strings.h"
#include "gui/profile/data_profile.h"
#include "gui/commands/command_registry.h"
#include "gui/persistence/preferences.h"
#include "gui/profile/workbench_session.h"
#include "gui/providers/data_providers.h"
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
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <utility>
#include <string>
#include <string_view>
#include <optional>
#include <uomul/map.h>

namespace uo_mule::gui
{
namespace
{

constexpr float dockLeftWidthRatio = 0.20F;
constexpr float dockRightWidthRatio = 0.24F;
constexpr float statusBarHeight = 24.0F;
constexpr float notificationLeftMargin = 16.0F;
constexpr float notificationTopMargin = 48.0F;
constexpr float notificationMinimumWidth = 280.0F;
constexpr float notificationMaximumWidth = 480.0F;
constexpr float notificationMaximumHeightRatio = 0.5F;
constexpr std::size_t shortcutCount = 6;
constexpr std::size_t graveShortcutIndex = 0;
constexpr std::size_t saveAsShortcutIndex = 1;
constexpr std::size_t openShortcutIndex = 2;
constexpr std::size_t newShortcutIndex = 3;
constexpr std::size_t quitShortcutIndex = 4;
constexpr std::size_t goToShortcutIndex = 5;

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
    const ImVec2 dockPosition(viewport->WorkPos.x, viewport->WorkPos.y);
    const float availableDockHeight = std::max(0.0F, viewport->WorkSize.y - bottomPanelHeight - statusBarHeight -
                                                         (dockPosition.y - viewport->WorkPos.y));
    constexpr ImGuiDockNodeFlags dockSpaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;
    const ImGuiID dockSpaceId = ImGui::GetID("BackgroundDockSpace");
    const ImGuiDockNode * existingDockNode = ImGui::DockBuilderGetNode(dockSpaceId);
    if (state.layoutResetRequested || existingDockNode == nullptr)
    {
        state.layoutResetRequested = false;
        ImGui::DockBuilderRemoveNode(dockSpaceId);
        ImGui::DockBuilderAddNode(dockSpaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodePos(dockSpaceId, dockPosition);
        ImGui::DockBuilderSetNodeSize(dockSpaceId, ImVec2(viewport->WorkSize.x, availableDockHeight));
        ImGuiID leftNode = 0;
        ImGuiID centerNode = 0;
        ImGuiID rightNode = 0;
        ImGui::DockBuilderSplitNode(dockSpaceId, ImGuiDir_Left, dockLeftWidthRatio, &leftNode, &centerNode);
        ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Right, dockRightWidthRatio, &rightNode, &centerNode);
        for (const auto & document : state.session.documents())
        {
            ImGui::DockBuilderDockWindow(documentWindowName(document.summary).c_str(), centerNode);
        }
        ImGui::DockBuilderDockWindow("Tile Inspector###view.tile-inspector", leftNode);
        ImGui::DockBuilderDockWindow("Block Inspector###view.block-inspector", leftNode);
        ImGui::DockBuilderDockWindow("Selection Statistics###view.selection-statistics", leftNode);
        ImGui::DockBuilderDockWindow("Map Information###view.map-information", leftNode);
        ImGui::DockBuilderDockWindow("Edit Tools###view.edit-tools", rightNode);
        ImGui::DockBuilderDockWindow("Texmaps###view.texmaps", rightNode);
        ImGui::DockBuilderDockWindow("Radarcol###view.radarcol", rightNode);
        ImGui::DockBuilderFinish(dockSpaceId);
    }
    ImGui::DockSpace(dockSpaceId, ImVec2(viewport->WorkSize.x, availableDockHeight), dockSpaceFlags);
    ImGui::End();
}

void drawMapPanels(WorkbenchState & state,
                   const DataProfile & activeProfile,
                   BlockInspectorPanelState & blockInspectorState,
                   SelectionStatsPanelState & selectionStatsState,
                   EditToolsPanelState & editToolsState,
                   RadarcolPanelState & radarcolState,
                   TexmapsPanelState & texmapsState)
{
    state.providers.sync(activeProfile);
    syncRadarcolPanel(radarcolState, activeProfile);
    syncTexmapsPanel(texmapsState, activeProfile);
    MapWorkspace & workspace = state.workspace.active();
    const MapContextSnapshot context = state.session.contextSnapshot(state.workspace);
    const DocumentRecord * active = state.session.activeDocumentRecord();
    const bool mapActive = active != nullptr && active->map.has_value() && state.workspace.reader() != nullptr;

    if (mapActive && state.panelRegistry.find(PanelId::tileInspector)->visible)
    {
        drawTileInspectorPanel(workspace, context, state.providers, state.panelRegistry.find(PanelId::tileInspector)->visible);
    }
    if (mapActive && state.panelRegistry.find(PanelId::blockInspector)->visible)
    {
        drawBlockInspectorPanel(workspace, context, state.providers, blockInspectorState, state.panelRegistry.find(PanelId::blockInspector)->visible);
    }
    if (mapActive && state.panelRegistry.find(PanelId::selectionStatistics)->visible)
    {
        drawSelectionStatsPanel(workspace, context, selectionStatsState, state.panelRegistry.find(PanelId::selectionStatistics)->visible);
    }
    if (mapActive && state.panelRegistry.find(PanelId::mapInformation)->visible)
    {
        drawMapInfoPanel(workspace, state.panelRegistry.find(PanelId::mapInformation)->visible);
    }
    if (mapActive && state.panelRegistry.find(PanelId::editTools)->visible)
    {
        drawEditToolsPanel(workspace, editToolsState, state.panelRegistry.find(PanelId::editTools)->visible);
    }
    if (state.panelRegistry.find(PanelId::radarcol)->visible)
    {
        drawRadarcolPanel(radarcolState,
                          state.panelRegistry.find(PanelId::radarcol)->visible,
                          state.fileBrowser,
                          true);
    }
    if (state.panelRegistry.find(PanelId::texmaps)->visible)
    {
        drawTexmapsPanel(texmapsState, state.panelRegistry.find(PanelId::texmaps)->visible, state.fileBrowser);
    }
}

void notifyRadarcolFallback(WorkbenchState & state)
{
    const ProviderStatus status = state.providers.radarColors().status();
    if (status.source != ProviderSourceKind::embeddedDefaultWithWarning || status.revision == state.radarcolNotificationRevision)
    {
        state.radarcolNotificationRevision = status.revision;
        return;
    }

    state.radarcolNotificationRevision = status.revision;
    state.notifications.add(NotificationSeverity::warning,
                            "Configured Radarcol is unavailable; using Embedded Radarcol fallback. " + status.message);
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
    const std::string profileStatus = strings::formatProfileStatus(state.activeProfile.name);
    const std::string mapStatus = strings::formatMapStatus(state.workspace.reader() == nullptr ? std::filesystem::path{} : state.workspace.mapPath());
    const std::string stateStatus = strings::formatStateStatus(state.workspace.dirty());
    const std::string readinessStatus = strings::formatDataStatus(state.session.profileState());
    const std::string radarcolStatus = strings::formatRadarcolStatus(state.providers.radarColors().status().source);
    const float rightContentWidth = ImGui::CalcTextSize(profileStatus.c_str()).x + ImGui::GetStyle().ItemSpacing.x +
                                    ImGui::CalcTextSize(mapStatus.c_str()).x + ImGui::GetStyle().ItemSpacing.x +
                                    ImGui::CalcTextSize(stateStatus.c_str()).x + ImGui::GetStyle().ItemSpacing.x +
                                    ImGui::CalcTextSize(readinessStatus.c_str()).x + ImGui::GetStyle().ItemSpacing.x +
                                    ImGui::CalcTextSize(radarcolStatus.c_str()).x;
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - rightContentWidth));
    ImGui::TextUnformatted(profileStatus.c_str());
    ImGui::SameLine();
    ImGui::TextUnformatted(mapStatus.c_str());
    ImGui::SameLine();
    ImGui::TextUnformatted(stateStatus.c_str());
    ImGui::SameLine();
    ImGui::TextUnformatted(readinessStatus.c_str());
    ImGui::SameLine();
    ImGui::TextUnformatted(radarcolStatus.c_str());
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
                color = theme::imguiColor(theme::red);
            }
            else if (notification.severity == NotificationSeverity::warning)
            {
                color = theme::imguiColor(theme::yellow);
            }
            else if (notification.severity == NotificationSeverity::success)
            {
                color = theme::imguiColor(theme::green);
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
    ImGui::TextUnformatted("Maps are available without a profile. Embedded Radarcol is used until an external source is configured.");
    const std::string profileReadiness(strings::profileReadinessName(state.session.profileState()));
    ImGui::Text("Profile status: %s", profileReadiness.c_str());
    const std::string startupRadarcolSource(strings::providerSourceName(state.providers.radarColors().status().source));
    ImGui::Text("Radarcol source: %s", startupRadarcolSource.c_str());
    if (ImGui::Button("Open Preferences"))
    {
        requestPreferencesDialog(state.menuBarState.preferencesDialog, state.activeProfile, state.preferences);
    }
    ImGui::End();
}

ImGuiID documentDockNodeId()
{
    const ImGuiID dockSpaceId = ImGui::GetID("BackgroundDockSpace");
    const ImGuiDockNode * centralNode = ImGui::DockBuilderGetCentralNode(dockSpaceId);
    return centralNode == nullptr ? 0U : centralNode->ID;
}

void requestDocumentClose(WorkbenchState & state, DocumentId id, bool dirty);

void drawDocumentWindow(WorkbenchState & state, DocumentRecord & document)
{
    const std::string windowName = documentWindowName(document.summary);
    if (document.summary.identity.kind == DocumentKind::map && document.map)
    {
        bool visible = true;
        if (const ImGuiID centralNode = documentDockNodeId(); centralNode != 0U)
        {
            ImGui::SetNextWindowDockID(centralNode, ImGuiCond_FirstUseEver);
        }
        drawMapCanvasPanel(state.workspace,
                           state.providers.radarColors(),
                           windowName,
                           [&state, id = document.summary.identity.id] {
                               const auto activated = state.session.activateDocument(id, state.workspace);
                               if (!activated)
                               {
                                   state.notifications.add(NotificationSeverity::error, activated.error().message);
                               }
                           },
                           [&state] { static_cast<void>(state.commands.execute(CommandId::fileOpen, state)); },
                           [&state] { static_cast<void>(state.commands.execute(CommandId::fileNew, state)); },
                           [&state](const MapCanvasStatus & status) { state.lastHoveredCanvasStatus = status; },
                           visible);
        if (!visible)
        {
            requestDocumentClose(state, document.summary.identity.id, document.summary.dirty);
        }
        return;
    }

    if (document.summary.identity.kind == DocumentKind::radarcol && document.radarcol)
    {
        bool visible = true;
        if (const ImGuiID centralNode = documentDockNodeId(); centralNode != 0U)
        {
            ImGui::SetNextWindowDockID(centralNode, ImGuiCond_FirstUseEver);
        }
        drawRadarcolDocumentPanel(state.radarcolState,
                                  *document.radarcol,
                                  visible,
                                  state.fileBrowser,
                                  [&state, id = document.summary.identity.id] {
                                      const auto activated = state.session.activateDocument(id, state.workspace);
                                      if (!activated)
                                      {
                                          state.notifications.add(NotificationSeverity::error, activated.error().message);
                                      }
                                  });
        if (!visible)
        {
            requestDocumentClose(state, document.summary.identity.id, document.summary.dirty);
        }
        return;
    }

    bool visible = true;
    if (const ImGuiID centralNode = documentDockNodeId(); centralNode != 0U)
    {
        ImGui::SetNextWindowDockID(centralNode, ImGuiCond_FirstUseEver);
    }
    if (ImGui::Begin(windowName.c_str(), &visible))
    {
        const auto activated = state.session.activateDocument(document.summary.identity.id, state.workspace);
        if (!activated)
        {
            state.notifications.add(NotificationSeverity::error, activated.error().message);
        }
        const std::string kindName(documentKindName(document.summary.identity.kind));
        ImGui::Text("%s is active, but has no view yet.", kindName.c_str());
        ImGui::TextUnformatted("This document type is represented as a placeholder until its view is implemented.");
    }
    ImGui::End();
    if (!visible)
    {
        requestDocumentClose(state, document.summary.identity.id, document.summary.dirty);
    }
}

void drawDocumentWindows(WorkbenchState & state)
{
    for (auto & document : state.session.documentsMutable())
    {
        drawDocumentWindow(state, document);
    }
}

void drawDocumentSurface(WorkbenchState & state)
{
    drawDocumentWindows(state);
}

void requestDocumentClose(WorkbenchState & state, DocumentId id, bool dirty)
{
    state.pendingDocumentClose = id;
    state.pendingDocumentCloseDirty = dirty;
}

void applyPendingDocumentAction(WorkbenchState & state)
{
    if (state.pendingDocumentActivation)
    {
        const DocumentId id = *state.pendingDocumentActivation;
        state.pendingDocumentActivation.reset();
        const auto activated = state.session.activateDocument(id, state.workspace);
        if (!activated)
        {
            state.notifications.add(NotificationSeverity::error, activated.error().message);
        }
        return;
    }

    if (!state.pendingDocumentClose)
    {
        return;
    }

    const DocumentId id = *state.pendingDocumentClose;
    const bool dirty = state.pendingDocumentCloseDirty;
    state.pendingDocumentClose.reset();
    state.pendingDocumentCloseDirty = false;

    if (dirty)
    {
        const auto activated = state.session.activateDocument(id, state.workspace);
        if (!activated)
        {
            state.notifications.add(NotificationSeverity::error, activated.error().message);
            return;
        }
        static_cast<void>(state.commands.execute(CommandId::fileClose, state));
        return;
    }

    const auto closed = state.session.closeDocument(id, state.workspace);
    if (!closed)
    {
        state.notifications.add(NotificationSeverity::error, closed.error().message);
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

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
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
        state.workspace.invalidateProviderCaches();
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

    state.session.syncDocument(state.workspace);

    welcomeShell = state.session.startupState() == StartupState::mapWelcome && state.workspace.reader() == nullptr &&
                   !state.session.hasDocuments();
    state.providers.sync(state.activeProfile);
    if (state.nativeWelcomeWindowMode != welcomeShell)
    {
        glfwSetWindowAttrib(window, GLFW_DECORATED, welcomeShell ? GLFW_FALSE : GLFW_TRUE);
        glfwSetWindowAttrib(window, GLFW_RESIZABLE, welcomeShell ? GLFW_FALSE : GLFW_TRUE);
        state.nativeWelcomeWindowMode = welcomeShell;
    }
    dispatchShortcuts(state);

    const float bottomPanelHeight = welcomeShell ? 0.0F : drawLogPanel(logSink, state.preferences, state.logEntries, state.followLogScroll);
    if (state.session.startupState() == StartupState::configureProfile)
    {
        drawStartupScreen(state);
    }
    else if (welcomeShell)
    {
        state.recentMapRecordedPath.clear();
        drawWelcomeScreen(state);
        notifyRadarcolFallback(state);
        profileChanged = drawMenuBar(state, false);
        static_cast<void>(profileChanged);
    }
    else
    {
        state.welcomeGreetingSelected = false;
        state.welcomeGreeting.clear();
        drawDockSpace(state, bottomPanelHeight);
        applyPendingDocumentAction(state);
        drawDocumentSurface(state);
        drawMapPanels(state,
                      state.activeProfile,
                      state.blockInspectorState,
                      state.selectionStatsState,
                      state.editToolsState,
                      state.radarcolState,
                      state.texmapsState);
        notifyRadarcolFallback(state);
        if (state.workspace.reader() != nullptr && state.recentMapRecordedPath != state.workspace.mapPath())
        {
            state.session.recordRecentMap(state.workspace.mapPath(), state.workspace.dimensions());
            state.recentMapRecordedPath = state.workspace.mapPath();
        }
    }
    if (state.workspace.reader() != nullptr)
    {
        drawStatusBar(state, bottomPanelHeight);
    }
    drawNotifications(state.notifications);
}

} // namespace uo_mule::gui
