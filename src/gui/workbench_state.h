#pragma once

#include "core/gui_log_sink.h"
#include "gui/command_registry.h"
#include "gui/data_profile.h"
#include "gui/data_profile_store.h"
#include "gui/data_providers.h"
#include "gui/dialogs/preferences_dialog.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/dialogs/file_selector_dialog.h"
#include "gui/map_workspace.h"
#include "gui/new_map_dialog.h"
#include "gui/notification_state.h"
#include "gui/operation_controller.h"
#include "gui/panels/block_inspector_panel.h"
#include "gui/panels/edit_tools_panel.h"
#include "gui/panels/map_canvas_panel.h"
#include "gui/panels/radarcol_panel.h"
#include "gui/panels/selection_stats_panel.h"
#include "gui/panels/texmaps_panel.h"
#include "gui/panel_registry.h"
#include "gui/preferences.h"
#include "gui/workbench_session.h"
#include "gui/view_registry.h"
#include <vector>
#include <string>
#include <optional>
#include <filesystem>

namespace uo_mule::gui
{

struct MenuBarState
{
    NewMapDialogState fileDialog;
    PreferencesDialogState preferencesDialog;
    bool windowCloseRequested = false;
};

struct WorkbenchState
{
    explicit WorkbenchState(Preferences initialPreferences = {});
    ~WorkbenchState() = default;
    WorkbenchState(const WorkbenchState &) = delete;
    WorkbenchState & operator=(const WorkbenchState &) = delete;
    WorkbenchState(WorkbenchState &&) = delete;
    WorkbenchState & operator=(WorkbenchState &&) = delete;

    Preferences preferences;
    DataProfileStore profileStore;
    DataProfile activeProfile;
    DataProviders providers;
    WorkbenchSession session;
    MapWorkspace workspace;
    CommandRegistry commands;
    OperationController operationController;
    NotificationState notifications;
    PanelRegistry panelRegistry;
    ViewRegistry views;
    std::string dockingIni;
    bool layoutResetRequested = false;
    MenuBarState menuBarState;
    FileBrowserState fileBrowser;
    FileSelectorState fileSelector;
    MapCanvasPanelState mapCanvasState;
    std::optional<MapCanvasStatus> lastHoveredCanvasStatus;
    std::string pendingClipboardText;
    BlockInspectorPanelState blockInspectorState;
    SelectionStatsPanelState selectionStatsState;
    EditToolsPanelState editToolsState;
    RadarcolPanelState radarcolState;
    TexmapsPanelState texmapsState;
    std::vector<core::GuiLogEntry> logEntries;
    bool followLogScroll = true;
    bool shouldClose = false;
    bool startupLoaded = false;
    bool nativeWelcomeWindowMode = false;
    std::filesystem::path recentMapRecordedPath;
    std::string welcomeGreeting;
    bool welcomeGreetingSelected = false;
};

} // namespace uo_mule::gui
