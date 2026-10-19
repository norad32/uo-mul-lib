#include "gui/panels/tile_inspector_panel.h"
#include "gui/map_workspace.h"
#include "gui/panels/common.h"
#include "gui/panels/radarcol_panel.h"
#include "gui/panels/texmaps_panel.h"
#include "gui/theme.h"
#include "uomul/map.h"
#include <cstddef>
#include <cstdint>
#include <imgui.h>
#include <string>

namespace uo_mule::gui
{

namespace
{

constexpr float tilePreviewSize = 40.0F;
constexpr float selectedBorderThickness = 2.0F;

void drawTilePreview(uomul::MapTile tile, MapDisplayMode mode, RadarcolPanelState & radarcolState)
{
    const ImVec2 cellMinimum = ImGui::GetCursorScreenPos();
    const ImVec2 cellMaximum(cellMinimum.x + tilePreviewSize, cellMinimum.y + tilePreviewSize);
    const MapColor mapColor = tileDisplayColor(mode, tile, radarMapDisplayColor(radarcolState, tile.id));
    ImDrawList * drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(cellMinimum, cellMaximum, IM_COL32(mapColor.red, mapColor.green, mapColor.blue, 255));
    drawList->AddRect(cellMinimum, cellMaximum, theme::packedColor(theme::stoneBorder), 0.0F, ImDrawFlags_None, 1.0F);
    drawList->AddRect(
        cellMinimum, cellMaximum, theme::packedColor(theme::brightBrass), 0.0F, ImDrawFlags_None, selectedBorderThickness);

    const std::string label = tileLabel(mode, tile);
    const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
    const ImVec2 textPosition(cellMinimum.x + ((tilePreviewSize - textSize.x) * 0.5F),
                              cellMinimum.y + ((tilePreviewSize - textSize.y) * 0.5F));
    const ImU32 inverseColor = IM_COL32(255U - mapColor.red, 255U - mapColor.green, 255U - mapColor.blue, 255U);
    drawList->AddText(textPosition, inverseColor, label.c_str());
    ImGui::Dummy(ImVec2(tilePreviewSize, tilePreviewSize));
}

void drawTexturePreview(const uo_mule::gui::TexmapsTexturePreview & texturePreview)
{
    constexpr float previewScaleSize = 128.0F;
    const float scaleX = previewScaleSize / static_cast<float>(texturePreview.width);
    const float scaleY = previewScaleSize / static_cast<float>(texturePreview.height);

    ImDrawList * drawList = ImGui::GetWindowDrawList();
    const ImVec2 cellMin = ImGui::GetCursorScreenPos();
    const ImVec2 cellMax(cellMin.x + previewScaleSize, cellMin.y + previewScaleSize);

    // Draw checkerboard background
    constexpr float checkerSize = 4.0F;
    constexpr ImU32 checkerColor1 = IM_COL32(64, 64, 64, 255);
    constexpr ImU32 checkerColor2 = IM_COL32(128, 128, 128, 255);

    for (int checkerY = 0; (static_cast<float>(checkerY) * checkerSize) < previewScaleSize; checkerY++)
    {
        for (int checkerX = 0; (static_cast<float>(checkerX) * checkerSize) < previewScaleSize; checkerX++)
        {
            const bool oddX = checkerX % 2 == 1;
            const bool oddY = checkerY % 2 == 1;
            const ImU32 pixelColor = (oddX != oddY) ? checkerColor1 : checkerColor2;
            const ImVec2 checkerMin(cellMin.x + (static_cast<float>(checkerX) * checkerSize),
                                     cellMin.y + (static_cast<float>(checkerY) * checkerSize));
            const ImVec2 checkerMax(checkerMin.x + checkerSize, checkerMin.y + checkerSize);
            drawList->AddRectFilled(checkerMin, checkerMax, pixelColor);
        }
    }

    // Draw texture
    for (std::uint32_t ty = 0; ty < texturePreview.height; ty++)
    {
        for (std::uint32_t tx = 0; tx < texturePreview.width; tx++)
        {
            const std::size_t pixelIndex = (ty * texturePreview.width) + tx;
            if (pixelIndex < texturePreview.pixels.size())
            {
                const MapColor & pixelColor = texturePreview.pixels.at(pixelIndex);
                const ImU32 imColor = IM_COL32(pixelColor.red, pixelColor.green, pixelColor.blue, 255);

                const ImVec2 pixelMin(cellMin.x + (static_cast<float>(tx) * scaleX),
                                       cellMin.y + (static_cast<float>(ty) * scaleY));
                const ImVec2 pixelMax(cellMin.x + (static_cast<float>(tx + 1) * scaleX),
                                       cellMin.y + (static_cast<float>(ty + 1) * scaleY));

                drawList->AddRectFilled(pixelMin, pixelMax, imColor);
            }
        }
    }

    drawList->AddRect(cellMin, cellMax, IM_COL32(255, 255, 255, 255), 0.0F, ImDrawFlags_None, 1.0F);
    ImGui::Dummy(ImVec2(previewScaleSize, previewScaleSize));
}

void drawTileDetails(const MapWorkspace & workspace, RadarcolPanelState & radarcolState, TexmapsPanelState & texmapsState)
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
    const MapColor mapColor = tileDisplayColor(workspace.displayMode(), value, radarMapDisplayColor(radarcolState, value.id));
    const ImVec4 color(static_cast<float>(mapColor.red) / theme::maximumColorChannel,
                       static_cast<float>(mapColor.green) / theme::maximumColorChannel,
                       static_cast<float>(mapColor.blue) / theme::maximumColorChannel,
                       1.0F);
    ImGui::BeginGroup();
    drawTilePreview(value, workspace.displayMode(), radarcolState);
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

    // Display texture from texmaps if available
    if (ImGui::CollapsingHeader("Texture Preview", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const auto texturePreview = texmapsTexturePreview(texmapsState, value.id);
        if (texturePreview && !texturePreview->pixels.empty())
        {
            ImGui::Text("Texture ID: %u", value.id);
            ImGui::Text("Dimension: %ux%u", texturePreview->width, texturePreview->height);
            drawTexturePreview(*texturePreview);
        }
        else
        {
            ImGui::TextUnformatted("Texture not available");
        }
    }
}

} // namespace

void drawTileInspectorPanel(const MapWorkspace & workspace, RadarcolPanelState & radarcolState,
                            TexmapsPanelState & texmapsState, bool & visible)
{
    const bool contentsVisible = ImGui::Begin("Tile Inspector###view.tile-inspector", &visible);

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
            drawTileDetails(workspace, radarcolState, texmapsState);
        }
    }

    ImGui::End();
}

} // namespace uo_mule::gui
