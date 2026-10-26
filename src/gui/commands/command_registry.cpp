#include "gui/commands/command_registry.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/dialogs/map_workflow_dialog.h"
#include "gui/dialogs/preferences_dialog.h"
#include "gui/documents/map_document_window.h"
#include "gui/documents/radarcol_document_window.h"
#include "gui/presentation/strings.h"
#include "gui/workbench/tool_context.h"
#include "gui/workbench/workbench_state.h"
#include "gui_model/dialogs/dialog_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/documents/map_document.h"
#include "gui_model/documents/map_document_state.h"
#include "gui_model/documents/map_view_state.h"
#include "gui_model/documents/radarcol_document.h"
#include "gui_model/documents/texmaps_document.h"
#include "gui_model/input/coordinate_parser.h"
#include "gui_model/input/radarcol_color_input.h"
#include "gui_model/layout/panel_registry.h"
#include "gui_model/layout/window_registry.h"
#include "gui_model/lifecycle/document_lifecycle.h"
#include "gui_model/model/assets/asset_window_contracts.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/session/workbench_session.h"
#include "gui_model/tools/tool_contracts.h"
#include "gui_model/tools/tool_state.h"
#include "gui_model/world/map_canvas_state.h"
#include "gui_model/world/map_context.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <uomul/radarcol/radarcol.h>
#include <utility>
#include <vector>

