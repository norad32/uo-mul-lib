#include "gui/commands/menu_bar.h"
#include "gui/commands/command_registry.h"
#include "gui/dialogs/map_workflow_dialog.h"
#include "gui/documents/radarcol_document_window.h"
#include "gui/presentation/iconography.h"
#include "gui/presentation/strings.h"
#include "gui/workbench/workbench_state.h"
#include "gui/workflows/file_workflow.h"
#include "gui_model/documents/map_document_state.h"
#include "gui_model/documents/map_view_state.h"
#include "gui_model/layout/panel_registry.h"
#include "gui_model/session/workbench_session.h"
#include "gui_model/tools/tool_contracts.h"
#include <imgui.h>
#include <optional>
#include <string>

namespace uo_mule::gui
{
namespace
{

[[nodiscard]] std::optional<IconId> commandIcon(CommandId id)
{
    switch (id)
    {
        case CommandId::fileOpen :
            return IconId::folder;
        case CommandId::fileSave :
        case CommandId::fileSaveAs :
            return IconId::save;
        case CommandId::fileExportAs :
            return IconId::exportIcon;
        case CommandId::fileDiscard :
        case CommandId::fileClose :
            return IconId::cancel;
        case CommandId::viewTileIds :
            return IconId::tileId;
        case CommandId::toolMove :
            return IconId::pan;
        case CommandId::toolRectangleSelect :
            return IconId::selection;
        case CommandId::toolLassoSelect :
            return IconId::lasso;
        case CommandId::toolFuzzySelect :
            return IconId::autoFix;
        case CommandId::toolFill :
            return IconId::formatColorFill;
        case CommandId::toolReplace :
            return IconId::replace;
        case CommandId::selectionTiles :
            return IconId::squareMedium;
        case CommandId::selectionBlocks :
            return IconId::square;
        case CommandId::selectionAll :
            return IconId::selectAll;
        case CommandId::selectionNone :
            return IconId::selectRemove;
        case CommandId::selectionInvert :
            return IconId::selectInverse;
        case CommandId::dataPaths :
            return IconId::preferences;
        case CommandId::mapFit :
        case CommandId::mapFitSelection :
            return IconId::fitView;
        default :
            return std::nullopt;
    }
}

[[nodiscard]] std::string commandLabel(const Command & command)
{
    const auto icon = commandIcon(command.id);
    if (!iconFontAvailable() || !icon.has_value())
    {
        return command.label;
    }
    return iconLabel(*icon, command.label);
}

void drawCommandItem(WorkbenchState & state, CommandId id, bool selected = false)
{
    const Command * command = state.commands.find(id);
    if (command == nullptr)
    {
        return;
    }
    const CommandAvailability commandState = state.commands.availability(id, state);
    const std::string shortcut = command->defaultShortcut;
    const std::string label = commandLabel(*command);
    const bool activated =
        ImGui::MenuItem(label.c_str(), shortcut.empty() ? nullptr : shortcut.c_str(), selected, commandState.enabled);
    if (activated)
    {
        state.deferWorkflowAction([&state, id] { static_cast<void>(state.commands.execute(id, state)); });
    }
}

void drawFileMenu(WorkbenchState & state)
{
    if (!ImGui::BeginMenu(strings::fileMenu()))
    {
        return;
    }

    drawCommandItem(state, CommandId::fileOpen);
    drawCommandItem(state, CommandId::fileNew);
    ImGui::Separator();
    drawCommandItem(state, CommandId::fileSave);
    drawCommandItem(state, CommandId::fileSaveAs);
    drawCommandItem(state, CommandId::fileExportAs);
    drawCommandItem(state, CommandId::fileDiscard);
    ImGui::Separator();
    drawCommandItem(state, CommandId::fileClose);
    drawCommandItem(state, CommandId::fileQuit);

    ImGui::EndMenu();
}

void drawViewMenu(WorkbenchState & state)
{
    if (!ImGui::BeginMenu(strings::viewMenu()))
    {
        return;
    }

    const MapDisplayMode displayMode = state.mapDocuments.active().viewState().displayMode;
    drawCommandItem(state, CommandId::viewTileIds, displayMode == MapDisplayMode::tileIds);
    drawCommandItem(state, CommandId::viewZ, displayMode == MapDisplayMode::z);
    drawCommandItem(state, CommandId::viewCombined, displayMode == MapDisplayMode::combined);
    drawCommandItem(state, CommandId::viewToggleFullscreen, state.preferences.fullscreen);

    ImGui::EndMenu();
}

void drawWindowsMenu(WorkbenchState & state)
{
    if (!ImGui::BeginMenu(strings::windowsMenu()))
    {
        return;
    }

    drawCommandItem(state, CommandId::panelToolPalette, state.panelRegistry.find(PanelId::toolPalette)->visible);
    drawCommandItem(state, CommandId::panelTileInspector, state.panelRegistry.find(PanelId::tileInspector)->visible);
    drawCommandItem(state, CommandId::panelBlockInspector, state.panelRegistry.find(PanelId::blockInspector)->visible);
    drawCommandItem(state, CommandId::windowRadarcol, state.windowRegistry.find(WindowId::radarcol)->visible);
    drawCommandItem(state, CommandId::windowTexmaps, state.windowRegistry.find(WindowId::texmaps)->visible);
    drawCommandItem(state, CommandId::windowTiledata, state.windowRegistry.find(WindowId::tiledata)->visible);
    drawCommandItem(state, CommandId::panelIconGallery, state.panelRegistry.find(PanelId::iconGallery)->visible);
    drawCommandItem(state, CommandId::viewToggleLogPanel, state.preferences.showLogPanel);
    drawCommandItem(state, CommandId::resetLayout);
    drawCommandItem(state, CommandId::resetAssetState);
    ImGui::EndMenu();
}

void drawEditMenu(WorkbenchState & state)
{
    if (!ImGui::BeginMenu(strings::editMenu()))
    {
        return;
    }
    ImGui::BeginDisabled();
    ImGui::MenuItem(strings::undo());
    ImGui::MenuItem(strings::redo());
    ImGui::Separator();
    ImGui::MenuItem(strings::cut());
    ImGui::MenuItem(strings::paste());
    ImGui::EndDisabled();
    drawCommandItem(state, CommandId::assetCopy);
    ImGui::Separator();
    drawCommandItem(state, CommandId::assetEdit);
    ImGui::Separator();
    drawCommandItem(state, CommandId::dataPaths);
    ImGui::EndMenu();
}

void drawSelectMenu(WorkbenchState & state)
{
    if (!ImGui::BeginMenu(strings::selectMenu()))
    {
        return;
    }

    const MapSelectionMode selectionMode = state.mapDocuments.active().viewState().selectionMode;
    drawCommandItem(state, CommandId::toolRectangleSelect, state.toolState.activeTool() == ToolId::rectangleSelect);
    drawCommandItem(state, CommandId::toolLassoSelect, state.toolState.activeTool() == ToolId::lassoSelect);
    drawCommandItem(state, CommandId::toolFuzzySelect, state.toolState.activeTool() == ToolId::fuzzySelect);
    ImGui::Separator();
    drawCommandItem(state, CommandId::selectionAll);
    drawCommandItem(state, CommandId::selectionNone);
    drawCommandItem(state, CommandId::selectionInvert);
    ImGui::Separator();
    drawCommandItem(state, CommandId::selectionTiles, selectionMode == MapSelectionMode::tiles);
    drawCommandItem(state, CommandId::selectionBlocks, selectionMode == MapSelectionMode::blocks);

    ImGui::EndMenu();
}

void drawToolsMenu(WorkbenchState & state)
{
    if (!ImGui::BeginMenu(strings::toolsMenu()))
    {
        return;
    }

    drawCommandItem(state, CommandId::toolMove, state.toolState.activeTool() == ToolId::move);
    ImGui::Separator();
    drawCommandItem(state, CommandId::toolRectangleSelect, state.toolState.activeTool() == ToolId::rectangleSelect);
    drawCommandItem(state, CommandId::toolLassoSelect, state.toolState.activeTool() == ToolId::lassoSelect);
    drawCommandItem(state, CommandId::toolFuzzySelect, state.toolState.activeTool() == ToolId::fuzzySelect);
    ImGui::Separator();
    drawCommandItem(state, CommandId::toolFill, state.toolState.activeTool() == ToolId::fill);
    drawCommandItem(state, CommandId::toolReplace, state.toolState.activeTool() == ToolId::replace);

    ImGui::EndMenu();
}

void drawWorldMenu(WorkbenchState & state)
{
    if (!ImGui::BeginMenu(strings::worldMenu()))
    {
        return;
    }
    drawCommandItem(state, CommandId::mapFit);
    drawCommandItem(state, CommandId::mapFitSelection);
    drawCommandItem(state, CommandId::mapCenterSelection);
    drawCommandItem(state, CommandId::mapGoToCoordinate);
    drawCommandItem(state, CommandId::mapGoToBlockCoordinate);
    drawCommandItem(state, CommandId::mapCopyCoordinate);
    drawCommandItem(state, CommandId::mapCopyTileData);
    ImGui::EndMenu();
}

void handleEscape(WorkbenchState & state, bool showMainMenu)
{
    if (!ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        return;
    }
    if (state.radarcolState.colorPicker.sourceMode)
    {
        cancelRadarcolColorSource(state.radarcolState);
        return;
    }
    if (state.radarcolState.colorPicker.open)
    {
        state.radarcolState.colorPicker.open = false;
        return;
    }
    const bool fileOperationHandled = handleFileWorkflowEscape(state);
    if (!fileOperationHandled && !showMainMenu && state.session.startupState() == StartupState::mapWelcome)
    {
        state.session.setStartupState(StartupState::workbench);
    }
}

} // namespace

bool drawMenuBar(WorkbenchState & state, bool showMainMenu, bool drawDialogs)
{
    if (state.menuBarState.windowCloseRequested && state.menuBarState.fileDialog.mode == MapFileDialogMode::none)
    {
        state.menuBarState.windowCloseRequested = false;
        state.deferWorkflowAction(
            [&state] { static_cast<void>(state.commands.execute(CommandId::fileQuit, state)); });
    }

    if (showMainMenu && ImGui::BeginMainMenuBar())
    {
        drawFileMenu(state);
        drawEditMenu(state);
        drawSelectMenu(state);
        drawWorldMenu(state);
        drawToolsMenu(state);
        drawViewMenu(state);
        drawWindowsMenu(state);
        if (ImGui::BeginMenu(strings::helpMenu()))
        {
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    handleEscape(state, showMainMenu);

    if (!drawDialogs)
    {
        return false;
    }

    return drawFileWorkflow(state);
}

void requestWindowClose(WorkbenchState & state)
{
    state.menuBarState.windowCloseRequested = true;
}

} // namespace uo_mule::gui
