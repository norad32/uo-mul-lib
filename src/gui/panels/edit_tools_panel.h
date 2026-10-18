#pragma once

#include "gui/map_workspace.h"
#include <cstdint>
#include <string>

namespace uo_mule::gui
{

enum class EditToolsOperation : std::uint8_t
{
    none,
    fill,
    replaceTileId,
    replaceZ,
};

struct EditToolsPanelState
{
    MapEditScope scope = MapEditScope::selection;
    int fillId = 0;
    int fillZ = 0;
    int replaceIdFrom = 0;
    int replaceIdTo = 0;
    int replaceZFrom = 0;
    int replaceZTo = 0;
    EditToolsOperation confirmationOperation = EditToolsOperation::none;
    std::string status;
    bool statusIsError = false;
};

void drawEditToolsPanel(MapWorkspace & workspace, EditToolsPanelState & state, bool & visible);

} // namespace uo_mule::gui