namespace uo_mule::gui
{
namespace
{

[[nodiscard]] TexmapsDocumentIntent texmapsLifecycleIntent(TexmapsDocumentIntentKind kind)
{
    TexmapsDocumentIntent intent;
    intent.kind = kind;
    return intent;
}

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
    if (!state.mapDocuments.active().changes().reader.has_value())
    {
        return {.enabled = false, .disabledReason = "No map is open"};
    }
    return enabled(state);
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
    const MapContextSnapshot context = state.session.contextSnapshot(state.mapDocuments);
    if (!context.activeTile || !context.activeValue)
    {
        return {.enabled = false, .disabledReason = "No map location is active"};
    }
    return enabled(state);
}

CommandAvailability requiresMapForSave(const WorkbenchState & state)
{
    if (!state.mapDocuments.active().changes().reader.has_value() || !state.mapDocuments.active().changes().dirty())
    {
        return {
            .enabled = false,
            .disabledReason =
                !state.mapDocuments.active().changes().reader.has_value() ? "No map is open" : "There are no staged changes",
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
        if (!active->radarcol->editing() || !active->radarcol->dirty())
        {
            return {.enabled = false, .disabledReason = "There are no staged changes"};
        }
        return enabled(state);
    }
    if (active->texmaps)
    {
        if (!active->texmaps->editing() || !active->texmaps->dirty())
        {
            return {.enabled = false, .disabledReason = "There are no staged changes"};
        }
        return enabled(state);
    }
    return {.enabled = false, .disabledReason = "This document cannot be edited"};
}

CommandAvailability requiresRadarcolColorEditor(const WorkbenchState & state)
{
    const auto * active = state.session.activeDocumentRecord();
    if (active == nullptr || !active->radarcol)
    {
        return {.enabled = false, .disabledReason = "No standalone Radarcol document is active"};
    }
    const auto view = state.radarcolState.documentAssetViews.find(active->radarcol->id());
    if (view == state.radarcolState.documentAssetViews.end() || view->second.selection.selected.empty())
    {
        return {.enabled = false, .disabledReason = "Select one or more Radarcol colors"};
    }
    return enabled(state);
}

[[nodiscard]] const assets::AssetSelection * activeRadarcolSelection(const WorkbenchState & state)
{
    const auto * active = state.session.activeDocumentRecord();
    if (active != nullptr && active->radarcol)
    {
        const auto view = state.radarcolState.documentAssetViews.find(active->radarcol->id());
        return view == state.radarcolState.documentAssetViews.end() ? nullptr : &view->second.selection;
    }
    if (active != nullptr && active->texmaps)
    {
        const auto view = state.texmapsState.documentAssetViews.find(active->texmaps->id());
        return view == state.texmapsState.documentAssetViews.end() ? nullptr : &view->second.selection;
    }
    return &state.radarcolState.assetView.selection;
}

CommandAvailability requiresAssetSelection(const WorkbenchState & state)
{
    const auto * selection = activeRadarcolSelection(state);
    if (selection == nullptr || selection->selected.empty())
    {
        return {.enabled = false, .disabledReason = "Select one or more asset items"};
    }
    return enabled(state);
}

[[nodiscard]] std::optional<std::pair<std::optional<DocumentId>, assets::AssetSelection>>
activeTexmapExportSelection(const WorkbenchState & state)
{
    if (const auto * active = state.session.activeDocumentRecord(); active != nullptr && active->texmaps)
    {
        const auto view = state.texmapsState.documentAssetViews.find(active->texmaps->id());
        if (view != state.texmapsState.documentAssetViews.end())
        {
            return std::pair{std::optional<DocumentId>{active->texmaps->id()}, view->second.selection};
        }
    }
    return std::pair{std::optional<DocumentId>{}, state.texmapsState.assetView.selection};
}

CommandAvailability requiresTexmapExportSelection(const WorkbenchState & state)
{
    const auto selection = activeTexmapExportSelection(state);
    if (!selection || selection->second.selected.empty() || !state.texmapsState.exportFormatRequested)
    {
        return {.enabled = false, .disabledReason = "Select one or more Texmap textures"};
    }
    return enabled(state);
}

void requestTexmapFormatExport(const WorkbenchState & state)
{
    const auto selection = activeTexmapExportSelection(state);
    if (!selection || !state.texmapsState.exportFormatRequested)
    {
        return;
    }
    std::vector<std::uint32_t> textureIds;
    textureIds.reserve(selection->second.selected.size());
    for (const auto & item : selection->second.selected)
    {
        if (const auto id = assets::itemIndex(item))
        {
            textureIds.push_back(*id);
        }
    }
    state.texmapsState.exportFormatRequested(selection->first, std::move(textureIds));
}

template <typename State>
[[nodiscard]] auto hoveredAssetWindow(State & state)
    -> std::conditional_t<std::is_const_v<State>, const assets::AssetWindowState *, assets::AssetWindowState *>
{
    const std::string_view context =
        !state.activeAssetPanelContext.empty() ? state.activeAssetPanelContext : state.hoveredPanelContext;
    if (context == "Radarcol" && state.windowRegistry.find(WindowId::radarcol)->visible)
    {
        if (const auto * active = state.session.activeDocumentRecord(); active != nullptr && active->radarcol)
        {
            const auto view = state.radarcolState.documentAssetViews.find(active->radarcol->id());
            return view == state.radarcolState.documentAssetViews.end() ? nullptr : &view->second;
        }
        return &state.radarcolState.assetView;
    }
    if (context == "Texmaps" && state.windowRegistry.find(WindowId::texmaps)->visible)
    {
        if (const auto * active = state.session.activeDocumentRecord(); active != nullptr && active->texmaps)
        {
            const auto view = state.texmapsState.documentAssetViews.find(active->texmaps->id());
            return view == state.texmapsState.documentAssetViews.end() ? nullptr : &view->second;
        }
        return &state.texmapsState.assetView;
    }
    if (context == "Tiledata" && state.windowRegistry.find(WindowId::tiledata)->visible)
    {
        if (const auto * active = state.session.activeDocumentRecord(); active != nullptr && active->tiledata)
        {
            const auto view = state.tiledataState.documentAssetViews.find(active->tiledata->id());
            return view == state.tiledataState.documentAssetViews.end() ? nullptr : &view->second;
        }
        return &state.tiledataState.assetView;
    }
    return nullptr;
}

CommandAvailability requiresSelectionCommand(const WorkbenchState & state)
{
    const auto * asset = hoveredAssetWindow(state);
    return asset != nullptr && asset->selectAll ? enabled(state) : requiresMapDocument(state);
}

void executeSelectionAll(WorkbenchState & state)
{
    if (const auto * asset = hoveredAssetWindow(state); asset != nullptr && asset->selectAll)
    {
        asset->selectAll();
        return;
    }
    state.mapDocuments.active().viewState().selectAll(state.mapDocuments.active().changes().dimensions);
}

void executeSelectionNone(WorkbenchState & state)
{
    if (const auto * asset = hoveredAssetWindow(state); asset != nullptr && asset->selectNone)
    {
        asset->selectNone();
        return;
    }
    state.mapDocuments.active().viewState().clearSelection();
}

void executeSelectionInvert(WorkbenchState & state)
{
    if (const auto * asset = hoveredAssetWindow(state); asset != nullptr && asset->invertSelection)
    {
        asset->invertSelection();
        return;
    }
    state.mapDocuments.active().viewState().invertSelection(state.mapDocuments.active().changes().dimensions);
}

void copyActiveRadarcolSelection(WorkbenchState & state)
{
    const auto * selection = activeRadarcolSelection(state);
    if (selection == nullptr || selection->selected.empty())
    {
        return;
    }

    std::string csv = "ID,RGBHex,Raw\n";
    const auto appendColor = [&csv](std::uint32_t index, std::optional<uomul::RadarColor> color)
    {
        if (color)
        {
            csv += std::to_string(index) + "," + formatRadarcolRgbHex(*color) + "," + formatRadarcolRaw(*color) + "\n";
        }
    };
    const auto * active = state.session.activeDocumentRecord();
    if (active != nullptr && active->radarcol)
    {
        const auto kind = state.radarcolState.documentBrowserKinds[active->radarcol->id()];
        for (const auto & item : selection->selected)
        {
            if (const auto index = assets::itemIndex(item))
            {
                appendColor(*index,
                            active->radarcol->rawColor(
                                kind == RadarcolBrowserKind::statics ? uomul::radarStaticColorOffset + *index : *index));
            }
        }
    }
    else
    {
        for (const auto & item : selection->selected)
        {
            if (const auto index = assets::itemIndex(item))
            {
                appendColor(
                    *index,
                    state.providers.radarColors().rawColor(state.radarcolState.browserKind == RadarcolBrowserKind::statics
                                                               ? uomul::radarStaticColorOffset + *index
                                                               : *index));
            }
        }
    }
    state.pendingClipboardText = std::move(csv);
}

void openDialog(WorkbenchState & state, MapFileDialogMode mode)
{
    requestMapDialog(state.menuBarState.fileDialog, mode);
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

void requestClose(WorkbenchState & state, bool quit)
{
    state.quitRequested = quit;
    const auto activeId = state.session.activeDocumentId();
    if (!activeId)
    {
        if (quit)
        {
            state.quitRequested = false;
            state.shouldClose = true;
        }
        return;
    }
    state.documentLifecycle.requestClose({.id = *activeId, .dirtyPolicy = DocumentCloseDirtyPolicy::prompt});
}

void togglePanel(bool & visibility)
{
    visibility = !visibility;
}

void requestCoordinateDialog(WorkbenchState & state, CoordinateKind kind)
{
    state.coordinateDialog.inputKind = kind;
    state.coordinateDialog.inputFocusRequested = true;
    state.coordinateDialog.input.fill('\0');
    state.coordinateDialog.requested = true;
}

bool & panelVisibility(WorkbenchState & state, PanelId id)
{
    return state.panelRegistry.find(id)->visible;
}

bool & windowVisibility(WorkbenchState & state, WindowId id)
{
    return state.windowRegistry.find(id)->visible;
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
        case ToolId::move :
            return CommandId::toolMove;
        case ToolId::rectangleSelect :
            return CommandId::toolRectangleSelect;
        case ToolId::lassoSelect :
            return CommandId::toolLassoSelect;
        case ToolId::fuzzySelect :
            return CommandId::toolFuzzySelect;
        case ToolId::fill :
            return CommandId::toolFill;
        case ToolId::replace :
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

namespace
{
void registerToolCommands(CommandRegistry & registry)
{
    add(
        registry,
        CommandId::toolMove,
        "tool.move",
        "Pan",
        "Tools",
        "Pan the world view",
        "P",
        [](const WorkbenchState & state) { return requiresTool(ToolId::move, state); },
        [](WorkbenchState & state) { activateTool(state, ToolId::move); });
    add(
        registry,
        CommandId::toolRectangleSelect,
        "tool.rectangle-select",
        "Rectangle Select",
        "Tools",
        "Select a rectangular tile or block area",
        "R",
        [](const WorkbenchState & state) { return requiresTool(ToolId::rectangleSelect, state); },
        [](WorkbenchState & state) { activateTool(state, ToolId::rectangleSelect); });
    add(
        registry,
        CommandId::toolLassoSelect,
        "tool.lasso-select",
        "Free Select",
        "Tools",
        "Select a freehand tile or block area",
        "F",
        [](const WorkbenchState & state) { return requiresTool(ToolId::lassoSelect, state); },
        [](WorkbenchState & state) { activateTool(state, ToolId::lassoSelect); });
    add(
        registry,
        CommandId::toolFuzzySelect,
        "tool.fuzzy-select",
        "Fuzzy Select",
        "Tools",
        "Select matching connected or global map tiles",
        "U",
        [](const WorkbenchState & state) { return requiresTool(ToolId::fuzzySelect, state); },
        [](WorkbenchState & state) { activateTool(state, ToolId::fuzzySelect); });
    add(
        registry,
        CommandId::toolFill,
        "tool.fill",
        "Fill",
        "Tools",
        "Fill the current selection from the active tile",
        "Shift+B",
        [](const WorkbenchState & state) { return requiresTool(ToolId::fill, state); },
        [](WorkbenchState & state) { activateTool(state, ToolId::fill); });
    add(
        registry,
        CommandId::toolReplace,
        "tool.replace",
        "Replace",
        "Tools",
        "Replace matching IDs or elevations in the selection",
        "Shift+R",
        [](const WorkbenchState & state) { return requiresTool(ToolId::replace, state); },
        [](WorkbenchState & state) { activateTool(state, ToolId::replace); });
}

void registerFileOpenCommands(CommandRegistry & registry)
{
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
                const auto saved = state.mapDocuments.save(state.mapDocuments.active().changes().originalPath, false);
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
                    .colorEdits = {},
                    .outputPath = {},
                    .overwriteOutput = false,
                });
                return;
            }
            if (active->texmaps)
            {
                active->texmaps->requestIntent(texmapsLifecycleIntent(TexmapsDocumentIntentKind::save));
            }
        });
}

