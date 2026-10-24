#include "gui/frame/workbench_frame.h"
#include "gui/commands/command_registry.h"
#include "gui/commands/menu_bar.h"
#include "gui/dialogs/preferences_dialog.h"
#include "gui/documents/document_window.h"
#include "gui/panels/block_inspector_panel.h"
#include "gui/panels/icon_gallery_panel.h"
#include "gui/panels/log_panel.h"
#include "gui/panels/radarcol_panel.h"
#include "gui/panels/texmap_panel.h"
#include "gui/panels/tile_inspector_panel.h"
#include "gui/panels/tiledata_panel.h"
#include "gui/panels/tool_palette_panel.h"
#include "gui/persistence/layout_store.h"
#include "gui/presentation/strings/ui_strings.h"
#include "gui/presentation/icons/iconography.h"
#include "gui/presentation/theme/theme.h"
#include "gui/shell/welcome_screen.h"
#include "gui/state/notification_state.h"
#include "gui/state/workbench_state.h"
#include "gui/views/world/map_canvas_view.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/documents/map_document.h"
#include "gui_model/documents/radarcol_document.h"
#include "gui_model/layout/panel_registry.h"
#include "gui_model/lifecycle/document_lifecycle.h"
#include "gui_model/model/assets/asset_window_contracts.h"
#include "gui_model/operations/operation_controller.h"
#include "gui_model/persistence/asset_state_store.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/providers/data_providers.h"
#include "gui_model/session/workbench_session.h"
#include "gui_model/tools/tool_contracts.h"
#include "gui_model/workspaces/map_workspace.h"
#include "gui_model/world/map_canvas_state.h"
#include "gui_model/world/map_context.h"
#include "logger/gui_log_sink.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <optional>
#include <string>
#include <string_view>
#include <uomul/map/map.h>
#include <utility>
#include <vector>

