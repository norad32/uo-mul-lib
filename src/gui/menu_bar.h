#pragma once

#include "gui/map_workspace.h"
#include "gui/new_map_dialog.h"

namespace uo_mule::gui
{

struct MenuBarState
{
    NewMapDialogState fileDialog;
    bool windowCloseRequested = false;
};

void drawMenuBar(MapWorkspace & workspace, MenuBarState & state, bool & shouldClose);
void requestWindowClose(MenuBarState & state);

} // namespace uo_mule::gui
