#include "gui/panels/tile_inspector_panel.h"
#include "gui/presentation/iconography.h"
#include "gui/presentation/strings.h"
#include "gui/presentation/texture_preview.h"
#include "gui/presentation/theme.h"
#include "gui_model/documents/map_document_state.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/providers/data_providers.h"
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

void drawTileDetails(const MapContextSnapshot & context,
                     const DataProviders & providers,
                     IdFormat idFormat,
                     bool iconFontAvailable)
{
    const auto & tileValue = context.activeTile;
    const auto & dataValue = context.activeValue;

    if (!tileValue || !dataValue)
    {
        ImGui::TextUnformatted(strings::tileInspectionUnavailable());
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
    const std::string tilePosition = strings::formatTile(tile.x, tile.y);
    const std::string blockPosition = strings::formatBlock(block.x, block.y);
    ImGui::TextUnformatted(tilePosition.c_str());
    ImGui::TextUnformatted(blockPosition.c_str());
    const std::string idLabel = strings::formatId(value.id, idFormat);
    const std::string tileIdLabel = iconLabel(IconId::tileId, strings::id(), iconFontAvailable);
    ImGui::Text("%s: %s", tileIdLabel.c_str(), idLabel.c_str());
    const std::string elevationLabel = iconLabel(IconId::elevation, strings::elevation(), iconFontAvailable);
    ImGui::Text("%s: %d", elevationLabel.c_str(), static_cast<int>(value.z));
    ImGui::EndGroup();
}

} // namespace

void drawTileInspectorPanel(const MapDocumentState & mapDocument,
                            const MapContextSnapshot & context,
                            const DataProviders & providers,
                            IdFormat idFormat,
                            bool iconFontAvailable,
                            bool & visible)
{
    const bool contentsVisible = ImGui::Begin("Tile Inspector###view.tile-inspector", &visible);

    if (contentsVisible)
    {
        if (!mapDocument.changes().reader.has_value())
        {
            ImGui::TextUnformatted(strings::noMapOpen());
        }
        else if (!context.activeTile || !context.activeValue)
        {
            ImGui::TextUnformatted(strings::noTileUnderPointerOrSelected());
        }
        else
        {
            drawTileDetails(context, providers, idFormat, iconFontAvailable);
        }
    }

    ImGui::End();
}

} // namespace uo_mule::gui
