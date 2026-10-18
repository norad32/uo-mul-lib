#include "gui/tile_inspector_panel.h"
#include "gui/map_workspace.h"
#include "gui/theme.h"
#include "uomul/map.h"
#include <imgui.h>
#include <string>

namespace uo_mule::gui
{

namespace
{

constexpr float tilePreviewSize = 40.0F;
constexpr float selectedBorderThickness = 2.0F;
constexpr float textOutlineOffset = 1.0F;

[[nodiscard]] std::string tileLabel(MapDisplayMode mode, uomul::MapTile tile)
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

void drawTilePreview(uomul::MapTile tile, MapDisplayMode mode)
{
    const ImVec2 cellMinimum = ImGui::GetCursorScreenPos();
    const ImVec2 cellMaximum(cellMinimum.x + tilePreviewSize, cellMinimum.y + tilePreviewSize);
    const MapColor mapColor = tileDisplayColor(mode, tile);
    ImDrawList * drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(cellMinimum, cellMaximum, IM_COL32(mapColor.red, mapColor.green, mapColor.blue, 255));
    drawList->AddRect(cellMinimum, cellMaximum, theme::packedColor(theme::stoneBorder), 0.0F, ImDrawFlags_None, 1.0F);
    drawList->AddRect(
        cellMinimum, cellMaximum, theme::packedColor(theme::brightBrass), 0.0F, ImDrawFlags_None, selectedBorderThickness);

    const std::string label = tileLabel(mode, tile);
    const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
    const ImVec2 textPosition(cellMinimum.x + ((tilePreviewSize - textSize.x) * 0.5F),
                              cellMinimum.y + ((tilePreviewSize - textSize.y) * 0.5F));
    const ImU32 outlineColor = theme::packedColor(theme::stoneSurface);
    const ImU32 textColor = theme::packedColor(theme::parchmentText);
    drawList->AddText(ImVec2(textPosition.x - textOutlineOffset, textPosition.y), outlineColor, label.c_str());
    drawList->AddText(ImVec2(textPosition.x + textOutlineOffset, textPosition.y), outlineColor, label.c_str());
    drawList->AddText(ImVec2(textPosition.x, textPosition.y - textOutlineOffset), outlineColor, label.c_str());
    drawList->AddText(ImVec2(textPosition.x, textPosition.y + textOutlineOffset), outlineColor, label.c_str());
    drawList->AddText(textPosition, textColor, label.c_str());
    ImGui::Dummy(ImVec2(tilePreviewSize, tilePreviewSize));
}

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
    ImGui::BeginGroup();
    drawTilePreview(value, workspace.displayMode());
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::BeginGroup();
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
    ImGui::EndGroup();
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

} // namespace uo_mule::gui
