#include "gui/menu_bar.h"
#include "gui/map_workspace.h"
#include "gui/new_map_dialog.h"
#include <imgui.h>

namespace im_cpp::gui
{
namespace
{

void openDialog(MenuBarState & state, MapFileDialogMode mode)
{
    state.fileDialog.mode = mode;
    state.fileDialog.activePopup = MapFileDialogMode::none;
    state.fileDialog.error.clear();
}

void requestClose(MapWorkspace & workspace, MenuBarState & state, PendingMapOperationKind kind)
{
    state.fileDialog.operation = PendingMapOperation{};
    state.fileDialog.operation.kind = kind;
    if (workspace.dirty())
    {
        openDialog(state, MapFileDialogMode::dirtyChanges);
        return;
    }
    const auto closed = workspace.closeMap();
    if (!closed)
    {
        openDialog(state, MapFileDialogMode::error);
        state.fileDialog.error = closed.error().message;
        return;
    }
    state.fileDialog.shouldClose = kind == PendingMapOperationKind::quit;
    state.fileDialog.operation = {};
}

void drawFileMenu(MapWorkspace & workspace, MenuBarState & state, bool & shouldClose)
{
    if (!ImGui::BeginMenu("File"))
    {
        return;
    }
    if (ImGui::MenuItem("Open..."))
    {
        openDialog(state, MapFileDialogMode::openMap);
    }
    if (ImGui::MenuItem("New Empty Map..."))
    {
        openDialog(state, MapFileDialogMode::newMap);
    }
    ImGui::Separator();
    const bool hasMap = workspace.reader() != nullptr;
    if (ImGui::MenuItem("Save As / Commit...", nullptr, false, hasMap))
    {
        if (workspace.dirty())
        {
            openDialog(state, MapFileDialogMode::saveAs);
        }
        else
        {
            state.fileDialog.error = "There are no staged changes to save";
            openDialog(state, MapFileDialogMode::error);
        }
    }
    if (ImGui::MenuItem("Discard", nullptr, false, workspace.dirty()))
    {
        const auto discarded = workspace.discard();
        if (!discarded)
        {
            openDialog(state, MapFileDialogMode::error);
            state.fileDialog.error = discarded.error().path.empty()
                                         ? discarded.error().message
                                         : discarded.error().path.string() + ": " + discarded.error().message;
        }
        else
        {
            state.fileDialog.error.clear();
        }
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Close"))
    {
        requestClose(workspace, state, PendingMapOperationKind::close);
    }
    if (ImGui::MenuItem("Quit"))
    {
        requestClose(workspace, state, PendingMapOperationKind::quit);
    }
    ImGui::EndMenu();
    if (state.fileDialog.shouldClose)
    {
        shouldClose = true;
        state.fileDialog.shouldClose = false;
    }
}

void drawViewMenu(MapWorkspace & workspace)
{
    if (!ImGui::BeginMenu("View"))
    {
        return;
    }
    const MapDisplayMode displayMode = workspace.displayMode();
    if (ImGui::MenuItem("Tile IDs", nullptr, displayMode == MapDisplayMode::tileIds))
    {
        workspace.setDisplayMode(MapDisplayMode::tileIds);
    }
    if (ImGui::MenuItem("Z", nullptr, displayMode == MapDisplayMode::z))
    {
        workspace.setDisplayMode(MapDisplayMode::z);
    }
    if (ImGui::MenuItem("Combined", nullptr, displayMode == MapDisplayMode::combined))
    {
        workspace.setDisplayMode(MapDisplayMode::combined);
    }
    ImGui::Separator();
    MapPanelVisibility & visibility = workspace.panelVisibility();
    ImGui::MenuItem("Map Canvas", nullptr, &visibility.canvas);
    ImGui::MenuItem("Tile Inspector", nullptr, &visibility.tileInspector);
    ImGui::MenuItem("Block Inspector", nullptr, &visibility.blockInspector);
    ImGui::MenuItem("Selection Statistics", nullptr, &visibility.selectionStatistics);
    ImGui::MenuItem("Map Information", nullptr, &visibility.mapInformation);
    ImGui::MenuItem("Edit Tools", nullptr, &visibility.editTools);
    ImGui::EndMenu();
}

void drawSelectionMenu(MapWorkspace & workspace)
{
    if (!ImGui::BeginMenu("Selection"))
    {
        return;
    }
    const MapSelectionMode selectionMode = workspace.selectionMode();
    if (ImGui::MenuItem("Tile selection", nullptr, selectionMode == MapSelectionMode::tiles))
    {
        workspace.setSelectionMode(MapSelectionMode::tiles);
    }
    if (ImGui::MenuItem("Block selection", nullptr, selectionMode == MapSelectionMode::blocks))
    {
        workspace.setSelectionMode(MapSelectionMode::blocks);
    }
    ImGui::EndMenu();
}

void drawEditMenu(MapWorkspace & workspace)
{
    if (!ImGui::BeginMenu("Edit"))
    {
        return;
    }
    bool editMode = workspace.editMode();
    const char * label = editMode ? "Edit mode (ON)" : "Edit mode (OFF)";
    if (ImGui::MenuItem(label, nullptr, editMode, workspace.reader() != nullptr))
    {
        editMode = !editMode;
        workspace.setEditMode(editMode);
    }
    ImGui::EndMenu();
}

} // namespace

void drawMenuBar(MapWorkspace & workspace, MenuBarState & state, bool & shouldClose)
{
    if (state.windowCloseRequested && state.fileDialog.mode == MapFileDialogMode::none)
    {
        state.windowCloseRequested = false;
        requestClose(workspace, state, PendingMapOperationKind::quit);
    }
    if (ImGui::BeginMainMenuBar())
    {
        drawFileMenu(workspace, state, shouldClose);
        drawViewMenu(workspace);
        drawSelectionMenu(workspace);
        drawEditMenu(workspace);
        ImGui::EndMainMenuBar();
    }
    if (state.fileDialog.shouldClose)
    {
        shouldClose = true;
        state.fileDialog.shouldClose = false;
    }
    drawNewMapDialog(workspace, state.fileDialog, shouldClose);
}

void requestWindowClose(MenuBarState & state)
{
    state.windowCloseRequested = true;
}

} // namespace im_cpp::gui