void registerAssetCommands(CommandRegistry & registry)
{
    add(registry,
        CommandId::assetEdit,
        "asset.edit",
        "Edit...",
        "Edit",
        "Edit the selected asset items",
        "Ctrl+E",
        requiresRadarcolColorEditor,
        [](WorkbenchState & state)
        {
            auto * active = state.session.activeDocumentRecord();
            if (active == nullptr || !active->radarcol)
            {
                return;
            }
            const auto view = state.radarcolState.documentAssetViews.find(active->radarcol->id());
            if (view != state.radarcolState.documentAssetViews.end())
            {
                openRadarcolColorPicker(state.radarcolState, *active->radarcol, view->second.selection);
            }
        });
    add(registry,
        CommandId::assetCopy,
        "asset.copy",
        "Copy",
        "Edit",
        "Copy the selected asset items",
        "Ctrl+C",
        requiresAssetSelection,
        [](WorkbenchState & state) { copyActiveRadarcolSelection(state); });
}

void registerFileSaveCommands(CommandRegistry & registry)
{
    add(registry,
        CommandId::fileSaveAs,
        "file.save-as",
        "Save As...",
        "File",
        "Save staged document changes to a new file",
        "Ctrl+Shift+S",
        requiresSaveableDocument,
        [](WorkbenchState & state)
        {
            if (const auto * active = state.session.activeDocumentRecord(); active != nullptr && active->radarcol)
            {
                requestFileBrowser(state.fileBrowser,
                                   {
                                       .mode = FileBrowserMode::saveFile,
                                       .target = FileBrowserTarget::radarcolOutput,
                                       .initialPath = active->radarcol->sourcePath(),
                                       .extension = ".mul",
                                       .title = "Save Radarcol As",
                                   });
                return;
            }
            if (state.mapDocuments.active().changes().dirty())
            {
                state.menuBarState.fileDialog.savePath.fill('\0');
                if (const auto path = state.mapDocuments.active().changes().originalPath; !path.empty())
                {
                    const std::string value = path.string();
                    const std::size_t length = std::min(value.size(), state.menuBarState.fileDialog.savePath.size() - 1U);
                    std::copy_n(value.data(), length, state.menuBarState.fileDialog.savePath.data());
                    state.menuBarState.fileDialog.savePath.at(length) = '\0';
                }
                openDialog(state, MapFileDialogMode::saveAs);
                requestFileBrowser(state.fileBrowser,
                                   {
                                       .mode = FileBrowserMode::saveFile,
                                       .target = FileBrowserTarget::mapSaveAs,
                                       .initialPath = state.mapDocuments.active().changes().originalPath,
                                       .extension = ".mul",
                                       .title = "Save Map As",
                                   });
            }
            else
            {
                state.menuBarState.fileDialog.error = "There are no staged changes to save";
                openDialog(state, MapFileDialogMode::error);
            }
        });
    add(registry,
        CommandId::fileExportAs,
        "file.export-as",
        "Export As...",
        "File",
        "Export selected Texmap textures as BMP or PNG",
        "",
        requiresTexmapExportSelection,
        requestTexmapFormatExport);
    add(registry,
        CommandId::fileDiscard,
        "file.discard",
        "Discard",
        "File",
        "Discard staged document changes",
        "",
        requiresSaveableDocument,
        [](WorkbenchState & state)
        {
            if (auto * active = state.session.activeDocumentRecord(); active != nullptr && active->radarcol)
            {
                active->radarcol->requestIntent({
                    .kind = RadarcolDocumentIntentKind::discard,
                    .colorEdits = {},
                    .outputPath = {},
                    .overwriteOutput = false,
                });
                return;
            }
            if (auto * active = state.session.activeDocumentRecord(); active != nullptr && active->texmaps)
            {
                active->texmaps->requestIntent(texmapsLifecycleIntent(TexmapsDocumentIntentKind::discard));
                return;
            }
            const auto discarded = state.mapDocuments.discard();
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
        "Close the current document",
        "",
        enabled,
        [](WorkbenchState & state) { requestClose(state, false); });
}

void registerFileCloseCommands(CommandRegistry & registry)
{
    add(registry,
        CommandId::fileQuit,
        "file.quit",
        "Quit",
        "File",
        "Close documents and quit",
        "Ctrl+Q",
        enabled,
        [](WorkbenchState & state) { requestClose(state, true); });
}

void registerFileCommands(CommandRegistry & registry)
{
    registerFileOpenCommands(registry);
    registerAssetCommands(registry);
    registerFileSaveCommands(registry);
    registerFileCloseCommands(registry);
}

void registerViewCommands(CommandRegistry & registry)
{
    add(registry,
        CommandId::viewTileIds,
        "view.display.tile-ids",
        "Tile IDs",
        "View",
        "Display tile IDs",
        "",
        enabled,
        [](WorkbenchState & state) { state.mapDocuments.active().viewState().displayMode = MapDisplayMode::tileIds; });
    add(registry,
        CommandId::viewZ,
        "view.display.z",
        "Z",
        "View",
        "Display elevation",
        "",
        enabled,
        [](WorkbenchState & state) { state.mapDocuments.active().viewState().displayMode = MapDisplayMode::z; });
    add(registry,
        CommandId::viewCombined,
        "view.display.combined",
        "Combined",
        "View",
        "Display tile IDs and elevation",
        "",
        enabled,
        [](WorkbenchState & state) { state.mapDocuments.active().viewState().displayMode = MapDisplayMode::combined; });
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
}

void registerPanelCommands(CommandRegistry & registry)
{
    add(registry,
        CommandId::panelToolPalette,
        "view.panel.tool-palette",
        "Tool Palette",
        "Windows",
        "Show or hide the tool palette",
        "",
        enabled,
        [](WorkbenchState & state) { togglePanel(panelVisibility(state, PanelId::toolPalette)); });
    add(registry,
        CommandId::panelTileInspector,
        "view.panel.tile-inspector",
        "Tile Inspector",
        "Windows",
        "Show or hide the tile inspector",
        "",
        enabled,
        [](WorkbenchState & state) { togglePanel(panelVisibility(state, PanelId::tileInspector)); });
    add(registry,
        CommandId::panelBlockInspector,
        "view.panel.block-inspector",
        "Block Inspector",
        "Windows",
        "Show or hide the block inspector",
        "",
        enabled,
        [](WorkbenchState & state) { togglePanel(panelVisibility(state, PanelId::blockInspector)); });
    add(registry,
        CommandId::windowRadarcol,
        "view.window.radarcol",
        "Radarcol",
        "Windows",
        "Show or hide the configured Radarcol document",
        "",
        enabled,
        [](WorkbenchState & state) { togglePanel(windowVisibility(state, WindowId::radarcol)); });
    add(registry,
        CommandId::windowTexmaps,
        "view.window.texmaps",
        "Texmaps",
        "Windows",
        "Show or hide the configured Texmaps document",
        "",
        enabled,
        [](WorkbenchState & state) { togglePanel(windowVisibility(state, WindowId::texmaps)); });
    add(registry,
        CommandId::windowTiledata,
        "view.window.tiledata",
        "Tiledata",
        "Windows",
        "Show or hide the configured Tiledata document",
        "",
        enabled,
        [](WorkbenchState & state) { togglePanel(windowVisibility(state, WindowId::tiledata)); });
    add(registry,
        CommandId::panelIconGallery,
        "view.panel.icon-gallery",
        "Icon Gallery",
        "Windows",
        "Show or hide the semantic icon gallery",
        "",
        enabled,
        [](WorkbenchState & state) { togglePanel(panelVisibility(state, PanelId::iconGallery)); });
}

void registerSelectionCommands(CommandRegistry & registry)
{
    add(registry,
        CommandId::selectionTiles,
        "selection.tiles",
        "Tile selection",
        "Select",
        "Select individual tiles",
        "",
        enabled,
        [](WorkbenchState & state)
        {
            const auto result = state.toolState.setSelectionGranularity(
                ToolSelectionGranularity::tiles, toolContext(state), state.toolRegistry);
            if (result.accepted)
            {
                state.mapDocuments.active().viewState().setSelectionMode(MapSelectionMode::tiles,
                                                                         state.mapDocuments.active().changes().dimensions);
            }
        });
    add(registry,
        CommandId::selectionBlocks,
        "selection.blocks",
        "Block selection",
        "Select",
        "Select map blocks",
        "",
        enabled,
        [](WorkbenchState & state)
        {
            const auto result = state.toolState.setSelectionGranularity(
                ToolSelectionGranularity::blocks, toolContext(state), state.toolRegistry);
            if (result.accepted)
            {
                state.mapDocuments.active().viewState().setSelectionMode(MapSelectionMode::blocks,
                                                                         state.mapDocuments.active().changes().dimensions);
            }
        });
    add(registry,
        CommandId::selectionAll,
        "selection.all",
        "All",
        "Select",
        "Select all tiles or blocks at the active granularity",
        "Ctrl+A",
        requiresSelectionCommand,
        executeSelectionAll);
    add(registry,
        CommandId::selectionNone,
        "selection.none",
        "None",
        "Select",
        "Clear the current selection",
        "Ctrl+Shift+A",
        requiresSelectionCommand,
        executeSelectionNone);
    add(registry,
        CommandId::selectionInvert,
        "selection.invert",
        "Invert",
        "Select",
        "Invert the current selection",
        "Ctrl+I",
        requiresSelectionCommand,
        executeSelectionInvert);
    add(registry,
        CommandId::dataPaths,
        "data.paths",
        "Preferences...",
        "Edit",
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
            state.windowRegistry.resetToDefaults();
            state.layout.dockingIni.clear();
            state.layout.window.geometryValid = false;
            state.preferences.fullscreen = false;
            state.layoutResetRequested = true;
        });
}

