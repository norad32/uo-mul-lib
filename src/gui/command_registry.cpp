#include "gui/command_registry.h"
#include "gui/dialogs/data_paths_dialog.h"
#include "gui/map_workspace.h"
#include "gui/new_map_dialog.h"
#include "gui/panels/common.h"
#include "gui/preferences.h"
#include "gui/panel_registry.h"
#include "gui/workbench_state.h"
#include "gui/workbench_session.h"
#include "gui/panels/map_canvas_panel.h"
#include <functional>
#include <string_view>
#include <utility>
#include <vector>

namespace uo_mule::gui
{
namespace
{

CommandAvailability enabled(const WorkbenchState & state)
{
    static_cast<void>(state);
    return {.enabled = true, .disabledReason = {}};
}

CommandAvailability requiresMapReadyProfile(const WorkbenchState & state)
{
    if (state.startupLoaded && !state.session.mapReady())
    {
        return {.enabled = false, .disabledReason = "Configure a valid radarcol.mul profile first"};
    }
    return enabled(state);
}

CommandAvailability requiresMapDocument(const WorkbenchState & state)
{
    if (state.workspace.reader() == nullptr)
    {
        return {.enabled = false, .disabledReason = "No map is open"};
    }
    return enabled(state);
}

CommandAvailability requiresMap(const WorkbenchState & state)
{
    if (state.workspace.reader() == nullptr)
    {
        return {.enabled = false, .disabledReason = "No map is open"};
    }
    return enabled(state);
}

CommandAvailability requiresMapForSave(const WorkbenchState & state)
{
    if (state.workspace.reader() == nullptr)
    {
        return {.enabled = false, .disabledReason = "No map is open"};
    }
    return enabled(state);
}

void openDialog(WorkbenchState & state, MapFileDialogMode mode)
{
    state.menuBarState.fileDialog.mode = mode;
    state.menuBarState.fileDialog.activePopup = MapFileDialogMode::none;
    state.menuBarState.fileDialog.error.clear();
    state.menuBarState.fileDialog.openPickerRequested = mode == MapFileDialogMode::openMap;
    if (mode == MapFileDialogMode::openMap)
    {
        state.menuBarState.fileDialog.openPath.fill('\0');
    }
}

void requestClose(WorkbenchState & state, PendingMapOperationKind kind)
{
    requestCloseOperation(state.workspace,
                          state.menuBarState.fileDialog,
                          state.operationController,
                          kind,
                          state.notifications);
}

void togglePanel(bool & visibility)
{
    visibility = !visibility;
}

bool & panelVisibility(WorkbenchState & state, PanelId id)
{
    return state.panelRegistry.find(id)->visible;
}

void add(CommandRegistry & registry,
         CommandId id,
         std::string stableId,
         std::string label,
         std::string category,
         std::string description,
         std::string defaultShortcut,
         std::function<CommandAvailability(const WorkbenchState &)> availability,
         std::function<void(WorkbenchState &)> execute)
{
    registry.registerCommand({.id = id,
                              .stableId = std::move(stableId),
                              .label = std::move(label),
                              .category = std::move(category),
                              .description = std::move(description),
                              .defaultShortcut = std::move(defaultShortcut),
                              .availability = std::move(availability),
                              .execute = std::move(execute),
                             });
}

} // namespace

void CommandRegistry::registerCommand(Command command)
{
    commands_.push_back(std::move(command));
}

const Command * CommandRegistry::find(CommandId id) const noexcept
{
    for (const Command & command : commands_)
    {
        if (command.id == id)
        {
            return &command;
        }
    }
    return nullptr;
}

const Command * CommandRegistry::find(std::string_view stableId) const noexcept
{
    for (const Command & command : commands_)
    {
        if (command.stableId == stableId)
        {
            return &command;
        }
    }
    return nullptr;
}

CommandAvailability CommandRegistry::availability(CommandId id, const WorkbenchState & state) const
{
    const Command * command = find(id);
    if (command == nullptr || !command->availability)
    {
        return {.enabled = false, .disabledReason = "Command is not registered"};
    }
    return command->availability(state);
}

bool CommandRegistry::execute(CommandId id, WorkbenchState & state) const
{
    const Command * command = find(id);
    if (command == nullptr || !command->execute || !availability(id, state).enabled)
    {
        return false;
    }
    command->execute(state);
    return true;
}

bool CommandRegistry::executeShortcut(std::string_view shortcut, WorkbenchState & state) const
{
    for (const Command & command : commands_)
    {
        if (command.defaultShortcut == shortcut)
        {
            return execute(command.id, state);
        }
    }
    return false;
}

const std::vector<Command> & CommandRegistry::commands() const noexcept
{
    return commands_;
}

void registerDefaultCommands(CommandRegistry & registry)
{
    add(registry, CommandId::fileOpen, "file.open", "Open...", "File", "Open a map", "Ctrl+O", requiresMapReadyProfile,
        [](WorkbenchState & state) { openDialog(state, MapFileDialogMode::openMap); });
    add(registry, CommandId::fileNew, "file.new", "Create Map...", "File", "Create an empty map", "Ctrl+N", requiresMapReadyProfile,
        [](WorkbenchState & state) { openDialog(state, MapFileDialogMode::newMap); });
    add(registry,
        CommandId::fileSaveAs,
        "file.save-as",
        "Save As / Commit...",
        "File",
        "Save staged map changes",
        "Ctrl+Shift+S",
        requiresMapForSave,
        [](WorkbenchState & state)
        {
            if (state.workspace.dirty())
            {
                openDialog(state, MapFileDialogMode::saveAs);
            }
            else
            {
                state.menuBarState.fileDialog.error = "There are no staged changes to save";
                openDialog(state, MapFileDialogMode::error);
            }
        });
    add(registry,
        CommandId::fileDiscard,
        "file.discard",
        "Discard",
        "File",
        "Discard staged map changes",
        "",
        [](const WorkbenchState & state)
        { return state.workspace.dirty() ? enabled(state) : CommandAvailability{.enabled = false, .disabledReason = "There are no staged changes"}; },
        [](WorkbenchState & state)
        {
            const auto discarded = state.workspace.discard();
            if (!discarded)
            {
                openDialog(state, MapFileDialogMode::error);
                state.menuBarState.fileDialog.error = formatError(discarded.error());
            }
            else
            {
                state.menuBarState.fileDialog.error.clear();
            }
        });
    add(registry, CommandId::fileClose, "file.close", "Close", "File", "Close the current map", "", enabled,
        [](WorkbenchState & state) { requestClose(state, PendingMapOperationKind::close); });
    add(registry, CommandId::fileQuit, "file.quit", "Quit", "File", "Close the map and quit", "Ctrl+Q", enabled,
        [](WorkbenchState & state) { requestClose(state, PendingMapOperationKind::quit); });

    add(registry, CommandId::viewTileIds, "view.display.tile-ids", "Tile IDs", "View", "Display tile IDs", "", enabled,
        [](WorkbenchState & state) { state.workspace.setDisplayMode(MapDisplayMode::tileIds); });
    add(registry, CommandId::viewZ, "view.display.z", "Z", "View", "Display elevation", "", enabled,
        [](WorkbenchState & state) { state.workspace.setDisplayMode(MapDisplayMode::z); });
    add(registry, CommandId::viewCombined, "view.display.combined", "Combined", "View", "Display tile IDs and elevation", "", enabled,
        [](WorkbenchState & state) { state.workspace.setDisplayMode(MapDisplayMode::combined); });
    add(registry,
        CommandId::viewToggleLogPanel,
        "view.logs.toggle",
        "Logs",
        "View",
        "Show or hide the log panel",
        "`",
        enabled,
        [](WorkbenchState & state)
        {
            state.preferences.showLogPanel = !state.preferences.showLogPanel;
            savePreferences(state.preferences);
        });

    add(registry, CommandId::panelMapCanvas, "view.panel.map-canvas", "Map Canvas", "View", "Show or hide the map canvas", "",
        enabled, [](WorkbenchState & state) { togglePanel(panelVisibility(state, PanelId::mapCanvas)); });
    add(registry, CommandId::panelTileInspector, "view.panel.tile-inspector", "Tile Inspector", "View", "Show or hide the tile inspector", "",
        enabled, [](WorkbenchState & state) { togglePanel(panelVisibility(state, PanelId::tileInspector)); });
    add(registry, CommandId::panelBlockInspector, "view.panel.block-inspector", "Block Inspector", "View", "Show or hide the block inspector", "",
        enabled, [](WorkbenchState & state) { togglePanel(panelVisibility(state, PanelId::blockInspector)); });
    add(registry, CommandId::panelSelectionStatistics, "view.panel.selection-statistics", "Selection Statistics", "View", "Show or hide selection statistics", "",
        enabled, [](WorkbenchState & state) { togglePanel(panelVisibility(state, PanelId::selectionStatistics)); });
    add(registry, CommandId::panelMapInformation, "view.panel.map-information", "Map Information", "View", "Show or hide map information", "",
        enabled, [](WorkbenchState & state) { togglePanel(panelVisibility(state, PanelId::mapInformation)); });
    add(registry, CommandId::panelEditTools, "view.panel.edit-tools", "Edit Tools", "View", "Show or hide edit tools", "",
        enabled, [](WorkbenchState & state) { togglePanel(panelVisibility(state, PanelId::editTools)); });
    add(registry, CommandId::panelRadarcol, "view.panel.radarcol", "Radarcol", "View", "Show or hide radar colors", "",
        enabled, [](WorkbenchState & state) { togglePanel(panelVisibility(state, PanelId::radarcol)); });
    add(registry, CommandId::panelTexmaps, "view.panel.texmaps", "Texmaps", "View", "Show or hide textures", "",
        enabled, [](WorkbenchState & state) { togglePanel(panelVisibility(state, PanelId::texmaps)); });

    add(registry, CommandId::selectionTiles, "selection.tiles", "Tile selection", "Selection", "Select individual tiles", "", enabled,
        [](WorkbenchState & state) { state.workspace.setSelectionMode(MapSelectionMode::tiles); });
    add(registry, CommandId::selectionBlocks, "selection.blocks", "Block selection", "Selection", "Select map blocks", "", enabled,
        [](WorkbenchState & state) { state.workspace.setSelectionMode(MapSelectionMode::blocks); });
    add(registry, CommandId::editToggle, "edit.toggle", "Edit mode", "Edit", "Toggle map editing", "", requiresMap,
        [](WorkbenchState & state) { state.workspace.setEditMode(!state.workspace.editMode()); });
    add(registry, CommandId::dataPaths, "data.paths", "UO Data Paths...", "Data", "Configure the active UO data profile", "", enabled,
        [](WorkbenchState & state)
        {
            state.session.setStartupState(StartupState::configureProfile);
            requestDataPathsDialog(state.menuBarState.dataPathsDialog, state.activeProfile);
        });
    add(registry, CommandId::resetLayout, "view.reset-layout", "Reset Layout", "View", "Restore the default workbench layout", "", enabled,
        [](WorkbenchState & state)
        {
            state.panelRegistry.resetToDefaults();
            state.layoutResetRequested = true;
        });
    add(registry, CommandId::mapFit, "map.fit", "Fit Map", "Map", "Fit the complete map in the canvas", "", requiresMapDocument,
        [](WorkbenchState & state) { state.mapCanvasState.navigationRequest.request = MapCanvasViewRequest::fitMap; });
    add(registry,
        CommandId::mapFitSelection,
        "map.fit-selection",
        "Fit Selection",
        "Map",
        "Fit the current selection in the canvas",
        "",
        requiresMapDocument,
        [](WorkbenchState & state) { state.mapCanvasState.navigationRequest.request = MapCanvasViewRequest::fitSelection; });
    add(registry,
        CommandId::mapCenterSelection,
        "map.center-selection",
        "Center Selection",
        "Map",
        "Center the current selection in the canvas",
        "",
        requiresMapDocument,
        [](WorkbenchState & state) { state.mapCanvasState.navigationRequest.request = MapCanvasViewRequest::centerSelection; });
    add(registry,
        CommandId::mapResetView,
        "map.reset-view",
        "Reset View",
        "Map",
        "Reset map pan and zoom",
        "",
        requiresMapDocument,
        [](WorkbenchState & state) { state.mapCanvasState.navigationRequest.request = MapCanvasViewRequest::resetView; });
    add(registry,
        CommandId::mapGoToCoordinate,
        "map.go-to-coordinate",
        "Go to Coordinate...",
        "Map",
        "Center the map on a tile or block coordinate",
        "Ctrl+G",
        requiresMapDocument,
        [](WorkbenchState & state) { state.mapCanvasState.coordinatePopupRequested = true; });
}

} // namespace uo_mule::gui
