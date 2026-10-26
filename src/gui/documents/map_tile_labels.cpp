#include "gui/documents/map_tile_labels.h"
#include "gui_model/documents/map_view_state.h"
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
