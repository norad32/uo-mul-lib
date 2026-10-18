#pragma once

#include "gui/dialogs/data_paths_dialog.h"
#include "gui/map_workspace.h"
#include "gui/new_map_dialog.h"

namespace uo_mule::gui
{

struct MenuBarState
{
    NewMapDialogState fileDialog;
    DataPathsDialogState dataPathsDialog;
    bool windowCloseRequested = false;
};

[[nodiscard]] bool drawMenuBar(MapWorkspace & workspace,
                               DataProfile & activeProfile,
                               MenuBarState & state,
                               bool & shouldClose);
void requestWindowClose(MenuBarState & state);

} // namespace uo_mule::gui
