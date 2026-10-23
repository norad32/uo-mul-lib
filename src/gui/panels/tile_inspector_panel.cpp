#include "gui/panels/tile_inspector_panel.h"
#include "gui/presentation/strings/ui_strings.h"
#include "gui/presentation/theme/theme.h"
#include "gui/widgets/texture_preview.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/providers/data_providers.h"
#include "gui_model/workspaces/map_workspace.h"
#include "gui_model/world/map_color.h"
#include "gui_model/world/map_context.h"
#include "uomul/map/map.h"
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

void drawTileDetails(const MapContextSnapshot & context, const DataProviders & providers, IdFormat idFormat)
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
            drawTexturePreview(
                {.width = texturePreview->width, .height = texturePreview->height, .pixels = texturePreview->pixels},
                assetPreviewSize);
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
