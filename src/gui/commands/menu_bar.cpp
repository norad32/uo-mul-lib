#include "gui/commands/menu_bar.h"
#include "gui/commands/file_operations.h"
#include "gui/commands/command_registry.h"
#include "gui/dialogs/new_file_dialog.h"
#include "gui/documents/map_workspace.h"
#include "gui/application/workbench_state.h"
#include "gui/profile/workbench_session.h"
#include "gui/views/panel_registry.h"
#include <imgui.h>
#include <string>

namespace uo_mule::gui
{
namespace
{

void drawCommandItem(WorkbenchState & state, CommandId id, bool selected = false)
{
    const Command * command = state.commands.find(id);
    if (command == nullptr)
    {
        return;
    }
    const CommandAvailability commandState = state.commands.availability(id, state);
    const std::string shortcut = command->defaultShortcut;
    const bool activated = ImGui::MenuItem(command->label.c_str(),
                                           shortcut.empty() ? nullptr : shortcut.c_str(),
                                           selected,
                                           commandState.enabled);
    if (activated)
    {
        static_cast<void>(state.commands.execute(id, state));
    }
}

void drawFileMenu(WorkbenchState & state)
{
    if (!ImGui::BeginMenu("File"))
    {
        return;
    }

    drawCommandItem(state, CommandId::fileOpen);
    drawCommandItem(state, CommandId::fileNew);
    ImGui::Separator();
    drawCommandItem(state, CommandId::fileSave);
    drawCommandItem(state, CommandId::fileSaveAs);
    drawCommandItem(state, CommandId::fileDiscard);
    ImGui::Separator();
    drawCommandItem(state, CommandId::fileClose);
    drawCommandItem(state, CommandId::fileQuit);

    ImGui::EndMenu();
}

void drawViewMenu(WorkbenchState & state)
{
    if (!ImGui::BeginMenu("View"))
    {
        return;
    }

    const MapDisplayMode displayMode = state.workspace.displayMode();
    drawCommandItem(state, CommandId::viewTileIds, displayMode == MapDisplayMode::tileIds);
    drawCommandItem(state, CommandId::viewZ, displayMode == MapDisplayMode::z);
    drawCommandItem(state, CommandId::viewCombined, displayMode == MapDisplayMode::combined);

    ImGui::Separator();
    drawCommandItem(state, CommandId::panelTileInspector, state.panelRegistry.find(PanelId::tileInspector)->visible);
    drawCommandItem(state, CommandId::panelBlockInspector, state.panelRegistry.find(PanelId::blockInspector)->visible);
    drawCommandItem(state, CommandId::panelSelectionStatistics, state.panelRegistry.find(PanelId::selectionStatistics)->visible);
    drawCommandItem(state, CommandId::panelMapInformation, state.panelRegistry.find(PanelId::mapInformation)->visible);
    drawCommandItem(state, CommandId::panelEditTools, state.panelRegistry.find(PanelId::editTools)->visible);
    drawCommandItem(state, CommandId::panelRadarcol, state.panelRegistry.find(PanelId::radarcol)->visible);
    drawCommandItem(state, CommandId::panelTexmaps, state.panelRegistry.find(PanelId::texmaps)->visible);
    drawCommandItem(state, CommandId::viewToggleLogPanel, state.preferences.showLogPanel);
    drawCommandItem(state, CommandId::resetLayout);
    ImGui::EndMenu();
}

void drawSelectionMenu(WorkbenchState & state)
{
    if (!ImGui::BeginMenu("Selection"))
    {
        return;
    }

    const MapSelectionMode selectionMode = state.workspace.selectionMode();
    drawCommandItem(state, CommandId::selectionTiles, selectionMode == MapSelectionMode::tiles);
    drawCommandItem(state, CommandId::selectionBlocks, selectionMode == MapSelectionMode::blocks);

    ImGui::EndMenu();
}

void drawMapMenu(WorkbenchState & state)
{
    if (!ImGui::BeginMenu("Map"))
    {
        return;
    }
    drawCommandItem(state, CommandId::mapFit);
    drawCommandItem(state, CommandId::mapFitSelection);
    drawCommandItem(state, CommandId::mapCenterSelection);
    drawCommandItem(state, CommandId::mapResetView);
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
    const bool fileOperationHandled = handleFileOperationsEscape(state);
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
        static_cast<void>(state.commands.execute(CommandId::fileQuit, state));
    }

    if (showMainMenu && ImGui::BeginMainMenuBar())
    {
        drawFileMenu(state);
        drawViewMenu(state);
        drawSelectionMenu(state);
        drawMapMenu(state);

        if (ImGui::BeginMenu("Data"))
        {
            drawCommandItem(state, CommandId::dataPaths);

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    handleEscape(state, showMainMenu);

    if (!drawDialogs)
    {
        return false;
    }

    return drawFileOperations(state);
}

void requestWindowClose(WorkbenchState & state)
{
    state.menuBarState.windowCloseRequested = true;
}

} // namespace uo_mule::gui
