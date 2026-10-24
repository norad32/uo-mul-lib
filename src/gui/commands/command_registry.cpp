#include "gui/commands/command_registry.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/dialogs/map_workflow_dialog.h"
#include "gui/dialogs/preferences_dialog.h"
#include "gui/presentation/strings/ui_strings.h"
#include "gui/state/workbench_state.h"
#include "gui/views/world/map_canvas_view.h"
#include "gui_model/dialogs/dialog_contracts.h"
#include "gui_model/documents/radarcol_document.h"
#include "gui_model/input/coordinate_parser.h"
#include "gui_model/layout/panel_registry.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/session/workbench_session.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/tools/tool_state.h"
#include "gui_model/workspaces/map_workspace.h"
#include "gui_model/world/map_canvas_state.h"
#include "gui_model/world/map_context.h"
#include <algorithm>
#include <cstddef>
#include <functional>
#include <string>
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
    static_cast<void>(state);
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

ToolContext toolContext(const WorkbenchState & state)
{
    const DocumentRecord * active = state.session.activeDocumentRecord();
    const bool mapOpen = active != nullptr && active->summary.identity.kind == DocumentKind::map &&
                         state.workspace.reader() != nullptr;
    return {
        .mapDocumentOpen = mapOpen,
        .mapEditable = mapOpen && active->summary.access == DocumentAccess::editable,
        .selectionGranularity = state.toolState.selectionGranularity(),
    };
}

CommandAvailability requiresTool(ToolId tool, const WorkbenchState & state)
{
    const ToolAvailability available = state.toolRegistry.availability(tool, toolContext(state));
    return {.enabled = available.enabled, .disabledReason = available.disabledReason};
}

void activateTool(WorkbenchState & state, ToolId tool)
{
    static_cast<void>(state.toolState.activate(tool, toolContext(state), state.toolRegistry));
}

CommandAvailability requiresActiveLocation(const WorkbenchState & state)
{
    const MapContextSnapshot context = state.session.contextSnapshot(state.workspace);
    if (!context.activeTile || !context.activeValue)
    {
        return {.enabled = false, .disabledReason = "No map location is active"};
    }
    return enabled(state);
}

CommandAvailability requiresMapForSave(const WorkbenchState & state)
{
    if (state.workspace.reader() == nullptr || !state.workspace.dirty())
    {
        return {
            .enabled = false,
            .disabledReason = state.workspace.reader() == nullptr ? "No map is open" : "There are no staged changes",
        };
    }
    return enabled(state);
}

CommandAvailability requiresSaveableDocument(const WorkbenchState & state)
{
    const auto * active = state.session.activeDocumentRecord();
    if (active == nullptr)
    {
        return {.enabled = false, .disabledReason = "No document is open"};
    }
    if (active->map)
    {
        return requiresMapForSave(state);
    }
    if (active->radarcol)
    {
        if (active->radarcol->readOnly())
        {
            return {.enabled = false, .disabledReason = "Configured Radarcol sources are read-only"};
        }
        if (!active->radarcol->editing() || !active->radarcol->dirty())
        {
            return {.enabled = false, .disabledReason = "There are no staged changes"};
        }
        return enabled(state);
    }
    return {.enabled = false, .disabledReason = "This document cannot be edited"};
}

void openDialog(WorkbenchState & state, MapFileDialogMode mode)
{
    state.menuBarState.fileDialog.mode = mode;
    state.menuBarState.fileDialog.activePopup = MapFileDialogMode::none;
    state.menuBarState.fileDialog.openWithoutReview = false;
    state.menuBarState.fileDialog.error.clear();
    if (mode == MapFileDialogMode::openMap)
    {
        state.menuBarState.fileDialog.openPath.fill('\0');
        requestFileBrowser(state.fileBrowser,
                           {
                               .mode = FileBrowserMode::openFile,
                               .target = FileBrowserTarget::genericOpen,
                               .initialPath = {},
                               .extension = ".mul",
                               .title = "Open MUL File",
                           });
    }
}

