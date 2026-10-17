#include "gui/tile_inspector_panel.h"

#include <imgui.h>

#include "gui/map_workspace.h"
#include "gui/theme.h"
#include "uomul/map.h"

namespace im_cpp::gui
{
namespace
{

void drawTileDetails(const MapWorkspace & workspace)
{
    const auto & tileValue = workspace.hoveredTile() ? workspace.hoveredTile() : workspace.selectedTile();
    const auto & dataValue = workspace.hoveredValue() ? workspace.hoveredValue() : workspace.selectedValue();
    if (!tileValue || !dataValue)
    {
        ImGui::TextUnformatted("Tile inspection data unavailable");
        return;
    }

    const uomul::TileCoord tile = *tileValue;
    const uomul::MapTile value = *dataValue;
    const uomul::BlockCoord block = tileToBlock(tile);
    const MapColor mapColor = tileDisplayColor(workspace.displayMode(), value);
    const ImVec4 color(static_cast<float>(mapColor.red) / theme::maximumColorChannel,
                       static_cast<float>(mapColor.green) / theme::maximumColorChannel,
                       static_cast<float>(mapColor.blue) / theme::maximumColorChannel,
                       1.0F);
    ImGui::Text("Tile: (%u, %u)", tile.x, tile.y);
    if (workspace.displayMode() != MapDisplayMode::z)
    {
        ImGui::TextColored(color, "ID: %u", static_cast<unsigned int>(value.id));
    }
    if (workspace.displayMode() != MapDisplayMode::tileIds)
    {
        ImGui::TextColored(color, "Z: %d", static_cast<int>(value.z));
    }
    ImGui::Text("Block: (%u, %u)", block.x, block.y);
}

} // namespace

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
            drawTileDetails(workspace);
        }
    }
    ImGui::End();
}

} // namespace im_cpp::gui
