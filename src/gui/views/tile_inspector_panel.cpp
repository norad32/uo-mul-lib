#include "gui/views/tile_inspector_panel.h"
#include "gui/persistence/preferences.h"
#include "gui/strings/ui_strings.h"
#include "gui/context/map_context.h"
#include "gui/providers/data_providers.h"
#include "gui/documents/map_workspace.h"
#include "gui/frontend/theme.h"
#include "uomul/map.h"
#include <cstddef>
#include <cstdint>
#include <imgui.h>
#include <optional>
#include <string>

namespace uo_mule::gui
{

namespace
{

constexpr float assetPreviewSize = 128.0F;

void drawColorPreview(MapColor color)
{
    const ImVec2 cellMinimum = ImGui::GetCursorScreenPos();
    const ImVec2 cellMaximum(cellMinimum.x + assetPreviewSize, cellMinimum.y + assetPreviewSize);
    ImDrawList * drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(cellMinimum, cellMaximum, IM_COL32(color.red, color.green, color.blue, 255));
    drawList->AddRect(cellMinimum, cellMaximum, theme::packedColor(theme::border), 0.0F, ImDrawFlags_None, 1.0F);
    ImGui::Dummy(ImVec2(assetPreviewSize, assetPreviewSize));
}

void drawTexturePreview(const ProviderTexture & texturePreview)
{
    const float scaleX = assetPreviewSize / static_cast<float>(texturePreview.width);
    const float scaleY = assetPreviewSize / static_cast<float>(texturePreview.height);

    ImDrawList * drawList = ImGui::GetWindowDrawList();
    const ImVec2 cellMin = ImGui::GetCursorScreenPos();
    const ImVec2 cellMax(cellMin.x + assetPreviewSize, cellMin.y + assetPreviewSize);

    // Draw checkerboard background
    constexpr float checkerSize = 4.0F;
    constexpr ImU32 checkerColor1 = IM_COL32(64, 64, 64, 255);
    constexpr ImU32 checkerColor2 = IM_COL32(128, 128, 128, 255);

    for (int checkerY = 0; (static_cast<float>(checkerY) * checkerSize) < assetPreviewSize; checkerY++)
    {
        for (int checkerX = 0; (static_cast<float>(checkerX) * checkerSize) < assetPreviewSize; checkerX++)
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
    ImGui::Dummy(ImVec2(assetPreviewSize, assetPreviewSize));
}

void drawTileDetails(const MapContextSnapshot & context,
                     const DataProviders & providers,
                     IdFormat idFormat)
{
    const auto & tileValue = context.activeTile;
    const auto & dataValue = context.activeValue;

    if (!tileValue || !dataValue)
    {
        ImGui::TextUnformatted("Tile inspection data unavailable");
        return;
    }

    const uomul::TileCoord tile = *tileValue;
    const uomul::MapTile value = *dataValue;
    const uomul::BlockCoord block = tileToBlock(tile);
    const std::optional<MapColor> radarColor = providers.radarColors().mapColor(value.id);
    const auto texturePreview = providers.textures().texture(value.id);
    const bool hasTexturePreview = texturePreview && !texturePreview->pixels.empty();
    if (hasTexturePreview || radarColor)
    {
        ImGui::BeginGroup();
        if (hasTexturePreview)
        {
            drawTexturePreview(*texturePreview);
        }
        else
        {
            drawColorPreview(*radarColor);
        }
        ImGui::EndGroup();
        ImGui::SameLine();
    }
    ImGui::BeginGroup();
    ImGui::Text("Tile: (%u, %u)", tile.x, tile.y);
    ImGui::Text("Block: (%u, %u)", block.x, block.y);
    const std::string idLabel = strings::formatId(value.id, idFormat);
    ImGui::Text("ID: %s", idLabel.c_str());
    ImGui::Text("Elevation: %d", static_cast<int>(value.z));
    ImGui::EndGroup();
}

} // namespace

void drawTileInspectorPanel(const MapWorkspace & workspace,
                             const MapContextSnapshot & context,
                             const DataProviders & providers,
                             IdFormat idFormat,
                             bool & visible)
{
    const bool contentsVisible = ImGui::Begin("Tile Inspector###view.tile-inspector", &visible);

    if (contentsVisible)
    {
        if (workspace.reader() == nullptr)
        {
            ImGui::TextUnformatted("No map open");
        }
        else if (!context.activeTile || !context.activeValue)
        {
            ImGui::TextUnformatted("No tile under pointer or selected");
        }
        else
        {
            drawTileDetails(context, providers, idFormat);
        }
    }

    ImGui::End();
}

} // namespace uo_mule::gui