void registerWorldCommands(CommandRegistry & registry)
{
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
        "World",
        "Fit the complete map in the canvas",
        "",
        requiresMapDocument,
        [](WorkbenchState & state)
        { state.mapDocuments.canvasState().navigationRequest.request = MapCanvasViewRequest::fitMap; });
    add(registry,
        CommandId::mapFitSelection,
        "map.fit-selection",
        "Fit Selection",
        "World",
        "Fit the current selection in the canvas",
        "",
        requiresMapDocument,
        [](WorkbenchState & state)
        { state.mapDocuments.canvasState().navigationRequest.request = MapCanvasViewRequest::fitSelection; });
    add(registry,
        CommandId::mapCenterSelection,
        "map.center-selection",
        "Center Selection",
        "World",
        "Center the current selection in the canvas",
        "",
        requiresMapDocument,
        [](WorkbenchState & state)
        { state.mapDocuments.canvasState().navigationRequest.request = MapCanvasViewRequest::centerSelection; });
    add(registry,
        CommandId::mapGoToCoordinate,
        "map.go-to-coordinate",
        "Go to Coordinate...",
        "World",
        "Center the map on a tile coordinate",
        "Ctrl+G",
        requiresMapDocument,
        [](WorkbenchState & state) { requestCoordinateDialog(state, CoordinateKind::tile); });
    add(registry,
        CommandId::mapGoToBlockCoordinate,
        "map.go-to-block-coordinate",
        "Go to Block...",
        "World",
        "Center the map on a block coordinate",
        "",
        requiresMapDocument,
        [](WorkbenchState & state) { requestCoordinateDialog(state, CoordinateKind::block); });
    add(registry,
        CommandId::mapCopyCoordinate,
        "map.copy-coordinate",
        "Copy Coordinate",
        "World",
        "Copy the active tile coordinate",
        "",
        requiresActiveLocation,
        [](WorkbenchState & state)
        {
            const MapContextSnapshot context = state.session.contextSnapshot(state.mapDocuments);
            state.pendingClipboardText = std::to_string(context.activeTile->x) + " " + std::to_string(context.activeTile->y);
        });
    add(registry,
        CommandId::mapCopyTileData,
        "map.copy-tile-data",
        "Copy Tile Data",
        "World",
        "Copy the active tile ID and elevation",
        "",
        requiresActiveLocation,
        [](WorkbenchState & state)
        {
            const MapContextSnapshot context = state.session.contextSnapshot(state.mapDocuments);
            state.pendingClipboardText = "Tile ID: " + strings::formatId(context.activeValue->id, state.preferences.idFormat) +
                                         "\nElevation: " + std::to_string(static_cast<int>(context.activeValue->z));
        });
}

} // namespace

void registerDefaultCommands(CommandRegistry & registry)
{
    registerToolCommands(registry);
    registerFileCommands(registry);
    registerViewCommands(registry);
    registerPanelCommands(registry);
    registerSelectionCommands(registry);
    registerWorldCommands(registry);
}

} // namespace uo_mule::gui
