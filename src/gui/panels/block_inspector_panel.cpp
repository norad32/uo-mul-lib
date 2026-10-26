#include "gui/panels/block_inspector_panel.h"
#include "gui/documents/map_tile_labels.h"
#include "gui/presentation/map_texture_cache.h"
#include "gui/presentation/panel_view.h"
#include "gui/presentation/strings.h"
#include "gui/presentation/theme.h"
#include "gui_model/documents/map_document_state.h"
#include "gui_model/documents/map_view_state.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/providers/data_providers.h"
#include "gui_model/world/map_canvas_state.h"
#include "gui_model/world/map_color.h"
#include "gui_model/world/map_context.h"
#include "uomul/map/map.h"
#include "uomul/mul_common/mul_common.h"
#include <GL/gl.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <imgui.h>
#include <optional>
#include <string>

namespace uo_mule::gui
{

namespace
{

constexpr float tileCellSize = 40.0F;
constexpr float coordinateColumnWidth = 38.0F;
constexpr float selectedBorderThickness = 2.0F;
constexpr float gridLineThickness = 1.0F;
void drawTileCell(const uomul::MapBlock & block,
                  uomul::BlockCoord blockCoordinate,
                  MapDocumentState & mapDocument,
                  const RadarColorProvider & radarColors,
                  const TextureProvider & textures,
                  MapViewTileSource tileSource,
                  BlockInspectorPanelState & state,
                  std::size_t row,
                  std::size_t column,
                  ImVec2 gridOrigin)
{
    const std::size_t index = (row * uomul::mapBlockDimension) + column;
    const uomul::MapTile tile = block.at(index);
    const uomul::TileCoord tileCoordinate{
        .x = (blockCoordinate.x * uomul::mapBlockDimension) + static_cast<std::uint32_t>(column),
        .y = (blockCoordinate.y * uomul::mapBlockDimension) + static_cast<std::uint32_t>(row),
    };
    const auto selectedTile = mapDocument.viewState().selectedTile;
    const bool selected = selectedTile && selectedTile->x == tileCoordinate.x && selectedTile->y == tileCoordinate.y;
    MapCanvasCachedTexture * texture = nullptr;
    if (tileSource == MapViewTileSource::textures && mapDocument.viewState().displayMode != MapDisplayMode::z)
    {
        texture = &cachedMapTexture(state.textureCache, textures, tile.id);
    }
    const bool useTexture = texture != nullptr && !texture->pixels.empty();
    const MapColor mapColor = tileDisplayColor(mapDocument.viewState().displayMode, tile, radarColors.mapColor(tile.id));
    const ImVec2 cellMinimum(gridOrigin.x + coordinateColumnWidth + (static_cast<float>(column) * tileCellSize),
                             gridOrigin.y + ImGui::GetTextLineHeightWithSpacing() + (static_cast<float>(row) * tileCellSize));
    const ImVec2 cellMaximum(cellMinimum.x + tileCellSize, cellMinimum.y + tileCellSize);

    ImGui::SetCursorScreenPos(cellMinimum);
    ImGui::PushID(static_cast<int>(index));
    const bool clicked = ImGui::InvisibleButton("##BlockTile", ImVec2(tileCellSize, tileCellSize));
    ImGui::PopID();

    ImDrawList * drawList = ImGui::GetWindowDrawList();
    if (useTexture)
    {
        if (texture->gpuTexture == 0U)
        {
            uploadMapTexture(*texture);
        }
        if (texture->gpuTexture != 0U)
        {
            drawList->AddImage(static_cast<ImTextureID>(texture->gpuTexture),
                               cellMinimum,
                               cellMaximum,
                               ImVec2(0.0F, 0.0F),
                               ImVec2(1.0F, 1.0F),
                               mapTextureTint(mapDocument.viewState().displayMode, tile));
        }
        else
        {
            drawList->AddRectFilled(cellMinimum, cellMaximum, IM_COL32(mapColor.red, mapColor.green, mapColor.blue, 255));
        }
    }
    else
    {
        drawList->AddRectFilled(cellMinimum, cellMaximum, IM_COL32(mapColor.red, mapColor.green, mapColor.blue, 255));
    }
    drawList->AddRect(
        cellMinimum, cellMaximum, theme::packedOpaqueColor(theme::border), 0.0F, ImDrawFlags_None, gridLineThickness);
    if (selected)
    {
        drawList->AddRect(
            cellMinimum, cellMaximum, theme::packedColor(theme::selection), 0.0F, ImDrawFlags_None, selectedBorderThickness);
    }

    const std::string label = tileLabel(mapDocument.viewState().displayMode, tile);
    const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
    const ImVec2 textPosition(cellMinimum.x + ((tileCellSize - textSize.x) * 0.5F),
                              cellMinimum.y + ((tileCellSize - textSize.y) * 0.5F));
    const ImU32 inverseColor = IM_COL32(255U - mapColor.red, 255U - mapColor.green, 255U - mapColor.blue, 255U);
    drawList->AddText(textPosition, inverseColor, label.c_str());

    if (clicked)
    {
        const auto result = mapDocument.updateSelectedTile(tileCoordinate);
        static_cast<void>(result);
    }

    if (ImGui::IsItemHovered())
    {
        const std::string tooltip = strings::formatTileTooltip(tileCoordinate.x,
                                                               tileCoordinate.y,
                                                               tile.id,
                                                               static_cast<std::int32_t>(tile.z));
        ImGui::SetTooltip("%s", tooltip.c_str());
    }
}

void drawBlock(const uomul::MapBlock & block,
               uomul::BlockCoord blockCoordinate,
               MapDocumentState & mapDocument,
               const RadarColorProvider & radarColors,
               const TextureProvider & textures,
               MapViewTileSource tileSource,
               BlockInspectorPanelState & state)
{
    const ImVec2 gridOrigin = ImGui::GetCursorScreenPos();
    ImDrawList * drawList = ImGui::GetWindowDrawList();
    const float axisTextY = gridOrigin.y;
    drawList->AddText(ImVec2(gridOrigin.x, axisTextY), theme::packedColor(theme::primaryText), "Y/X");

    for (std::size_t column = 0; column < uomul::mapBlockDimension; column++)
    {
        const std::string coordinate =
            std::to_string((blockCoordinate.x * uomul::mapBlockDimension) + static_cast<std::uint32_t>(column));
        const ImVec2 textSize = ImGui::CalcTextSize(coordinate.c_str());
        const float x = gridOrigin.x + coordinateColumnWidth + (static_cast<float>(column) * tileCellSize) +
                        ((tileCellSize - textSize.x) * 0.5F);
        drawList->AddText(ImVec2(x, axisTextY), theme::packedColor(theme::primaryText), coordinate.c_str());
    }

    for (std::size_t row = 0; row < uomul::mapBlockDimension; row++)
    {
        const std::string coordinate =
            std::to_string((blockCoordinate.y * uomul::mapBlockDimension) + static_cast<std::uint32_t>(row));
        const ImVec2 textSize = ImGui::CalcTextSize(coordinate.c_str());
        const float y = gridOrigin.y + ImGui::GetTextLineHeightWithSpacing() + (static_cast<float>(row) * tileCellSize) +
                        ((tileCellSize - textSize.y) * 0.5F);
        drawList->AddText(ImVec2(gridOrigin.x + coordinateColumnWidth - textSize.x - ImGui::GetStyle().ItemSpacing.x, y),
                          theme::packedColor(theme::primaryText),
                          coordinate.c_str());
        for (std::size_t column = 0; column < uomul::mapBlockDimension; column++)
        {
            drawTileCell(
                block, blockCoordinate, mapDocument, radarColors, textures, tileSource, state, row, column, gridOrigin);
        }
    }

    const float gridHeight =
        ImGui::GetTextLineHeightWithSpacing() + (static_cast<float>(uomul::mapBlockDimension) * tileCellSize);
    ImGui::SetCursorScreenPos(ImVec2(gridOrigin.x, gridOrigin.y + gridHeight));
    ImGui::Dummy(ImVec2(coordinateColumnWidth + (static_cast<float>(uomul::mapBlockDimension) * tileCellSize), 0.0F));
}

void refreshBlock(const uomul::MapReader & reader,
                  const std::filesystem::path & path,
                  uomul::BlockCoord coordinate,
                  BlockInspectorPanelState & state)
{
    state.mapPath = path;
    state.blockCoordinate = coordinate;
    state.block.reset();
    state.error.reset();
    state.cacheValid = true;
    auto result = reader.readBlock(coordinate);

    if (result)
    {
        state.block = *result;
    }
    else
    {
        state.error = result.error();
    }
}

void invalidateTexturePreviews(BlockInspectorPanelState & state, const TextureProvider & textures, MapViewTileSource tileSource)
{
    if (state.textureProviderRevision == textures.status().revision && state.textureSource == tileSource)
    {
        return;
    }
    clearMapTextureCache(state.textureCache, state.releaseTexture);
    state.textureProviderRevision = textures.status().revision;
    state.textureSource = tileSource;
}

void drawBlockError(const uomul::Error & error)
{
    ImGui::TextColored(theme::imguiColor(theme::error), "Block read error: %s", error.message.c_str());
}

} // namespace

BlockInspectorPanelState::~BlockInspectorPanelState()
{
    clearMapTextureCache(textureCache, releaseTexture);
}

namespace
{

void drawBlockInspectorContents(MapDocumentState & mapDocument,
                                const MapContextSnapshot & context,
                                const DataProviders & providers,
                                MapViewTileSource tileSource,
                                BlockInspectorPanelState & state,
                                const uomul::MapReader * reader)
{
    if (reader == nullptr)
    {
        ImGui::TextUnformatted(strings::noMapOpen());
    }
    else if (!state.blockCoordinate)
    {
        ImGui::TextUnformatted(strings::hoverBlockToInspect());
    }
    else
    {
        const uomul::BlockCoord blockCoordinate = *state.blockCoordinate;
        const std::string blockLabel = strings::formatBlock(blockCoordinate.x, blockCoordinate.y);
        const std::string coordinates = strings::formatTileCoordinates(
            blockCoordinate.x * uomul::mapBlockDimension,
            (blockCoordinate.x * uomul::mapBlockDimension) + uomul::mapBlockDimension - 1U,
            blockCoordinate.y * uomul::mapBlockDimension,
            (blockCoordinate.y * uomul::mapBlockDimension) + uomul::mapBlockDimension - 1U);
        ImGui::TextUnformatted(blockLabel.c_str());
        ImGui::TextUnformatted(coordinates.c_str());
        if (state.block)
        {
            drawBlock(
                *state.block, blockCoordinate, mapDocument, providers.radarColors(), providers.textures(), tileSource, state);
            if (context.selectedTile && context.selectedValue)
            {
                const uomul::TileCoord tileCoordinate = *context.selectedTile;
                const uomul::MapTile tile = *context.selectedValue;
                const std::string selected = strings::formatSelectedTile(tileCoordinate.x,
                                                                          tileCoordinate.y,
                                                                          tile.id,
                                                                          static_cast<std::int32_t>(tile.z));
                ImGui::TextUnformatted(selected.c_str());
            }
        }
        else if (state.error)
        {
            drawBlockError(*state.error);
        }
    }

    if (const auto & error = mapDocument.changes().error; error)
    {
        const std::string message = "Map error: " + error->message;
        ImGui::TextColored(theme::imguiColor(theme::error), "%s", message.c_str());
    }
}

} // namespace

void drawBlockInspectorPanel(MapDocumentState & mapDocument,
                             const MapContextSnapshot & context,
                             const DataProviders & providers,
                             MapViewTileSource tileSource,
                             BlockInspectorPanelState & state,
                             bool & visible)
{
    if (!state.releaseTexture)
    {
        state.releaseTexture = [](std::uint32_t texture)
        {
            const GLuint handle = texture;
            glDeleteTextures(1, &handle);
        };
    }
    const auto & readerOptional = mapDocument.changes().reader;
    const uomul::MapReader * reader = readerOptional ? readerOptional.operator->() : nullptr;
    if (reader == nullptr || (state.cacheValid && state.mapPath != mapDocument.changes().mapPath))
    {
        state.cacheValid = false;
        state.block.reset();
        state.error.reset();
        state.blockCoordinate.reset();
    }

    invalidateTexturePreviews(state, providers.textures(), tileSource);

    if (reader != nullptr && context.activeTile)
    {
        const uomul::BlockCoord coordinate = tileToBlock(*context.activeTile);

        if (!state.cacheValid || !state.blockCoordinate || state.blockCoordinate->x != coordinate.x ||
            state.blockCoordinate->y != coordinate.y)
        {
            refreshBlock(*reader, mapDocument.changes().mapPath, coordinate, state);
        }
    }

    drawPanelView("Block Inspector###panel.block-inspector",
                   visible,
                   [&mapDocument, &context, &providers, tileSource, &state, reader]
                   { drawBlockInspectorContents(mapDocument, context, providers, tileSource, state, reader); });
}

} // namespace uo_mule::gui
