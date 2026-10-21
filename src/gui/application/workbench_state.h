#pragma once

#include "core/gui_log_sink.h"
#include "gui/commands/command_registry.h"
#include "gui/profile/data_profile.h"
#include "gui/profile/data_profile_store.h"
#include "gui/providers/data_providers.h"
#include "gui/dialogs/preferences_dialog.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/dialogs/file_selector_dialog.h"
#include "gui/documents/map_workspace.h"
#include "gui/dialogs/new_map_dialog.h"
#include "gui/dialogs/dialog_host.h"
#include "gui/context/notification_state.h"
#include "gui/commands/operation_controller.h"
#include "gui/views/block_inspector_panel.h"
#include "gui/tools/edit_tools_panel.h"
#include "gui/views/map_canvas_panel.h"
#include "gui/views/radarcol_panel.h"
#include "gui/views/selection_stats_panel.h"
#include "gui/views/texmaps_panel.h"
#include "gui/views/panel_registry.h"
#include "gui/persistence/preferences.h"
#include "gui/profile/workbench_session.h"
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
    std::string dockingIni;
    bool layoutResetRequested = false;
    std::optional<DocumentId> pendingDocumentActivation;
    std::optional<DocumentId> pendingDocumentClose;
    bool pendingDocumentCloseDirty = false;
    MenuBarState menuBarState;
    DialogHostState dialogHost;
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
    std::uint64_t radarcolNotificationRevision = 0;
};

} // namespace uo_mule::gui
