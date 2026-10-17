#include "gui/tile_inspector_panel.h"

#include <imgui.h>

#include "gui/map_workspace.h"
#include "uomul/map.h"

namespace im_cpp::gui
{

void drawTileInspectorPanel(const MapWorkspace & workspace, bool & visible)
{
    const bool contentsVisible = ImGui::Begin("Tile Inspector", &visible);
    if (contentsVisible)
    {
        if (workspace.reader() == nullptr)
        {
            ImGui::TextUnformatted("No map open");
        }
        else if ((!workspace.hoveredTile() || !workspace.hoveredValue()) &&
                 (!workspace.selectedTile() || !workspace.selectedValue()))
        {
            ImGui::TextUnformatted("No tile under pointer or selected");
        }
        else
        {
            const auto & tileValue = workspace.hoveredTile() ? workspace.hoveredTile() : workspace.selectedTile();
            const auto & dataValue = workspace.hoveredValue() ? workspace.hoveredValue() : workspace.selectedValue();
            if (!tileValue || !dataValue)
            {
                ImGui::TextUnformatted("Tile inspection data unavailable");
                ImGui::End();
                return;
            }
            const uomul::TileCoord tile = tileValue.value();
            const uomul::MapTile value = dataValue.value();
            const uomul::BlockCoord block = tileToBlock(tile);
            ImGui::Text("Tile: (%u, %u)", tile.x, tile.y);
            ImGui::Text("ID: %u", static_cast<unsigned int>(value.id));
            ImGui::Text("Z: %d", static_cast<int>(value.z));
            ImGui::Text("Block: (%u, %u)", block.x, block.y);
        }
    }
    ImGui::End();
}

} // namespace im_cpp::gui
