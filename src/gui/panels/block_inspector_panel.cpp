#include "gui/panels/block_inspector_panel.h"
#include "gui/presentation/theme.h"
#include "gui/documents/map_tile_labels.h"
#include "gui_model/documents/map_document_state.h"
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
constexpr int colorChannelMaximum = 255;
constexpr std::uint8_t alphaChannelMaximum = 255U;

[[nodiscard]] MapCanvasCachedTexture makeTextureCacheEntry(const TextureProvider & textures, std::uint32_t id)
{
    MapCanvasCachedTexture entry;
    const auto texture = textures.texture(id);
    if (!texture || texture->width == 0U || texture->height == 0U || texture->pixels.empty())
    {
        return entry;
    }

    entry.width = texture->width;
    entry.height = texture->height;
    entry.pixels = texture->pixels;
    entry.rgbaPixels.reserve(entry.pixels.size() * 4U);
    for (const MapColor pixel : entry.pixels)
    {
        entry.rgbaPixels.push_back(pixel.red);
        entry.rgbaPixels.push_back(pixel.green);
        entry.rgbaPixels.push_back(pixel.blue);
        entry.rgbaPixels.push_back(alphaChannelMaximum);
    }
    return entry;
}

void uploadTexture(MapCanvasCachedTexture & texture)
{
    if (texture.pixels.empty())
    {
        return;
    }
    if (texture.gpuTexture == 0U)
    {
        GLuint handle = 0;
        glGenTextures(1, &handle);
        texture.gpuTexture = handle;
    }
    if (texture.gpuTexture == 0U)
    {
        return;
    }
    GLint previousTexture = 0;
    GLint previousUnpackAlignment = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
    glBindTexture(GL_TEXTURE_2D, texture.gpuTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 static_cast<GLsizei>(texture.width),
                 static_cast<GLsizei>(texture.height),
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 texture.rgbaPixels.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
}

[[nodiscard]] ImU32 textureTint(MapDisplayMode mode, uomul::MapTile tile) noexcept
{
    if (mode != MapDisplayMode::combined)
    {
        return IM_COL32(255, 255, 255, 255);
    }
    const double brightness = 0.5 + (static_cast<double>(zGrayscale(tile.z)) / 255.0);
    const auto channel = static_cast<std::uint8_t>(std::clamp(static_cast<int>(brightness * 255.0), 0, colorChannelMaximum));
    return IM_COL32(channel, channel, channel, 255);
}

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
    const bool selected = mapDocument.viewState().selectedTile_ &&
                          mapDocument.viewState().selectedTile_->x == tileCoordinate.x &&
                          mapDocument.viewState().selectedTile_->y == tileCoordinate.y;
    MapCanvasCachedTexture * texture = nullptr;
    if (tileSource == MapViewTileSource::textures && mapDocument.viewState().displayMode_ != MapDisplayMode::z)
    {
        texture = &state.textureCache.try_emplace(tile.id, makeTextureCacheEntry(textures, tile.id)).first->second;
    }
    const bool useTexture = texture != nullptr && !texture->pixels.empty();
    const MapColor mapColor = tileDisplayColor(mapDocument.viewState().displayMode_, tile, radarColors.mapColor(tile.id));
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
            uploadTexture(*texture);
        }
        if (texture->gpuTexture != 0U)
        {
            drawList->AddImage(static_cast<ImTextureID>(texture->gpuTexture),
                               cellMinimum,
                               cellMaximum,
                               ImVec2(0.0F, 0.0F),
                               ImVec2(1.0F, 1.0F),
                               textureTint(mapDocument.viewState().displayMode_, tile));
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

    const std::string label = tileLabel(mapDocument.viewState().displayMode_, tile);
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
        ImGui::SetTooltip("Tile (%u, %u): ID %u, Z %d",
                          tileCoordinate.x,
                          tileCoordinate.y,
                          static_cast<unsigned int>(tile.id),
                          static_cast<int>(tile.z));
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
    for (auto & [id, texture] : state.textureCache)
    {
        static_cast<void>(id);
        if (texture.gpuTexture != 0U && state.releaseTexture)
        {
            state.releaseTexture(texture.gpuTexture);
        }
    }
    state.textureCache.clear();
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
    if (!releaseTexture)
    {
        return;
    }
    for (auto & [id, texture] : textureCache)
    {
        static_cast<void>(id);
        if (texture.gpuTexture != 0U)
        {
            releaseTexture(texture.gpuTexture);
        }
    }
}

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
    const uomul::MapReader * reader = mapDocument.changes().reader_ ? &*mapDocument.changes().reader_ : nullptr;
    if (reader == nullptr || (state.cacheValid && state.mapPath != mapDocument.changes().mapPath_))
    {
        state.cacheValid = false;
        state.block.reset();
        state.error.reset();
        state.blockCoordinate.reset();
    }

    invalidateTexturePreviews(state, providers.textures(), tileSource);

    const std::optional<uomul::TileCoord> inspectedTile = context.activeTile;

    if (reader != nullptr && inspectedTile)
    {
        const uomul::BlockCoord coordinate = tileToBlock(*inspectedTile);

        if (!state.cacheValid || !state.blockCoordinate || state.blockCoordinate->x != coordinate.x ||
            state.blockCoordinate->y != coordinate.y)
        {
            refreshBlock(*reader, mapDocument.changes().mapPath_, coordinate, state);
        }
    }

    const bool contentsVisible = ImGui::Begin("Block Inspector###view.block-inspector", &visible);
    if (contentsVisible)
    {
        if (reader == nullptr)
        {
            ImGui::TextUnformatted("No map open");
        }
        else if (!state.blockCoordinate)
        {
            ImGui::TextUnformatted("Hover a block in the map canvas to inspect it");
        }
        else
        {
            const uomul::BlockCoord blockCoordinate = *state.blockCoordinate;
            ImGui::Text("Block: (%u, %u)", blockCoordinate.x, blockCoordinate.y);
            ImGui::Text("Tile coordinates: X %u-%u, Y %u-%u",
                        blockCoordinate.x * uomul::mapBlockDimension,
                        (blockCoordinate.x * uomul::mapBlockDimension) + uomul::mapBlockDimension - 1U,
                        blockCoordinate.y * uomul::mapBlockDimension,
                        (blockCoordinate.y * uomul::mapBlockDimension) + uomul::mapBlockDimension - 1U);
            if (state.block)
            {
                drawBlock(*state.block,
                          blockCoordinate,
                          mapDocument,
                          providers.radarColors(),
                          providers.textures(),
                          tileSource,
                          state);
                if (context.selectedTile && context.selectedValue)
                {
                    const uomul::TileCoord tileCoordinate = *context.selectedTile;
                    const uomul::MapTile tile = *context.selectedValue;
                    ImGui::Text("Selected tile (%u, %u): ID %u, Z %d",
                                tileCoordinate.x,
                                tileCoordinate.y,
                                static_cast<unsigned int>(tile.id),
                                static_cast<int>(tile.z));
                }
            }
            else if (state.error)
            {
                drawBlockError(*state.error);
            }
        }

        if (mapDocument.changes().error_)
        {
            const std::string message = "Map error: " + mapDocument.changes().error_->message;
            ImGui::TextColored(theme::imguiColor(theme::error), "%s", message.c_str());
        }
    }

    ImGui::End();
}

} // namespace uo_mule::gui