void requestClose(WorkbenchState & state, PendingMapOperationKind kind)
{
    requestCloseOperation(state.workspace, state.menuBarState.fileDialog, state.operationController, kind, state.notifications);
}

void togglePanel(bool & visibility)
{
    visibility = !visibility;
}

void requestCoordinateDialog(WorkbenchState & state, CoordinateKind kind)
{
    state.workspace.canvasState().coordinateInputKind = kind;
    state.workspace.canvasState().coordinateInputFocusRequested = true;
    state.workspace.canvasState().coordinateInput.fill('\0');
    state.workspace.canvasState().coordinatePopupRequested = true;
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
    registry.registerCommand({
        .id = id,
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

CommandId toolCommandId(ToolId tool) noexcept
{
    switch (tool)
    {
    case ToolId::move:
        return CommandId::toolMove;
    case ToolId::rectangleSelect:
        return CommandId::toolRectangleSelect;
    case ToolId::lassoSelect:
        return CommandId::toolLassoSelect;
    case ToolId::fuzzySelect:
        return CommandId::toolFuzzySelect;
    case ToolId::fill:
        return CommandId::toolFill;
    case ToolId::replace:
        return CommandId::toolReplace;
    }
    return CommandId::toolMove;
}

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
    add(registry,
        CommandId::toolMove,
        "tool.move",
        "Pan",
        "Tools",
        "Pan the world view",
        "P",
        [](const WorkbenchState & state) { return requiresTool(ToolId::move, state); },
        [](WorkbenchState & state) { activateTool(state, ToolId::move); });
    add(registry,
        CommandId::toolRectangleSelect,
        "tool.rectangle-select",
        "Rectangle Select",
        "Tools",
        "Select a rectangular tile or block area",
        "R",
        [](const WorkbenchState & state) { return requiresTool(ToolId::rectangleSelect, state); },
        [](WorkbenchState & state) { activateTool(state, ToolId::rectangleSelect); });
    add(registry,
        CommandId::toolLassoSelect,
        "tool.lasso-select",
        "Free Select",
        "Tools",
        "Select a freehand tile or block area",
        "F",
        [](const WorkbenchState & state) { return requiresTool(ToolId::lassoSelect, state); },
        [](WorkbenchState & state) { activateTool(state, ToolId::lassoSelect); });
    add(registry,
        CommandId::toolFuzzySelect,
        "tool.fuzzy-select",
        "Fuzzy Select",
        "Tools",
        "Select matching connected or global map tiles",
        "U",
        [](const WorkbenchState & state) { return requiresTool(ToolId::fuzzySelect, state); },
        [](WorkbenchState & state) { activateTool(state, ToolId::fuzzySelect); });
    add(registry,
        CommandId::toolFill,
        "tool.fill",
        "Fill",
        "Tools",
        "Fill the current selection from the active tile",
        "Shift+B",
        [](const WorkbenchState & state) { return requiresTool(ToolId::fill, state); },
        [](WorkbenchState & state) { activateTool(state, ToolId::fill); });
    add(registry,
        CommandId::toolReplace,
        "tool.replace",
        "Replace",
        "Tools",
        "Replace matching IDs or elevations in the selection",
        "Shift+R",
        [](const WorkbenchState & state) { return requiresTool(ToolId::replace, state); },
        [](WorkbenchState & state) { activateTool(state, ToolId::replace); });

    add(registry,
        CommandId::fileOpen,
        "file.open",
        "Open...",
        "File",
        "Open a supported MUL file",
        "Ctrl+O",
        requiresMapReadyProfile,
        [](WorkbenchState & state) { openDialog(state, MapFileDialogMode::openMap); });
    add(registry,
        CommandId::fileNew,
        "file.new",
        "New...",
        "File",
        "Create a new supported asset",
        "Ctrl+N",
        requiresMapReadyProfile,
        [](WorkbenchState & state) { openDialog(state, MapFileDialogMode::newType); });
    add(registry,
        CommandId::fileSave,
        "file.save",
        "Save",
        "File",
        "Save staged changes in place",
        "Ctrl+S",
        requiresSaveableDocument,
        [](WorkbenchState & state)
        {
            auto * active = state.session.activeDocumentRecord();
            if (active == nullptr)
            {
                return;
            }
            if (active->map)
            {
                const auto saved = state.workspace.save(state.workspace.originalPath(), false);
                if (!saved)
                {
                    state.menuBarState.fileDialog.error = strings::formatError(saved.error());
                    state.menuBarState.fileDialog.mode = MapFileDialogMode::error;
                }
                return;
            }
            if (active->radarcol)
            {
                active->radarcol->requestIntent({
                    .kind = RadarcolDocumentIntentKind::save,
                    .colorIndex = 0,
                    .color = 0,
                    .outputPath = {},
                    .overwriteOutput = false,
                });
            }
        });
    add(registry,
        CommandId::fileSaveAs,
        "file.save-as",
        "Save As...",
        "File",
        "Save staged map changes",
        "Ctrl+Shift+S",
        requiresMapForSave,
        [](WorkbenchState & state)
        {
            if (state.workspace.dirty())
            {
                state.menuBarState.fileDialog.savePath.fill('\0');
                if (const auto path = state.workspace.originalPath(); !path.empty())
                {
                    const std::string value = path.string();
                    const std::size_t length = std::min(value.size(), state.menuBarState.fileDialog.savePath.size() - 1U);
                    std::copy_n(value.data(), length, state.menuBarState.fileDialog.savePath.data());
                    state.menuBarState.fileDialog.savePath.at(length) = '\0';
                }
                requestFileBrowser(state.fileBrowser,
                                   {
                                       .mode = FileBrowserMode::saveFile,
                                       .target = FileBrowserTarget::mapSaveAs,
                                       .initialPath = state.workspace.originalPath(),
                                       .extension = ".mul",
                                       .title = "Save Map As",
                                   });
                openDialog(state, MapFileDialogMode::saveAs);
            }
            else
            {
                state.menuBarState.fileDialog.error = "There are no staged changes to save";
                openDialog(state, MapFileDialogMode::error);
            }
        });
    add(
        registry,
        CommandId::fileDiscard,
        "file.discard",
        "Discard",
        "File",
        "Discard staged map changes",
        "",
        [](const WorkbenchState & state)
        {
            return state.workspace.dirty()
                       ? enabled(state)
                       : CommandAvailability{.enabled = false, .disabledReason = "There are no staged changes"};
        },
        [](WorkbenchState & state)
        {
            const auto discarded = state.workspace.discard();
            if (!discarded)
            {
                openDialog(state, MapFileDialogMode::error);
                state.menuBarState.fileDialog.error = strings::formatError(discarded.error());
            }
            else
            {
                state.menuBarState.fileDialog.error.clear();
            }
        });
    add(registry,
        CommandId::fileClose,
        "file.close",
        "Close",
        "File",
        "Close the current map",
        "",
        enabled,
        [](WorkbenchState & state) { requestClose(state, PendingMapOperationKind::close); });
    add(registry,
        CommandId::fileQuit,
        "file.quit",
        "Quit",
        "File",
        "Close the map and quit",
        "Ctrl+Q",
        enabled,
        [](WorkbenchState & state) { requestClose(state, PendingMapOperationKind::quit); });

    add(registry,
        CommandId::viewTileIds,
        "view.display.tile-ids",
        "Tile IDs",
        "View",
        "Display tile IDs",
        "",
        enabled,
        [](WorkbenchState & state) { state.workspace.setDisplayMode(MapDisplayMode::tileIds); });
    add(registry,
        CommandId::viewZ,
        "view.display.z",
        "Z",
        "View",
        "Display elevation",
        "",
        enabled,
        [](WorkbenchState & state) { state.workspace.setDisplayMode(MapDisplayMode::z); });
    add(registry,
        CommandId::viewCombined,
        "view.display.combined",
        "Combined",
        "View",
        "Display tile IDs and elevation",
        "",
        enabled,
        [](WorkbenchState & state) { state.workspace.setDisplayMode(MapDisplayMode::combined); });
    add(registry,
        CommandId::viewToggleLogPanel,
        "view.logs.toggle",
        "Logs",
        "View",
        "Show or hide the log panel",
        "`",
        enabled,
        [](WorkbenchState & state) { state.preferences.showLogPanel = !state.preferences.showLogPanel; });
    add(registry,
        CommandId::viewToggleFullscreen,
        "view.fullscreen.toggle",
        "Fullscreen",
        "View",
        "Toggle fullscreen workbench",
        "F11",
        enabled,
        [](WorkbenchState & state) { state.fullscreenToggleRequested = true; });

    add(registry,
        CommandId::panelToolPalette,
        "view.panel.tool-palette",
        "Tool Palette",
        "View",
        "Show or hide the tool palette",
        "",
        enabled,
        [](WorkbenchState & state) { togglePanel(panelVisibility(state, PanelId::toolPalette)); });
    add(registry,
        CommandId::panelTileInspector,
        "view.panel.tile-inspector",
        "Tile Inspector",
        "View",
        "Show or hide the tile inspector",
        "",
        enabled,
        [](WorkbenchState & state) { togglePanel(panelVisibility(state, PanelId::tileInspector)); });
    add(registry,
        CommandId::panelBlockInspector,
        "view.panel.block-inspector",
        "Block Inspector",
        "View",
        "Show or hide the block inspector",
        "",
        enabled,
        [](WorkbenchState & state) { togglePanel(panelVisibility(state, PanelId::blockInspector)); });
    add(registry,
        CommandId::panelRadarcol,
        "view.panel.radarcol",
        "Radarcol",
        "View",
        "Show or hide radar colors",
        "",
        enabled,
        [](WorkbenchState & state) { togglePanel(panelVisibility(state, PanelId::radarcol)); });
    add(registry,
        CommandId::panelTexmaps,
        "view.panel.texmaps",
        "Texmaps",
        "View",
        "Show or hide textures",
        "",
        enabled,
        [](WorkbenchState & state) { togglePanel(panelVisibility(state, PanelId::texmaps)); });
    add(registry,
        CommandId::panelTiledata,
        "view.panel.tiledata",
        "Tiledata",
        "View",
        "Show or hide Tiledata records",
        "",
        enabled,
        [](WorkbenchState & state) { togglePanel(panelVisibility(state, PanelId::tiledata)); });

    add(registry,
        CommandId::selectionTiles,
        "selection.tiles",
        "Tile selection",
        "Selection",
        "Select individual tiles",
        "",
        enabled,
        [](WorkbenchState & state) { state.workspace.setSelectionMode(MapSelectionMode::tiles); });
    add(registry,
        CommandId::selectionBlocks,
        "selection.blocks",
        "Block selection",
        "Selection",
        "Select map blocks",
        "",
        enabled,
        [](WorkbenchState & state) { state.workspace.setSelectionMode(MapSelectionMode::blocks); });
    add(registry,
        CommandId::selectionAll,
        "selection.all",
        "Select All",
        "Select",
        "Select all tiles or blocks at the active granularity",
        "Ctrl+A",
        requiresMapDocument,
        [](WorkbenchState & state) { state.workspace.selectAll(); });
    add(registry,
        CommandId::selectionNone,
        "selection.none",
        "Select None",
        "Select",
        "Clear the current selection",
        "Ctrl+Shift+A",
        requiresMapDocument,
        [](WorkbenchState & state) { state.workspace.clearSelection(); });
    add(registry,
        CommandId::selectionInvert,
        "selection.invert",
        "Invert",
        "Select",
        "Invert the current selection",
        "Ctrl+I",
        requiresMapDocument,
        [](WorkbenchState & state) { state.workspace.invertSelection(); });
    add(registry,
        CommandId::dataPaths,
        "data.paths",
        "Preferences...",
        "Data",
        "Configure the active UO data profile",
        "",
        enabled,
        [](WorkbenchState & state)
        { requestPreferencesDialog(state.menuBarState.preferencesDialog, state.activeProfile, state.preferences); });
    add(registry,
        CommandId::resetLayout,
        "view.reset-layout",
        "Reset Layout",
        "View",
        "Restore the default workbench layout",
        "",
        enabled,
        [](WorkbenchState & state)
        {
            state.panelRegistry.resetToDefaults();
            state.layout.dockingIni.clear();
            state.layout.window.geometryValid = false;
            state.preferences.workspaceFullscreen = false;
            state.layoutResetRequested = true;
        });
    add(registry,
        CommandId::resetAssetState,
        "view.reset-asset-state",
        "Reset Asset State",
        "View",
        "Clear saved metadata-view and asset-document presentation state",
        "",
        enabled,
        [](WorkbenchState & state) { state.assetStateResetRequested = true; });
    add(registry,
        CommandId::mapFit,
        "map.fit",
        "Fit Map",
        "Map",
        "Fit the complete map in the canvas",
        "",
        requiresMapDocument,
        [](WorkbenchState & state) { state.workspace.canvasState().navigationRequest.request = MapCanvasViewRequest::fitMap; });
    add(registry,
        CommandId::mapFitSelection,
        "map.fit-selection",
        "Fit Selection",
        "Map",
        "Fit the current selection in the canvas",
        "",
        requiresMapDocument,
        [](WorkbenchState & state)
        { state.workspace.canvasState().navigationRequest.request = MapCanvasViewRequest::fitSelection; });
    add(registry,
        CommandId::mapCenterSelection,
        "map.center-selection",
        "Center Selection",
        "Map",
        "Center the current selection in the canvas",
        "",
        requiresMapDocument,
        [](WorkbenchState & state)
        { state.workspace.canvasState().navigationRequest.request = MapCanvasViewRequest::centerSelection; });
    add(registry,
        CommandId::mapResetView,
        "map.reset-view",
        "Reset View",
        "Map",
        "Reset map pan and zoom",
        "",
        requiresMapDocument,
        [](WorkbenchState & state)
        { state.workspace.canvasState().navigationRequest.request = MapCanvasViewRequest::resetView; });
    add(registry,
        CommandId::mapGoToCoordinate,
        "map.go-to-coordinate",
        "Go to Coordinate...",
        "Map",
        "Center the map on a tile coordinate",
        "Ctrl+G",
        requiresMapDocument,
        [](WorkbenchState & state) { requestCoordinateDialog(state, CoordinateKind::tile); });
    add(registry,
        CommandId::mapGoToBlockCoordinate,
        "map.go-to-block-coordinate",
        "Go to Block...",
        "Map",
        "Center the map on a block coordinate",
        "",
        requiresMapDocument,
        [](WorkbenchState & state) { requestCoordinateDialog(state, CoordinateKind::block); });
    add(registry,
        CommandId::mapCopyCoordinate,
        "map.copy-coordinate",
        "Copy Coordinate",
        "Map",
        "Copy the active tile coordinate",
        "",
        requiresActiveLocation,
        [](WorkbenchState & state)
        {
            const MapContextSnapshot context = state.session.contextSnapshot(state.workspace);
            state.pendingClipboardText = std::to_string(context.activeTile->x) + " " + std::to_string(context.activeTile->y);
        });
    add(registry,
        CommandId::mapCopyTileData,
        "map.copy-tile-data",
        "Copy Tile Data",
        "Map",
        "Copy the active tile ID and elevation",
        "",
        requiresActiveLocation,
        [](WorkbenchState & state)
        {
            const MapContextSnapshot context = state.session.contextSnapshot(state.workspace);
            state.pendingClipboardText = "Tile ID: " + strings::formatId(context.activeValue->id, state.preferences.idFormat) +
                                         "\nElevation: " + std::to_string(static_cast<int>(context.activeValue->z));
        });
}

} // namespace uo_mule::gui
