#include "gui/menu_bar.h"
#include "gui/dialogs/data_paths_dialog.h"
#include "gui/command_registry.h"
#include "gui/map_workspace.h"
#include "gui/new_map_dialog.h"
#include "gui/workbench_state.h"
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
    const MapPanelVisibility & visibility = state.workspace.panelVisibility();
    drawCommandItem(state, CommandId::panelMapCanvas, visibility.canvas);
    drawCommandItem(state, CommandId::panelTileInspector, visibility.tileInspector);
    drawCommandItem(state, CommandId::panelBlockInspector, visibility.blockInspector);
    drawCommandItem(state, CommandId::panelSelectionStatistics, visibility.selectionStatistics);
    drawCommandItem(state, CommandId::panelMapInformation, visibility.mapInformation);
    drawCommandItem(state, CommandId::panelEditTools, visibility.editTools);
    drawCommandItem(state, CommandId::panelRadarcol, visibility.radarcol);
    drawCommandItem(state, CommandId::panelTexmaps, visibility.texmaps);
    drawCommandItem(state, CommandId::viewToggleLogPanel, state.preferences.showLogPanel);
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

void drawEditMenu(WorkbenchState & state)
{
    if (!ImGui::BeginMenu("Edit"))
    {
        return;
    }

    const bool editMode = state.workspace.editMode();
    const Command * command = state.commands.find(CommandId::editToggle);
    const CommandAvailability commandState = state.commands.availability(CommandId::editToggle, state);
    const std::string label = editMode ? "Edit mode (ON)" : "Edit mode (OFF)";
    if (command != nullptr && ImGui::MenuItem(label.c_str(), nullptr, editMode, commandState.enabled))
    {
        static_cast<void>(state.commands.execute(CommandId::editToggle, state));
    }

    ImGui::EndMenu();
}

} // namespace

bool drawMenuBar(WorkbenchState & state)
{
    bool profileChanged = false;

    if (state.menuBarState.windowCloseRequested && state.menuBarState.fileDialog.mode == MapFileDialogMode::none)
    {
        state.menuBarState.windowCloseRequested = false;
        static_cast<void>(state.commands.execute(CommandId::fileQuit, state));
    }

    if (ImGui::BeginMainMenuBar())
    {
        drawFileMenu(state);
        drawViewMenu(state);
        drawSelectionMenu(state);
        drawEditMenu(state);

        if (ImGui::BeginMenu("Data"))
        {
            drawCommandItem(state, CommandId::dataPaths);

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    if (state.menuBarState.fileDialog.shouldClose)
    {
        state.shouldClose = true;
        state.menuBarState.fileDialog.shouldClose = false;
    }

    drawNewMapDialog(state.workspace,
                     state.menuBarState.fileDialog,
                     state.operationController,
                     state.activeProfile.outputCollisionPolicy,
                     state.notifications,
                     state.shouldClose);
    profileChanged = drawDataPathsDialog(state.menuBarState.dataPathsDialog, state.activeProfile);
    return profileChanged;
}

void requestWindowClose(WorkbenchState & state)
{
    state.menuBarState.windowCloseRequested = true;
}

} // namespace uo_mule::gui
