#include "gui/views/world/tile_labels.h"
#include "gui_model/workspaces/map_workspace.h"
#include "uomul/map/map.h"
#include <string>

namespace uo_mule::gui
{

std::string tileLabel(MapDisplayMode mode, uomul::MapTile tile)
{
    if (mode == MapDisplayMode::tileIds)
    {
        return std::to_string(tile.id);
    }

    if (mode == MapDisplayMode::z)
    {
        return std::to_string(static_cast<int>(tile.z));
    }

    return std::to_string(tile.id) + "\n" + std::to_string(static_cast<int>(tile.z));
}

} // namespace uo_mule::gui
