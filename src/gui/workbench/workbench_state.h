#pragma once

#include "gui/commands/command_registry.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/dialogs/file_selector_dialog.h"
#include "gui/dialogs/map_workflow_dialog.h"
#include "gui/dialogs/preferences_dialog.h"
#include "gui/panels/block_inspector_panel.h"
#include "gui/documents/radarcol_document_window.h"
#include "gui/documents/texmaps_document_window.h"
#include "gui/documents/tiledata_document_window.h"
#include "gui/application/layout_store.h"
#include "gui/workbench/notification_state.h"
#include "gui/documents/map_document_window.h"
#include "gui_model/dialogs/dialog_host.h"
#include "gui_model/documents/document_descriptors.h"
#include "gui_model/documents/map_document_manager.h"
#include "gui_model/layout/panel_registry.h"
#include "gui_model/lifecycle/document_lifecycle.h"
#include "gui_model/operations/operation_controller.h"
#include "gui_model/persistence/asset_state_store.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/profile/data_profile_store.h"
#include "gui_model/providers/data_providers.h"
#include "gui_model/session/workbench_session.h"
#include "gui_model/tools/tool_contracts.h"
#include "gui_model/tools/tool_state.h"
#include "logger/gui_log_sink.h"
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace uo_mule::gui
{

struct MenuBarState
{
    NewFileDialogState fileDialog;
    PreferencesDialogState preferencesDialog;
    bool windowCloseRequested = false;
};

struct PendingPreferenceChanges
{
    std::optional<bool> texmapsHideUnused;
    std::optional<bool> texmapsTablePresentation;

    [[nodiscard]] bool empty() const noexcept { return !texmapsHideUnused && !texmapsTablePresentation; }
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
    Preferences persistedPreferences;
    DataProfileStore profileStore;
    DataProfile activeProfile;
    DataProviders providers;
    WorkbenchSession session;
    DocumentDescriptorRegistry documentDescriptors;
    MapDocumentManager mapDocuments;
    CommandRegistry commands;
    ToolRegistry toolRegistry;
    ToolState toolState;
    OperationController operationController;
    NotificationState notifications;
    PanelRegistry panelRegistry;
    AppLayout layout;
    bool layoutResetRequested = false;
    bool fullscreenToggleRequested = false;
    bool workbenchLayoutApplied = false;
    DocumentLifecycleController documentLifecycle;
    std::optional<DocumentId> pendingAssetCloseDecision;
    std::optional<PendingChangesAction> pendingAssetCloseAction;
    PendingPreferenceChanges pendingPreferenceChanges;
    std::optional<DataProfile> pendingProfileSave;
    std::vector<std::function<void()>> deferredWorkflowActions;
    MenuBarState menuBarState;
    DialogHostState dialogHost;
    FileBrowserState fileBrowser;
    FileSelectorState fileSelector;
    std::optional<MapCanvasStatus> lastHoveredCanvasStatus;
    std::string hoveredPanelContext;
    std::string activeAssetPanelContext;
    std::string operationStatus;
    NotificationSeverity operationStatusSeverity = NotificationSeverity::info;
    std::string pendingClipboardText;
    BlockInspectorPanelState blockInspectorState;
    RadarcolDocumentWindowState radarcolState;
    TexmapsDocumentWindowState texmapsState;
    TiledataDocumentWindowState tiledataState;
    std::vector<logger::GuiLogEntry> logEntries;
    bool followLogScroll = true;
    bool shouldClose = false;
    bool startupLoaded = false;
    bool nativeWelcomeWindowMode = false;
    std::filesystem::path recentMapRecordedPath;
    std::string welcomeGreeting;
    bool welcomeGreetingSelected = false;
    std::uint64_t radarcolNotificationRevision = 0;
    ProviderRevisions providerRevisions;
    std::vector<assets::AssetStateRecord> assetStateRecords;
    bool assetStateResetRequested = false;
    bool assetStateRestored = false;

    void deferWorkflowAction(std::function<void()> action)
    {
        deferredWorkflowActions.push_back(std::move(action));
    }
};

} // namespace uo_mule::gui