namespace uo_mule::gui
{
namespace
{

// Defaults mirror the user's persisted workbench arrangement: inspectors on
// the left, documents in the center, and asset palettes stacked on the right.
constexpr float dockLeftWidthRatio = 0.306F;
constexpr float dockRightWidthRatio = 0.447F;
constexpr float dockLeftBottomRatio = 0.5F;
constexpr float toolDockSplitRatio = 0.38F;
constexpr float statusBarHeight = 24.0F;
constexpr float notificationLeftMargin = 16.0F;
constexpr float notificationTopMargin = 48.0F;
constexpr float notificationMinimumWidth = 280.0F;
constexpr float notificationMaximumWidth = 480.0F;
constexpr float notificationMaximumHeightRatio = 0.5F;
constexpr std::size_t shortcutCount = 8;
constexpr std::size_t graveShortcutIndex = 0;
constexpr std::size_t saveAsShortcutIndex = 1;
constexpr std::size_t saveShortcutIndex = 2;
constexpr std::size_t openShortcutIndex = 3;
constexpr std::size_t newShortcutIndex = 4;
constexpr std::size_t quitShortcutIndex = 5;
constexpr std::size_t goToShortcutIndex = 6;
constexpr std::size_t fullscreenShortcutIndex = 7;

struct ToolShortcut
{
    ImGuiKey key;
    std::string_view shortcut;
    bool shift = false;
};

constexpr std::array<ToolShortcut, 6> toolShortcuts{
    ToolShortcut{.key = ImGuiKey_P, .shortcut = "P"},
    ToolShortcut{.key = ImGuiKey_R, .shortcut = "R"},
    ToolShortcut{.key = ImGuiKey_F, .shortcut = "F"},
    ToolShortcut{.key = ImGuiKey_U, .shortcut = "U"},
    ToolShortcut{.key = ImGuiKey_B, .shortcut = "Shift+B", .shift = true},
    ToolShortcut{.key = ImGuiKey_R, .shortcut = "Shift+R", .shift = true},
};

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
    const float availableDockHeight =
        std::max(0.0F, viewport->WorkSize.y - bottomPanelHeight - statusBarHeight - (dockPosition.y - viewport->WorkPos.y));
    constexpr ImGuiDockNodeFlags dockSpaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;
    const ImGuiID dockSpaceId = ImGui::GetID("BackgroundDockSpace");
    const ImGuiDockNode * existingDockNode = ImGui::DockBuilderGetNode(dockSpaceId);
    if (state.layoutResetRequested || existingDockNode == nullptr)
    {
        if (state.layoutResetRequested)
        {
            ImGui::ClearIniSettings();
        }
        state.layoutResetRequested = false;
        ImGui::DockBuilderRemoveNode(dockSpaceId);
        ImGui::DockBuilderAddNode(dockSpaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodePos(dockSpaceId, dockPosition);
        ImGui::DockBuilderSetNodeSize(dockSpaceId, ImVec2(viewport->WorkSize.x, availableDockHeight));
        ImGuiID leftNode = 0;
        ImGuiID centerNode = 0;
        ImGuiID rightNode = 0;
        ImGuiID leftBottomNode = 0;
        ImGuiID leftTopNode = 0;
        ImGuiID toolNode = 0;
        ImGui::DockBuilderSplitNode(dockSpaceId, ImGuiDir_Left, dockLeftWidthRatio, &leftNode, &centerNode);
        ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Right, dockRightWidthRatio, &rightNode, &centerNode);
        ImGui::DockBuilderSplitNode(leftNode, ImGuiDir_Down, dockLeftBottomRatio, &leftBottomNode, &leftTopNode);
        ImGui::DockBuilderSplitNode(leftTopNode, ImGuiDir_Up, toolDockSplitRatio, &toolNode, &leftTopNode);
        for (const auto & document : state.session.documents())
        {
            ImGui::DockBuilderDockWindow(documentWindowName(document.summary).c_str(), centerNode);
        }
        ImGui::DockBuilderDockWindow("Tool Palette###view.tool-palette", toolNode);
        ImGui::DockBuilderDockWindow("Tile Inspector###view.tile-inspector", leftBottomNode);
        ImGui::DockBuilderDockWindow("Block Inspector###view.block-inspector", leftTopNode);
        ImGui::DockBuilderDockWindow("Texmaps###view.texmaps", rightNode);
        ImGui::DockBuilderDockWindow("Radarcol###view.radarcol", rightNode);
        ImGui::DockBuilderDockWindow("Tiledata###view.tiledata", rightNode);
        ImGui::DockBuilderFinish(dockSpaceId);
    }
    ImGui::DockSpace(dockSpaceId, ImVec2(viewport->WorkSize.x, availableDockHeight), dockSpaceFlags);
    ImGui::End();
}

void captureWindowGeometry(GLFWwindow * window, WorkspaceLayout & layout)
{
    glfwGetWindowPos(window, &layout.window.x, &layout.window.y);
    glfwGetWindowSize(window, &layout.window.width, &layout.window.height);
    layout.window.geometryValid = layout.window.width > 0 && layout.window.height > 0;
}

bool applyWorkbenchLayout(GLFWwindow * window, WorkbenchState & state)
{
    if (state.workbenchLayoutApplied)
    {
        return true;
    }

    if (state.preferences.workspaceFullscreen)
    {
        GLFWmonitor * monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode * mode = monitor == nullptr ? nullptr : glfwGetVideoMode(monitor);
        if (mode == nullptr)
        {
            return false;
        }
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    }
    else if (state.layout.window.geometryValid)
    {
        glfwSetWindowPos(window, state.layout.window.x, state.layout.window.y);
        glfwSetWindowSize(window, state.layout.window.width, state.layout.window.height);
    }

    state.workbenchLayoutApplied = true;
    return true;
}

void toggleFullscreen(GLFWwindow * window, WorkbenchState & state)
{
    if (!state.fullscreenToggleRequested)
    {
        return;
    }

    state.fullscreenToggleRequested = false;
    if (!state.preferences.workspaceFullscreen)
    {
        captureWindowGeometry(window, state.layout);
        GLFWmonitor * monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode * mode = monitor == nullptr ? nullptr : glfwGetVideoMode(monitor);
        if (mode == nullptr)
        {
            return;
        }

        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        state.preferences.workspaceFullscreen = true;
        return;
    }

    const int width = state.layout.window.geometryValid ? state.layout.window.width : defaultWindowWidth;
    const int height = state.layout.window.geometryValid ? state.layout.window.height : defaultWindowHeight;
    const int x = state.layout.window.geometryValid ? state.layout.window.x : 0;
    const int y = state.layout.window.geometryValid ? state.layout.window.y : 0;
    glfwSetWindowMonitor(window, nullptr, x, y, width, height, 0);
    state.preferences.workspaceFullscreen = false;
}

void drawMapPanels(WorkbenchState & state,
                   BlockInspectorPanelState & blockInspectorState,
                   RadarcolPanelState & radarcolState,
                   TexmapsPanelState & texmapsState)
{
    const auto pickAssetItem = [&state](const assets::AssetItemId & item, bool tiledataSource)
    {
        if (!state.toolState.tilePickRequested())
        {
            return;
        }
        if (const auto id = assets::itemIndex(item))
        {
            if (state.toolState.tilePickTarget() == TilePickTarget::fuzzyName)
            {
                if (!tiledataSource)
                {
                    return;
                }
                const auto tile = state.providers.tiledata().mapTile(static_cast<std::uint16_t>(*id));
                if (tile)
                {
                    state.toolState.setPickedTileName(tile->record.name);
                }
                return;
            }
            static_cast<void>(state.toolState.setPickedTileId(static_cast<std::int32_t>(*id)));
        }
    };
    state.radarcolState.itemPicked = [&pickAssetItem](const assets::AssetItemId & item) { pickAssetItem(item, false); };
    state.texmapsState.itemPicked = [&pickAssetItem](const assets::AssetItemId & item) { pickAssetItem(item, false); };
    state.tiledataState.itemPicked = [&pickAssetItem](const assets::AssetItemId & item) { pickAssetItem(item, true); };

    if (state.panelRegistry.find(PanelId::toolPalette)->visible)
    {
        drawToolPalettePanel(state, state.panelRegistry.find(PanelId::toolPalette)->visible);
    }
    if (state.panelRegistry.find(PanelId::iconGallery)->visible)
    {
        drawIconGalleryPanel(state.panelRegistry.find(PanelId::iconGallery)->visible, iconFontAvailable());
    }

    MapWorkspace & workspace = state.workspace.active();
    const MapContextSnapshot context = state.session.contextSnapshot(state.workspace);
    const DocumentRecord * active = state.session.activeDocumentRecord();
    const bool mapActive = active != nullptr && active->map.has_value() && state.workspace.reader() != nullptr;

    if (mapActive && state.panelRegistry.find(PanelId::tileInspector)->visible)
    {
        drawTileInspectorPanel(workspace,
                                context,
                                state.providers,
                                state.preferences.idFormat,
                               iconFontAvailable(),
                                state.panelRegistry.find(PanelId::tileInspector)->visible);
    }
    if (mapActive && state.panelRegistry.find(PanelId::blockInspector)->visible)
    {
        drawBlockInspectorPanel(workspace,
                                context,
                                state.providers,
                                state.preferences.mapViewTileSource,
                                blockInspectorState,
                                state.panelRegistry.find(PanelId::blockInspector)->visible);
    }
    if (state.panelRegistry.find(PanelId::radarcol)->visible)
    {
        drawRadarcolPanel(radarcolState,
                          state.providers.radarColors(),
                          state.panelRegistry.find(PanelId::radarcol)->visible,
                          state.preferences.idFormat);
    }
    if (state.panelRegistry.find(PanelId::texmaps)->visible)
    {
        drawTexmapsPanel(texmapsState,
                         state.providers.textures(),
                         state.panelRegistry.find(PanelId::texmaps)->visible,
                         state.preferences,
                         state.pendingPreferenceChanges);
    }
    if (state.panelRegistry.find(PanelId::tiledata)->visible)
    {
        drawTiledataPanel(state.tiledataState,
                          state.providers.tiledata(),
                          state.providers.textures(),
                          state.providers.radarColors(),
                          state.panelRegistry.find(PanelId::tiledata)->visible,
                          state.preferences.idFormat);
    }
}

void applyPendingPreferenceChanges(WorkbenchState & state)
{
    if (state.pendingPreferenceChanges.empty())
    {
        return;
    }

    if (state.pendingPreferenceChanges.texmapsHideUnused)
    {
        state.preferences.texmapsHideUnused = *state.pendingPreferenceChanges.texmapsHideUnused;
    }
    if (state.pendingPreferenceChanges.texmapsTablePresentation)
    {
        state.preferences.texmapsTablePresentation = *state.pendingPreferenceChanges.texmapsTablePresentation;
    }
    state.pendingPreferenceChanges = {};
}

void notifyRadarcolFallback(WorkbenchState & state)
{
    const ProviderStatus status = state.providers.radarColors().status();
    if (status.source != ProviderSourceKind::embeddedDefaultWithWarning ||
        status.revision == state.radarcolNotificationRevision)
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
    if (!state.operationStatus.empty())
    {
        ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        if (state.operationStatusSeverity == NotificationSeverity::error)
        {
            color = theme::imguiColor(theme::error);
        }
        else if (state.operationStatusSeverity == NotificationSeverity::success)
        {
            color = theme::imguiColor(theme::success);
        }
        ImGui::TextColored(color, "%s", state.operationStatus.c_str());
        ImGui::SameLine();
    }
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
                        canvasStatus.canvasId.c_str(),
                        canvasStatus.tile->x,
                        canvasStatus.tile->y,
                        block.x,
                        block.y,
                        canvasStatus.zoom);
        }
    }
    if (hasCanvasStatus)
    {
        ImGui::SameLine();
    }
    const std::string profileStatus = strings::formatProfileStatus(state.activeProfile.name);
    const std::string mapStatus =
        strings::formatMapStatus(state.workspace.reader() == nullptr ? std::filesystem::path{} : state.workspace.mapPath());
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
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + notificationLeftMargin, viewport->WorkPos.y + notificationTopMargin),
                            ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(notificationMinimumWidth, 0.0F),
        ImVec2(notificationMaximumWidth, viewport->WorkSize.y * notificationMaximumHeightRatio));
    if (ImGui::Begin("Notifications###workbench.notifications",
                     nullptr,
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        for (const auto & notification : notifications.entries)
        {
            ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
            if (notification.severity == NotificationSeverity::error)
            {
                color = theme::imguiColor(theme::error);
            }
            else if (notification.severity == NotificationSeverity::warning)
            {
                color = theme::imguiColor(theme::warning);
            }
            else if (notification.severity == NotificationSeverity::success)
            {
                color = theme::imguiColor(theme::success);
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
    if (!ImGui::Begin("Configure Preferences###startup.configure-profile",
                      nullptr,
                      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::End();
        return;
    }
    const std::string configureLabel = iconLabel(IconId::preferences, "Configure Preferences");
    ImGui::TextUnformatted(configureLabel.c_str());
    ImGui::Separator();
    ImGui::TextUnformatted(
        "Maps are available without a profile. Embedded Radarcol is used until an external source is configured.");
    const std::string profileReadiness(strings::profileReadinessName(state.session.profileState()));
    ImGui::Text("Profile status: %s", profileReadiness.c_str());
    const std::string startupRadarcolSource(strings::providerSourceName(state.providers.radarColors().status().source));
    ImGui::Text("Radarcol source: %s", startupRadarcolSource.c_str());
    const std::string preferencesLabel = iconLabel(IconId::preferences, "Open Preferences");
    if (ImGui::Button(preferencesLabel.c_str()))
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

void activateDocumentOrNotify(WorkbenchState & state, DocumentId id)
{
    const auto activated = state.session.activateDocument(id, state.workspace);
    if (!activated)
    {
        state.notifications.add(NotificationSeverity::error, activated.error().message);
    }
}

void requestDocumentActivation(WorkbenchState & state, DocumentId id)
{
    state.documentLifecycle.requestActivation(id);
}

void dockDocumentWindow()
{
    if (const ImGuiID centralNode = documentDockNodeId(); centralNode != 0U)
    {
        ImGui::SetNextWindowDockID(centralNode, ImGuiCond_FirstUseEver);
    }
}

void reportOperation(WorkbenchState & state, std::string message, bool success)
{
    state.operationStatus = std::move(message);
    state.operationStatusSeverity = success ? NotificationSeverity::success : NotificationSeverity::error;
}

void executeMapEdit(WorkbenchState & state, uomul::TileCoord origin)
{
    if (state.toolState.activeTool() == ToolId::fill)
    {
        const auto result =
            state.workspace.fillMasked(state.toolState.activeTile().value_or(uomul::MapTile{}), state.toolState.fillMode());
        reportOperation(state,
                        result ? "Filled " + std::to_string(result->changedTiles) + " tiles" : result.error().message,
                        result.has_value());
        return;
    }
    if (state.toolState.activeTool() == ToolId::replace)
    {
        const auto result = state.workspace.replaceMasked(
            state.toolState.replaceMode(), state.toolState.replaceSource(), state.toolState.replaceTarget());
        reportOperation(state,
                        result ? "Replaced " + std::to_string(result->changedTiles) + " tiles" : result.error().message,
                        result.has_value());
        return;
    }
    if (state.toolState.activeTool() == ToolId::fuzzySelect)
    {
        const FuzzyTiledataNameLookup nameLookup = [&state](std::uint16_t id) -> std::optional<std::string>
        {
            const auto tile = state.providers.tiledata().mapTile(id);
            return tile ? std::optional<std::string>{tile->record.name} : std::nullopt;
        };
        const auto result = state.workspace.fuzzySelect(state.toolState.fuzzyOptions(), origin, nameLookup);
        reportOperation(
            state, result ? "Selected " + std::to_string(*result) + " tiles" : result.error().message, result.has_value());
    }
}

[[nodiscard]] std::function<void(uomul::TileCoord)> mapEditCallback(WorkbenchState & state)
{
    return [&state](uomul::TileCoord origin) { executeMapEdit(state, origin); };
}

[[nodiscard]] std::function<std::optional<std::string>(std::uint16_t)> tiledataNameCallback(WorkbenchState & state)
{
    return [&state](std::uint16_t id) -> std::optional<std::string>
    {
        const auto tile = state.providers.tiledata().mapTile(id);
        return tile ? std::optional<std::string>{tile->record.name} : std::nullopt;
    };
}

void drawDocumentWindow(WorkbenchState & state, DocumentRecord & document)
{
    const std::string windowName = documentWindowName(document.summary);
    if (document.summary.identity.kind == DocumentKind::map && document.map)
    {
        bool visible = true;
        dockDocumentWindow();
        drawMapCanvasView(
            state.workspace,
            state.toolState,
            state.providers.textures(),
            state.providers.radarColors(),
            state.preferences.mapViewTileSource,
            windowName,
            [&state, id = document.summary.identity.id] { requestDocumentActivation(state, id); },
            [&state] { static_cast<void>(state.commands.execute(CommandId::fileOpen, state)); },
            [&state] { static_cast<void>(state.commands.execute(CommandId::fileNew, state)); },
            mapEditCallback(state),
            tiledataNameCallback(state),
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
        dockDocumentWindow();
        drawRadarcolDocumentPanel(state.radarcolState,
                                  *document.radarcol,
                                  visible,
                                  state.fileBrowser,
                                  state.preferences.idFormat,
                                  windowName,
                                  [&state, id = document.summary.identity.id] { requestDocumentActivation(state, id); });
        if (!visible)
        {
            requestDocumentClose(state, document.summary.identity.id, document.summary.dirty);
        }
        return;
    }

    if (document.summary.identity.kind == DocumentKind::tiledata && document.tiledata)
    {
        bool visible = true;
        dockDocumentWindow();
        drawTiledataDocumentPanel(state.tiledataState,
                                  *document.tiledata,
                                  state.providers.textures(),
                                  state.providers.radarColors(),
                                  visible,
                                  state.preferences.idFormat,
                                  windowName);
        if (!visible)
        {
            requestDocumentClose(state, document.summary.identity.id, document.summary.dirty);
        }
        return;
    }

    bool visible = true;
    dockDocumentWindow();
    if (ImGui::Begin(windowName.c_str(), &visible))
    {
        requestDocumentActivation(state, document.summary.identity.id);
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

void finishPendingAssetClose(WorkbenchState & state, DocumentId id, PendingChangesAction action)
{
    if (action == PendingChangesAction::save)
    {
        const auto transition = state.operationController.completeSave(true);
        if (transition.kind == OperationTransitionKind::execute)
        {
            const auto closed = state.session.closeDocument(id, state.workspace);
            if (!closed)
            {
                state.notifications.add(NotificationSeverity::error, closed.error().message);
            }
        }
    }
    else if (action == PendingChangesAction::discard)
    {
        static_cast<void>(state.operationController.cancel());
        const auto closed = state.session.closeDocument(id, state.workspace);
        if (!closed)
        {
            state.notifications.add(NotificationSeverity::error, closed.error().message);
        }
    }
}

void processDocumentIntent(WorkbenchState & state, DocumentRecord & document)
{
    if (!document.radarcol)
    {
        return;
    }
    if (auto result = document.radarcol->processPendingIntent(); !result)
    {
        state.notifications.add(NotificationSeverity::error, result.error().message);
        if (state.pendingAssetCloseDecision && state.pendingAssetCloseAction &&
            *state.pendingAssetCloseDecision == document.summary.identity.id &&
            *state.pendingAssetCloseAction == PendingChangesAction::discard)
        {
            static_cast<void>(state.operationController.failDirtyResolution());
        }
        return;
    }
    if (state.pendingAssetCloseDecision && state.pendingAssetCloseAction &&
        *state.pendingAssetCloseDecision == document.summary.identity.id)
    {
        const DocumentId id = *state.pendingAssetCloseDecision;
        const PendingChangesAction action = *state.pendingAssetCloseAction;
        state.pendingAssetCloseDecision.reset();
        state.pendingAssetCloseAction.reset();
        finishPendingAssetClose(state, id, action);
    }
}

void processDocumentIntents(WorkbenchState & state)
{
    for (auto & document : state.session.documentsMutable())
    {
        processDocumentIntent(state, document);
    }
    state.session.syncAssetDocuments();
}

void drawPendingAssetCloseDialog(WorkbenchState & state)
{
    if (!state.pendingAssetCloseDecision)
    {
        return;
    }
    const auto id = *state.pendingAssetCloseDecision;
    if (!ImGui::BeginPopup("PendingAssetClose###dialog.pending-asset-close"))
    {
        ImGui::OpenPopup("PendingAssetClose###dialog.pending-asset-close");
        return;
    }
    ImGui::TextUnformatted("This document has staged changes.");
    const std::string saveLabel = iconLabel(IconId::save, "Save");
    if (ImGui::Button(saveLabel.c_str()))
    {
        DocumentRecord * record = nullptr;
        for (auto & candidate : state.session.documentsMutable())
        {
            if (candidate.summary.identity.id == id)
            {
                record = &candidate;
                break;
            }
        }
        if (record != nullptr && record->radarcol)
        {
            static_cast<void>(state.operationController.resolveDirty(PendingChangesAction::save));
            record->radarcol->requestIntent({
                .kind = RadarcolDocumentIntentKind::save,
                .colorIndex = 0,
                .color = 0,
                .outputPath = {},
                .overwriteOutput = false,
            });
            state.pendingAssetCloseAction = PendingChangesAction::save;
        }
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    const std::string discardLabel = iconLabel(IconId::cancel, "Discard");
    if (ImGui::Button(discardLabel.c_str()))
    {
        DocumentRecord * record = nullptr;
        for (auto & candidate : state.session.documentsMutable())
        {
            if (candidate.summary.identity.id == id)
            {
                record = &candidate;
                break;
            }
        }
        if (record != nullptr && record->radarcol)
        {
            static_cast<void>(state.operationController.resolveDirty(PendingChangesAction::discard));
            record->radarcol->requestIntent({
                .kind = RadarcolDocumentIntentKind::discard,
                .colorIndex = 0,
                .color = 0,
                .outputPath = {},
                .overwriteOutput = false,
            });
            state.pendingAssetCloseAction = PendingChangesAction::discard;
        }
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    const std::string cancelLabel = iconLabel(IconId::cancel, "Cancel");
    if (ImGui::Button(cancelLabel.c_str()))
    {
        state.pendingAssetCloseDecision.reset();
        state.pendingAssetCloseAction.reset();
        static_cast<void>(state.operationController.cancel());
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void requestDocumentClose(WorkbenchState & state, DocumentId id, bool dirty)
{
    static_cast<void>(dirty);
    state.documentLifecycle.requestClose({.id = id, .dirtyPolicy = DocumentCloseDirtyPolicy::prompt});
}

void applyPendingDocumentAction(WorkbenchState & state)
{
    if (const auto id = state.documentLifecycle.takeActivation())
    {
        activateDocumentOrNotify(state, *id);
        return;
    }

    const auto pendingClose = state.documentLifecycle.takeClose();
    if (!pendingClose)
    {
        return;
    }

    const DocumentCloseRequest request = *pendingClose;
    const DocumentId id = request.id;

    const DocumentRecord * requested = nullptr;
    for (const auto & candidate : state.session.documents())
    {
        if (candidate.summary.identity.id == id)
        {
            requested = &candidate;
            break;
        }
    }
    if (requested == nullptr)
    {
        return;
    }

    if (requested->summary.dirty)
    {
        const auto activated = state.session.activateDocument(id, state.workspace);
        if (!activated)
        {
            state.notifications.add(NotificationSeverity::error, activated.error().message);
            return;
        }
        if (requested->map)
        {
            static_cast<void>(state.commands.execute(CommandId::fileClose, state));
        }
        else if (requested->radarcol)
        {
            OperationRequest operation{};
            operation.kind = OperationKind::close;
            operation.documentId = id;
            const auto transition =
                state.operationController.begin(std::move(operation), true, false, OutputCollisionPolicy::confirm);
            if (transition.kind == OperationTransitionKind::showDirtyDecision)
            {
                state.pendingAssetCloseDecision = id;
                state.notifications.add(NotificationSeverity::warning, "Save or discard changes before closing this document");
            }
        }
        return;
    }

    const auto closed = state.session.closeDocument(id, state.workspace);
    if (!closed)
    {
        state.notifications.add(NotificationSeverity::error, closed.error().message);
    }
}

[[nodiscard]] bool dispatchToolShortcuts(WorkbenchState & state)
{
    for (const ToolShortcut & shortcut : toolShortcuts)
    {
        if (ImGui::GetIO().KeyShift != shortcut.shift || ImGui::GetIO().KeyCtrl || !ImGui::IsKeyPressed(shortcut.key, false))
        {
            continue;
        }
        return state.commands.executeShortcut(shortcut.shortcut, state);
    }
    return false;
}

[[nodiscard]] bool dispatchSelectionShortcuts(WorkbenchState & state)
{
    if (!ImGui::GetIO().KeyCtrl)
    {
        return false;
    }
    if (ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_A, false))
    {
        return state.commands.executeShortcut("Ctrl+Shift+A", state);
    }
    if (!ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_A, false))
    {
        return state.commands.executeShortcut("Ctrl+A", state);
    }
    return ImGui::IsKeyPressed(ImGuiKey_I, false) && state.commands.executeShortcut("Ctrl+I", state);
}

[[nodiscard]] bool dispatchGeneralShortcut(WorkbenchState & state, bool allowFullscreen)
{
    const std::array<std::pair<ImGuiKey, std::string_view>, shortcutCount> shortcuts{
        std::pair{ImGuiKey_GraveAccent, "`"},
        std::pair{ImGuiKey_S, "Ctrl+Shift+S"},
        std::pair{ImGuiKey_S, "Ctrl+S"},
        std::pair{ImGuiKey_O, "Ctrl+O"},
        std::pair{ImGuiKey_N, "Ctrl+N"},
        std::pair{ImGuiKey_Q, "Ctrl+Q"},
        std::pair{ImGuiKey_G, "Ctrl+G"},
        std::pair{ImGuiKey_F11, "F11"},
    };
    const auto execute = [&state, &shortcuts](std::size_t index)
    { return state.commands.executeShortcut(shortcuts.at(index).second, state); };
    if (ImGui::IsKeyPressed(shortcuts.at(graveShortcutIndex).first, false))
        return execute(graveShortcutIndex);
    if (ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyShift &&
        ImGui::IsKeyPressed(shortcuts.at(saveAsShortcutIndex).first, false))
        return execute(saveAsShortcutIndex);
    if (ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(shortcuts.at(saveShortcutIndex).first, false))
        return execute(saveShortcutIndex);
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(shortcuts.at(openShortcutIndex).first, false))
        return execute(openShortcutIndex);
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(shortcuts.at(newShortcutIndex).first, false))
        return execute(newShortcutIndex);
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(shortcuts.at(quitShortcutIndex).first, false))
        return execute(quitShortcutIndex);
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(shortcuts.at(goToShortcutIndex).first, false))
        return execute(goToShortcutIndex);
    return allowFullscreen && ImGui::IsKeyPressed(shortcuts.at(fullscreenShortcutIndex).first, false) &&
           execute(fullscreenShortcutIndex);
}

void dispatchShortcuts(WorkbenchState & state, bool allowFullscreen)
{
    if (ImGui::GetIO().WantTextInput || ImGui::GetIO().WantCaptureKeyboard)
    {
        return;
    }
    if (dispatchToolShortcuts(state) || dispatchSelectionShortcuts(state))
        return;
    static_cast<void>(dispatchGeneralShortcut(state, allowFullscreen));
}

void beginGuiFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

[[nodiscard]] GLFWcursor * transparentCursor()
{
    static GLFWcursor * cursor = []
    {
        static std::array<unsigned char, 4> pixels{0U, 0U, 0U, 0U};
        GLFWimage image{1, 1, pixels.data()};
        return glfwCreateCursor(&image, 0, 0);
    }();
    return cursor;
}

[[nodiscard]] std::optional<IconId> toolCursorIcon(const WorkbenchState & state)
{
    if (state.toolState.tilePickRequested())
    {
        return IconId::eyedropper;
    }
    const MapCanvasViewState & canvasState = state.workspace.canvasState();
    if (!canvasState.cursorHovered)
    {
        return std::nullopt;
    }
    if (canvasState.panning || state.toolState.activeTool() == ToolId::move)
    {
        return IconId::pan;
    }
    switch (state.toolState.activeTool())
    {
    case ToolId::rectangleSelect:
        return IconId::selection;
    case ToolId::lassoSelect:
        return IconId::lasso;
    case ToolId::fuzzySelect:
        return IconId::autoFix;
    case ToolId::fill:
        return IconId::formatColorFill;
    case ToolId::replace:
        return IconId::replace;
    case ToolId::move:
        return IconId::pan;
    }
    return std::nullopt;
}

void drawToolCursor(GLFWwindow * window, const WorkbenchState & state)
{
    static bool customCursorActive = false;
    const std::optional<IconId> icon = toolCursorIcon(state);
    if (!icon.has_value())
    {
        if (customCursorActive)
        {
            glfwSetCursor(window, nullptr);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            customCursorActive = false;
        }
        return;
    }
    if (!iconFontAvailable())
    {
        if (customCursorActive)
        {
            glfwSetCursor(window, nullptr);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            customCursorActive = false;
        }
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        return;
    }

    if (GLFWcursor * cursor = transparentCursor(); cursor != nullptr)
    {
        glfwSetCursor(window, cursor);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        customCursorActive = true;
    }
    else
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        customCursorActive = true;
    }
    ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    const std::string glyph = iconGlyph(*icon);
    const ImVec2 mousePosition = ImGui::GetIO().MousePos;
    ImGui::GetForegroundDrawList()->AddText(ImGui::GetFont(),
                                            ImGui::GetFontSize(),
                                            ImVec2(mousePosition.x + 1.0F, mousePosition.y + 1.0F),
                                            ImGui::GetColorU32(ImGuiCol_Text),
                                            glyph.c_str());
}

void applyPendingClipboard(GLFWwindow * window, WorkbenchState & state)
{
    if (state.pendingClipboardText.empty())
    {
        return;
    }

    glfwSetClipboardString(window, state.pendingClipboardText.c_str());
    state.pendingClipboardText.clear();
}

void applyProfileChange(WorkbenchState & state)
{
    state.preferences.activeDataProfile = state.activeProfile.name;
    state.session.updateProfile(state.activeProfile);
    state.assetStateRestored = false;
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

void syncProviders(WorkbenchState & state)
{
    state.providers.sync(state.activeProfile);
    const ProviderRevisions revisions = state.providers.revisions();
    if (revisions == state.providerRevisions)
    {
        return;
    }

    state.providerRevisions = revisions;
    state.workspace.invalidateProviderCaches();
    assets::resetContext(state.radarcolState.assetView);
    assets::resetContext(state.texmapsState.assetView);
    assets::resetContext(state.tiledataState.assetView);
}

[[nodiscard]] assets::AssetSourceContext
metadataContext(DocumentKind kind, const DocumentSourceIdentity & source, std::string contextId)
{
    return {
        .kind = kind,
        .contextKind = assets::AssetSourceContext::Kind::metadataView,
        .source = source,
        .contextId = std::move(contextId),
    };
}

void restoreMetadataAssetState(WorkbenchState & state)
{
    if (state.assetStateRestored)
    {
        return;
    }
    const auto radarContext =
        metadataContext(DocumentKind::radarcol, state.providers.radarColors().status().sourceIdentity, "view.radarcol");
    const auto texmapContext =
        metadataContext(DocumentKind::texmapPair, state.providers.textures().status().sourceIdentity, "view.texmaps");
    const auto tiledataContext =
        metadataContext(DocumentKind::tiledata, state.providers.tiledata().status().sourceIdentity, "view.tiledata");
    if (const auto * record =
            assets::findAssetState(state.assetStateRecords, state.activeProfile.name, "default", radarContext);
        record != nullptr)
    {
        assets::restoreAssetState(*record, state.radarcolState.assetView);
    }
    if (const auto * record =
            assets::findAssetState(state.assetStateRecords, state.activeProfile.name, "default", texmapContext);
        record != nullptr)
    {
        assets::restoreAssetState(*record, state.texmapsState.assetView);
    }
    if (const auto * record =
            assets::findAssetState(state.assetStateRecords, state.activeProfile.name, "default", tiledataContext);
        record != nullptr)
    {
        assets::restoreAssetState(*record, state.tiledataState.assetView);
    }
    state.assetStateRestored = true;
}

void updateAssetStateRecord(std::vector<assets::AssetStateRecord> & records, assets::AssetStateRecord record)
{
    const auto existing = std::ranges::find_if(records,
                                               [&record](const assets::AssetStateRecord & candidate)
                                               {
                                                   return candidate.profileIdentity == record.profileIdentity &&
                                                          candidate.workspaceIdentity == record.workspaceIdentity &&
                                                          candidate.context == record.context;
                                               });
    if (existing == records.end())
    {
        records.push_back(std::move(record));
    }
    else
    {
        *existing = std::move(record);
    }
}

void applyPendingPersistence(WorkbenchState & state)
{
    if (state.menuBarState.preferencesDialog.profileDeleteRequested)
    {
        const std::string profileName = *state.menuBarState.preferencesDialog.profileDeleteRequested;
        try
        {
            state.profileStore.remove(profileName);
            state.menuBarState.preferencesDialog.profileDeleteRequested.reset();
            if (state.activeProfile.name == profileName)
            {
                state.activeProfile = {};
                state.preferences.activeDataProfile.clear();
                applyProfileChange(state);
            }
        }
        catch (const std::exception & error)
        {
            state.menuBarState.preferencesDialog.profileDeleteRequested.reset();
            state.notifications.add(NotificationSeverity::error, "Profile deletion failed: " + std::string(error.what()));
        }
    }
    if (state.menuBarState.preferencesDialog.profileSaveRequested)
    {
        try
        {
            state.profileStore.save(state.menuBarState.preferencesDialog.workingProfile);
            state.menuBarState.preferencesDialog.profileSaveRequested = false;
        }
        catch (const std::exception & error)
        {
            state.notifications.add(NotificationSeverity::error, "Profile save failed: " + std::string(error.what()));
        }
    }
    if (state.preferences != state.persistedPreferences)
    {
        savePreferences(state.preferences);
        state.persistedPreferences = state.preferences;
    }
    if (state.assetStateResetRequested)
    {
        assets::resetAssetState(state.assetStateRecords);
        assets::resetContext(state.radarcolState.assetView);
        assets::resetContext(state.texmapsState.assetView);
        assets::resetContext(state.tiledataState.assetView);
        for (auto & [id, assetView] : state.radarcolState.documentAssetViews)
        {
            static_cast<void>(id);
            assets::resetContext(assetView);
        }
        state.assetStateResetRequested = false;
        state.assetStateRestored = true;
    }

    const auto radarContext =
        metadataContext(DocumentKind::radarcol, state.providers.radarColors().status().sourceIdentity, "view.radarcol");
    const auto texmapContext =
        metadataContext(DocumentKind::texmapPair, state.providers.textures().status().sourceIdentity, "view.texmaps");
    updateAssetStateRecord(
        state.assetStateRecords,
        assets::captureAssetState(state.activeProfile.name, "default", radarContext, state.radarcolState.assetView));
    updateAssetStateRecord(
        state.assetStateRecords,
        assets::captureAssetState(state.activeProfile.name, "default", texmapContext, state.texmapsState.assetView));
    updateAssetStateRecord(
        state.assetStateRecords,
        assets::captureAssetState(
            state.activeProfile.name,
            "default",
            metadataContext(DocumentKind::tiledata, state.providers.tiledata().status().sourceIdentity, "view.tiledata"),
            state.tiledataState.assetView));
}

[[nodiscard]] bool welcomeShell(const WorkbenchState & state) noexcept
{
    return state.session.startupState() == StartupState::mapWelcome && state.workspace.reader() == nullptr &&
           !state.session.hasDocuments();
}

void updateNativeWindowMode(GLFWwindow * window, WorkbenchState & state, bool welcome)
{
    if (state.nativeWelcomeWindowMode == welcome)
    {
        return;
    }

    glfwSetWindowAttrib(window, GLFW_DECORATED, welcome ? GLFW_FALSE : GLFW_TRUE);
    glfwSetWindowAttrib(window, GLFW_RESIZABLE, welcome ? GLFW_FALSE : GLFW_TRUE);
    state.nativeWelcomeWindowMode = welcome;
}

[[nodiscard]] bool drawWelcomeOrWorkbench(logger::GuiLogSink & logSink, WorkbenchState & state, bool welcome)
{
    bool profileChanged = false;
    const float bottomPanelHeight =
        welcome ? 0.0F : drawLogPanel(logSink, state.preferences, state.logEntries, state.followLogScroll);
    if (state.session.startupState() == StartupState::configureProfile)
    {
        drawStartupScreen(state);
    }
    else if (welcome)
    {
        state.recentMapRecordedPath.clear();
        drawWelcomeScreen(state);
        notifyRadarcolFallback(state);
        profileChanged = drawMenuBar(state, false);
    }
    else
    {
        state.welcomeGreetingSelected = false;
        state.welcomeGreeting.clear();
        drawDockSpace(state, bottomPanelHeight);
        drawDocumentSurface(state);
        drawPendingAssetCloseDialog(state);
        applyPendingDocumentAction(state);
        processDocumentIntents(state);
        drawMapPanels(state, state.blockInspectorState, state.radarcolState, state.texmapsState);
        notifyRadarcolFallback(state);
        if (state.workspace.reader() != nullptr && state.recentMapRecordedPath != state.workspace.mapPath())
        {
            state.session.recordRecentFile(state.workspace.mapPath(), state.workspace.dimensions());
            state.recentMapRecordedPath = state.workspace.mapPath();
        }
    }
    if (state.workspace.reader() != nullptr)
    {
        drawStatusBar(state, bottomPanelHeight);
    }
    drawNotifications(state.notifications);
    return profileChanged;
}

} // namespace

void drawWorkbenchFrame(GLFWwindow * window, logger::GuiLogSink & logSink, WorkbenchState & state)
{
    beginGuiFrame();
    bool welcome = welcomeShell(state);
    const bool profileChanged = welcome ? false : drawMenuBar(state, true);
    applyPendingClipboard(window, state);
    if (profileChanged)
    {
        applyProfileChange(state);
    }

    state.session.syncDocument(state.workspace);

    const bool wasWelcome = welcome;
    welcome = welcomeShell(state);
    syncProviders(state);
    restoreMetadataAssetState(state);
    updateNativeWindowMode(window, state, welcome);
    if (!welcome)
    {
        const bool enteredWorkbench = wasWelcome && !welcome;
        if (enteredWorkbench || !state.workbenchLayoutApplied)
        {
            static_cast<void>(applyWorkbenchLayout(window, state));
        }
        toggleFullscreen(window, state);
    }
    dispatchShortcuts(state, !welcome);
    const bool welcomeProfileChanged = drawWelcomeOrWorkbench(logSink, state, welcome);
    if (welcomeProfileChanged)
    {
        applyProfileChange(state);
    }
    applyPendingPreferenceChanges(state);
    applyPendingPersistence(state);
    drawToolCursor(window, state);
}

} // namespace uo_mule::gui
