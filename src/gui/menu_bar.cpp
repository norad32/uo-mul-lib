#include "gui/menu_bar.h"
#include "gui/dialogs/data_paths_dialog.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/data_profile.h"
#include "gui/panels/radarcol_panel.h"
#include "gui/panels/texmaps_panel.h"
#include "gui/command_registry.h"
#include "gui/map_workspace.h"
#include "gui/new_map_dialog.h"
#include "gui/workbench_state.h"
#include "gui/workbench_session.h"
#include "gui/panel_registry.h"
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

void cancelFileBrowserAndMapDialog(WorkbenchState & state)
{
    cancelFileBrowser(state.fileBrowser);
    if (state.menuBarState.fileDialog.mode != MapFileDialogMode::none)
    {
        cancelMapDialog(state.menuBarState.fileDialog);
    }
}

[[nodiscard]] bool applyFileBrowserResultToWorkbench(WorkbenchState & state, const FileBrowserResult & result)
{
    const bool profileChanged = result.target == FileBrowserTarget::radarcolSource ||
                                result.target == FileBrowserTarget::texmapsIndex ||
                                result.target == FileBrowserTarget::texmapsData;
    if (result.target == FileBrowserTarget::radarcolSource)
    {
        state.activeProfile.files[DataFileRole::radarcol] = {.path = result.path, .userOverride = true};
    }
    else if (result.target == FileBrowserTarget::texmapsIndex)
    {
        state.activeProfile.files[DataFileRole::texidx] = {.path = result.path, .userOverride = true};
    }
    else if (result.target == FileBrowserTarget::texmapsData)
    {
        state.activeProfile.files[DataFileRole::texmaps] = {.path = result.path, .userOverride = true};
    }

    applyFileBrowserResult(state.menuBarState.fileDialog, result);
    applyFileBrowserResult(state.menuBarState.dataPathsDialog, result);
    applyFileBrowserResult(state.radarcolState, result);
    applyFileBrowserResult(state.texmapsState, result);
    return profileChanged;
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
    drawCommandItem(state, CommandId::panelMapCanvas, state.panelRegistry.find(PanelId::mapCanvas)->visible);
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

} // namespace

bool drawMenuBar(WorkbenchState & state, bool showMainMenu, bool drawDialogs)
{
    bool profileChanged = false;

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
        drawEditMenu(state);
        drawMapMenu(state);

        if (ImGui::BeginMenu("Data"))
        {
            drawCommandItem(state, CommandId::dataPaths);

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        if (state.fileBrowser.visible)
        {
            cancelFileBrowserAndMapDialog(state);
        }
        else if (state.menuBarState.fileDialog.mode != MapFileDialogMode::none)
        {
            cancelMapDialog(state.menuBarState.fileDialog);
        }
        else if (state.menuBarState.dataPathsDialog.visible)
        {
            state.menuBarState.dataPathsDialog.visible = false;
        }
        else if (!showMainMenu && state.session.startupState() == StartupState::mapWelcome)
        {
            state.session.setStartupState(StartupState::workbench);
        }
    }

    if (!drawDialogs)
    {
        return profileChanged;
    }

    if (state.menuBarState.fileDialog.shouldClose)
    {
        state.shouldClose = true;
        state.menuBarState.fileDialog.shouldClose = false;
    }

    const bool browserWasVisible = state.fileBrowser.visible;
    if (!browserWasVisible)
    {
        drawNewMapDialog(state.workspace,
                         state.menuBarState.fileDialog,
                         state.operationController,
                         state.activeProfile.outputCollisionPolicy,
                         state.notifications,
                         state.fileBrowser,
                         state.shouldClose);
        profileChanged = drawDataPathsDialog(state.menuBarState.dataPathsDialog, state.activeProfile, state.fileBrowser);
    }
    drawFileBrowser(state.fileBrowser);
    const auto result = consumeFileBrowserResult(state.fileBrowser);
    if (!state.fileBrowser.visible && browserWasVisible && !result &&
        state.menuBarState.fileDialog.mode != MapFileDialogMode::none)
    {
        cancelMapDialog(state.menuBarState.fileDialog);
    }
    if (result)
    {
        profileChanged = applyFileBrowserResultToWorkbench(state, *result) || profileChanged;
    }
    return profileChanged;
}

void requestWindowClose(WorkbenchState & state)
{
    state.menuBarState.windowCloseRequested = true;
}

} // namespace uo_mule::gui
