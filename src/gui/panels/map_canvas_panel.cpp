#include "gui/panels/map_canvas_panel.h"
#include "gui/map_workspace.h"
#include "gui/panels/radarcol_panel.h"
#include "gui/theme.h"
#include "uomul/map.h"
#include "uomul/mul_common/mul_common.h"
#include <GL/gl.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <imgui.h>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace uo_mule::gui
{

namespace
{

constexpr double minimumCanvasZoom = 0.125;
constexpr double maximumCanvasZoom = 64.0;
constexpr double canvasZoomFactor = 1.15;
constexpr double maximumPanMagnitude = 1.0e12;
constexpr double lodRasterScale = 0.5;
constexpr double lodZoomThreshold = 1.0;
constexpr std::size_t lodPixelChannels = 4;
constexpr std::uint32_t colorChannelMask = std::numeric_limits<std::uint8_t>::max();
constexpr std::uint32_t colorChannelMaximum = std::numeric_limits<std::uint8_t>::max();
constexpr double floatCacheTolerance = 0.001;
constexpr std::size_t maximumCachedBlocks = 512;
constexpr double selectionOutlineThickness = 2.0;
constexpr double errorTextLeftPadding = 10.0;
constexpr double errorTextTopPadding = 8.0;
constexpr double readErrorTextOffset = 28.0;
constexpr double noMapTextPadding = 12.0;

[[nodiscard]] ImU32 colorForTile(MapDisplayMode mode, uomul::MapTile tile, RadarcolPanelState & radarcolState)
{
    const MapColor color = tileDisplayColor(mode, tile, radarMapDisplayColor(radarcolState, tile.id));
    return IM_COL32(color.red, color.green, color.blue, std::numeric_limits<std::uint8_t>::max());
}

[[nodiscard]] ImU32 unavailableColor(std::uint32_t tileX, std::uint32_t tileY) noexcept
{
    return theme::packedColor(((tileX / uomul::mapBlockDimension) + (tileY / uomul::mapBlockDimension)) % 2U == 0U
                                  ? theme::oxblood
                                  : theme::brickRed);
}

[[nodiscard]] std::uint32_t visibleStart(double pan, double tileSize, std::uint32_t limit) noexcept
{
    const double tile = std::floor((-pan / tileSize)) - 1.0;
    return static_cast<std::uint32_t>(std::clamp(tile, 0.0, static_cast<double>(limit)));
}

[[nodiscard]] std::uint32_t visibleEnd(double screenExtent, double pan, double tileSize, std::uint32_t limit) noexcept
{
    const double tile = std::ceil((screenExtent - pan) / tileSize) + 1.0;
    return static_cast<std::uint32_t>(std::clamp(tile, 0.0, static_cast<double>(limit)));
}

[[nodiscard]] const MapCanvasCachedBlock &
cachedBlock(const MapWorkspace & workspace, MapCanvasPanelState & state, uomul::BlockCoord coordinate)
{
    const std::uint64_t cacheKey = (static_cast<std::uint64_t>(coordinate.x) << 32U) | coordinate.y;
    const auto cachedIndex = state.blockCacheIndex.find(cacheKey);

    if (cachedIndex != state.blockCacheIndex.end())
    {
        MapCanvasCachedBlock & existing = state.blockCache.at(cachedIndex->second);
        if (state.cacheClock == std::numeric_limits<std::uint64_t>::max())
        {
            state.cacheClock = 0;
            for (MapCanvasCachedBlock & entry : state.blockCache)
            {
                entry.lastUsed = state.cacheClock;
                ++state.cacheClock;
            }
        }

        existing.lastUsed = state.cacheClock;
        state.cacheClock++;
        return existing;
    }

    MapCanvasCachedBlock entry{.coordinate = coordinate, .block = std::nullopt, .error = std::nullopt, .lastUsed = 0};
    const uomul::MapReader * reader = workspace.reader();

    if (reader != nullptr)
    {
        auto result = reader->readBlock(coordinate);
        if (result)
        {
            entry.block = *result;
        }
        else
        {
            entry.error = result.error();
        }
    }

    entry.lastUsed = state.cacheClock;
    state.cacheClock++;

    if (state.blockCache.size() < maximumCachedBlocks)
    {
        state.blockCache.push_back(std::move(entry));
        const std::size_t index = state.blockCache.size() - 1U;
        state.blockCacheIndex.emplace(cacheKey, index);
        return state.blockCache.back();
    }

    const auto leastRecentlyUsed = std::ranges::min_element(
        state.blockCache,
        [](const MapCanvasCachedBlock & left, const MapCanvasCachedBlock & right) { return left.lastUsed < right.lastUsed; });
    const std::uint64_t leastRecentlyUsedKey =
        (static_cast<std::uint64_t>(leastRecentlyUsed->coordinate.x) << 32U) | leastRecentlyUsed->coordinate.y;
    state.blockCacheIndex.erase(leastRecentlyUsedKey);
    const std::size_t index = static_cast<std::size_t>(std::distance(state.blockCache.begin(), leastRecentlyUsed));
    *leastRecentlyUsed = std::move(entry);
    state.blockCacheIndex.emplace(cacheKey, index);

    return *leastRecentlyUsed;
}

[[nodiscard]] std::optional<uomul::TileCoord> tileAtMouse(ImVec2 mouse,
                                                          ImVec2 canvasOrigin,
                                                          ImVec2 canvasSize,
                                                          const MapCanvasPanelState & state,
                                                          uomul::Dimensions dimensions,
                                                          bool canvasHovered) noexcept
{
    if (!canvasHovered || mouse.x < canvasOrigin.x || mouse.y < canvasOrigin.y || mouse.x >= canvasOrigin.x + canvasSize.x ||
        mouse.y >= canvasOrigin.y + canvasSize.y)
    {
        return std::nullopt;
    }

    return screenToTile(mouse.x, mouse.y, canvasOrigin.x, canvasOrigin.y, state.panX, state.panY, state.zoom, dimensions);
}

void drawSelection(ImDrawList & drawList,
                   ImVec2 canvasOrigin,
                   MapCanvasPanelState & state,
                   const MapWorkspace & workspace,
                   uomul::Dimensions dimensions)
{
    std::optional<uomul::TileRegion> selection = workspace.tileSelection();

    if (workspace.selectionMode() == MapSelectionMode::blocks && workspace.blockSelection())
    {
        const uomul::BlockRegion blocks = *workspace.blockSelection();
        selection = uomul::TileRegion{
            .min = {.x = blocks.min.x * uomul::mapBlockDimension, .y = blocks.min.y * uomul::mapBlockDimension},
            .maxExclusive =
                {
                    .x = std::min(dimensions.widthTiles, blocks.maxExclusive.x * uomul::mapBlockDimension),
                    .y = std::min(dimensions.heightTiles, blocks.maxExclusive.y * uomul::mapBlockDimension),
                },
        };
    }

    if (state.dragStart && state.dragEnd)
    {
        if (workspace.selectionMode() == MapSelectionMode::tiles)
        {
            selection = tileSelectionFromDrag(*state.dragStart, *state.dragEnd, dimensions);
        }
        else
        {
            const uomul::BlockRegion blocks = blockSelectionFromDrag(*state.dragStart, *state.dragEnd, dimensions);
            selection = uomul::TileRegion{
                .min = {.x = blocks.min.x * uomul::mapBlockDimension, .y = blocks.min.y * uomul::mapBlockDimension},
                .maxExclusive =
                    {
                        .x = static_cast<std::uint32_t>(
                            std::min(static_cast<std::uint64_t>(dimensions.widthTiles),
                                     static_cast<std::uint64_t>(blocks.maxExclusive.x) * uomul::mapBlockDimension)),
                        .y = static_cast<std::uint32_t>(
                            std::min(static_cast<std::uint64_t>(dimensions.heightTiles),
                                     static_cast<std::uint64_t>(blocks.maxExclusive.y) * uomul::mapBlockDimension)),
                    },
            };
        }
    }

    if (!selection || selection->min.x >= selection->maxExclusive.x || selection->min.y >= selection->maxExclusive.y)
    {
        return;
    }

    const ImVec2 minimum(canvasOrigin.x + static_cast<float>(state.panX + (static_cast<double>(selection->min.x) * state.zoom)),
                         canvasOrigin.y +
                             static_cast<float>(state.panY + (static_cast<double>(selection->min.y) * state.zoom)));
    const ImVec2 maximum(
        canvasOrigin.x + static_cast<float>(state.panX + (static_cast<double>(selection->maxExclusive.x) * state.zoom)),
        canvasOrigin.y + static_cast<float>(state.panY + (static_cast<double>(selection->maxExclusive.y) * state.zoom)));

    drawList.AddRect(minimum,
                     maximum,
                     theme::packedColor(theme::brass),
                     0.0F,
                     ImDrawFlags_None,
                     static_cast<float>(selectionOutlineThickness));
}

void writeLodPixel(std::vector<std::uint8_t> & pixels, std::size_t pixelIndex, ImU32 color)
{
    const std::size_t offset = pixelIndex * lodPixelChannels;
    pixels.at(offset) = static_cast<std::uint8_t>((color >> IM_COL32_R_SHIFT) & colorChannelMask);
    pixels.at(offset + 1U) = static_cast<std::uint8_t>((color >> IM_COL32_G_SHIFT) & colorChannelMask);
    pixels.at(offset + 2U) = static_cast<std::uint8_t>((color >> IM_COL32_B_SHIFT) & colorChannelMask);
    pixels.at(offset + 3U) = static_cast<std::uint8_t>(colorChannelMaximum);
}

void rasterizeLodRow(const MapWorkspace & workspace,
                     RadarcolPanelState & radarcolState,
                     MapCanvasPanelState & state,
                     std::vector<std::uint8_t> & pixels,
                     std::uint32_t rasterWidth,
                     std::uint32_t pixelY,
                     double scaleX,
                     double scaleY,
                     MapDisplayMode mode)
{
    const uomul::Dimensions dimensions = workspace.dimensions();
    const double screenY = (static_cast<double>(pixelY) + 0.5) / (scaleY * lodRasterScale);
    const double mapY = (screenY - state.panY) / state.zoom;

    if (mapY < 0.0 || mapY >= static_cast<double>(dimensions.heightTiles))
    {
        return;
    }

    const auto tileY = static_cast<std::uint32_t>(std::floor(mapY));
    const std::uint32_t blockY = tileY / uomul::mapBlockDimension;
    const std::uint32_t localY = tileY % uomul::mapBlockDimension;
    std::uint32_t loadedBlockX = std::numeric_limits<std::uint32_t>::max();
    const MapCanvasCachedBlock * loadedBlock = nullptr;

    for (std::uint32_t pixelX = 0; pixelX < rasterWidth; ++pixelX)
    {
        const double screenX = (static_cast<double>(pixelX) + 0.5) / (scaleX * lodRasterScale);
        const double mapX = (screenX - state.panX) / state.zoom;

        if (mapX < 0.0 || mapX >= static_cast<double>(dimensions.widthTiles))
        {
            continue;
        }

        const auto tileX = static_cast<std::uint32_t>(std::floor(mapX));
        const std::uint32_t blockX = tileX / uomul::mapBlockDimension;

        if (blockX != loadedBlockX)
        {
            loadedBlock = &cachedBlock(workspace, state, {.x = blockX, .y = blockY});
            loadedBlockX = blockX;
        }

        ImU32 color = unavailableColor(tileX, tileY);

        if (loadedBlock->block)
        {
            const std::uint32_t localIndex = (localY * uomul::mapBlockDimension) + (tileX % uomul::mapBlockDimension);
            color = colorForTile(mode, loadedBlock->block->at(static_cast<std::size_t>(localIndex)), radarcolState);
        }
        else if (loadedBlock->error && !state.lodReadError)
        {
            state.lodReadError = loadedBlock->error;
        }

        writeLodPixel(pixels, (static_cast<std::size_t>(pixelY) * rasterWidth) + pixelX, color);
    }
}

void rasterizeLod(const MapWorkspace & workspace,
                  RadarcolPanelState & radarcolState,
                  MapCanvasPanelState & state,
                  std::uint32_t rasterWidth,
                  std::uint32_t rasterHeight,
                  double scaleX,
                  double scaleY,
                  MapDisplayMode mode)
{
    const std::size_t pixelCount = static_cast<std::size_t>(rasterWidth) * rasterHeight;
    state.lodPixels.resize(pixelCount * lodPixelChannels);
    const ImU32 backgroundColor = theme::packedColor(theme::background);

    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel)
    {
        writeLodPixel(state.lodPixels, pixel, backgroundColor);
    }

    state.lodReadError.reset();

    for (std::uint32_t pixelY = 0; pixelY < rasterHeight; ++pixelY)
    {
        rasterizeLodRow(workspace, radarcolState, state, state.lodPixels, rasterWidth, pixelY, scaleX, scaleY, mode);
    }
}

void uploadLodTexture(MapCanvasPanelState & state, std::uint32_t rasterWidth, std::uint32_t rasterHeight)
{
    if (state.lodTexture == 0U)
    {
        GLuint texture = 0;
        glGenTextures(1, &texture);
        state.lodTexture = texture;
    }

    if (state.lodTexture == 0U)
    {
        state.lodCacheValid = false;
        return;
    }

    GLint previousTexture = 0;
    GLint previousUnpackAlignment = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
    glBindTexture(GL_TEXTURE_2D, state.lodTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (state.lodTextureWidth != rasterWidth || state.lodTextureHeight != rasterHeight)
    {
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA,
                     static_cast<GLsizei>(rasterWidth),
                     static_cast<GLsizei>(rasterHeight),
                     0,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     state.lodPixels.data());
        state.lodTextureWidth = rasterWidth;
        state.lodTextureHeight = rasterHeight;
    }
    else
    {
        glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        0,
                        0,
                        static_cast<GLsizei>(rasterWidth),
                        static_cast<GLsizei>(rasterHeight),
                        GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        state.lodPixels.data());
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
}

[[nodiscard]] bool lodCacheMatches(const MapCanvasPanelState & state,
                                   const RadarcolPanelState & radarcolState,
                                   std::uint32_t rasterWidth,
                                   std::uint32_t rasterHeight,
                                   ImVec2 canvasSize,
                                   ImVec2 framebufferScale,
                                   MapDisplayMode mode)
{
    const auto nearlyEqual = [](float left, float right) { return std::abs(left - right) < floatCacheTolerance; };
    return state.lodCacheValid && state.lodTextureWidth == rasterWidth && state.lodTextureHeight == rasterHeight &&
           nearlyEqual(state.lodCanvasWidth, canvasSize.x) && nearlyEqual(state.lodCanvasHeight, canvasSize.y) &&
           nearlyEqual(state.lodScaleX, framebufferScale.x) && nearlyEqual(state.lodScaleY, framebufferScale.y) &&
           std::abs(state.lodPanX - state.panX) < floatCacheTolerance &&
           std::abs(state.lodPanY - state.panY) < floatCacheTolerance &&
           std::abs(state.lodZoom - state.zoom) < floatCacheTolerance && state.lodDisplayMode == mode &&
           state.lodRadarColorRevision == radarcolState.colorRevision;
}

void saveLodCacheKey(MapCanvasPanelState & state,
                     const RadarcolPanelState & radarcolState,
                     std::uint32_t rasterWidth,
                     std::uint32_t rasterHeight,
                     ImVec2 canvasSize,
                     ImVec2 scale)
{
    state.lodTextureWidth = rasterWidth;
    state.lodTextureHeight = rasterHeight;
    state.lodCanvasWidth = canvasSize.x;
    state.lodCanvasHeight = canvasSize.y;
    state.lodScaleX = scale.x;
    state.lodScaleY = scale.y;
    state.lodPanX = state.panX;
    state.lodPanY = state.panY;
    state.lodZoom = state.zoom;
    state.lodRadarColorRevision = radarcolState.colorRevision;
    state.lodCacheValid = true;
}

[[nodiscard]] bool drawScreenSpaceLod(const MapWorkspace & workspace,
                                      RadarcolPanelState & radarcolState,
                                      MapCanvasPanelState & state,
                                      ImDrawList & drawList,
                                      ImVec2 canvasOrigin,
                                      ImVec2 canvasSize,
                                      std::optional<uomul::Error> & visibleReadError)
{
    const ImVec2 framebufferScale = ImGui::GetIO().DisplayFramebufferScale;
    const double scaleX = std::max(static_cast<double>(framebufferScale.x), std::numeric_limits<double>::epsilon());
    const double scaleY = std::max(static_cast<double>(framebufferScale.y), std::numeric_limits<double>::epsilon());
    const double width = std::ceil(static_cast<double>(canvasSize.x) * scaleX * lodRasterScale);
    const double height = std::ceil(static_cast<double>(canvasSize.y) * scaleY * lodRasterScale);

    if (width <= 0.0 || height <= 0.0 || width > std::numeric_limits<GLsizei>::max() ||
        height > std::numeric_limits<GLsizei>::max())
    {
        return false;
    }

    const auto rasterWidth = static_cast<std::uint32_t>(width);
    const auto rasterHeight = static_cast<std::uint32_t>(height);
    const std::uint64_t pixelCount = static_cast<std::uint64_t>(rasterWidth) * rasterHeight;

    if (pixelCount > std::numeric_limits<std::size_t>::max() / lodPixelChannels)
    {
        return false;
    }

    const MapDisplayMode mode = workspace.displayMode();
    if (!lodCacheMatches(state, radarcolState, rasterWidth, rasterHeight, canvasSize, framebufferScale, mode))
    {
        rasterizeLod(workspace, radarcolState, state, rasterWidth, rasterHeight, scaleX, scaleY, mode);
        uploadLodTexture(state, rasterWidth, rasterHeight);

        if (state.lodTexture == 0U)
        {
            return false;
        }

        saveLodCacheKey(state, radarcolState, rasterWidth, rasterHeight, canvasSize, framebufferScale);
        state.lodDisplayMode = mode;
    }

    visibleReadError = state.lodReadError;
    drawList.AddImage(static_cast<ImTextureID>(state.lodTexture),
                      canvasOrigin,
                      ImVec2(canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y),
                      ImVec2(0.0F, 1.0F),
                      ImVec2(1.0F, 0.0F));

    return true;
}

void drawCanvasContents(const MapWorkspace & workspace,
                        RadarcolPanelState & radarcolState,
                        MapCanvasPanelState & state,
                        ImVec2 canvasOrigin,
                        ImVec2 canvasSize,
                        std::optional<uomul::TileCoord> currentTile,
                        std::optional<uomul::Error> & visibleReadError)
{
    ImDrawList * drawList = ImGui::GetWindowDrawList();
    const ImVec2 canvasMaximum(canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y);
    drawList->AddRectFilled(canvasOrigin, canvasMaximum, theme::packedColor(theme::background));
    drawList->PushClipRect(canvasOrigin, canvasMaximum, true);

    const uomul::Dimensions dimensions = workspace.dimensions();
    const double tileSize = state.zoom;
    const std::uint32_t minX = visibleStart(state.panX, tileSize, dimensions.widthTiles);
    const std::uint32_t minY = visibleStart(state.panY, tileSize, dimensions.heightTiles);
    const std::uint32_t maxX = visibleEnd(canvasSize.x, state.panX, tileSize, dimensions.widthTiles);
    const std::uint32_t maxY = visibleEnd(canvasSize.y, state.panY, tileSize, dimensions.heightTiles);
    const MapDisplayMode mode = workspace.displayMode();
    const bool useScreenSpaceLod =
        tileSize * std::min(ImGui::GetIO().DisplayFramebufferScale.x, ImGui::GetIO().DisplayFramebufferScale.y) <
        lodZoomThreshold;

    if (!useScreenSpaceLod ||
        !drawScreenSpaceLod(workspace, radarcolState, state, *drawList, canvasOrigin, canvasSize, visibleReadError))
    {
        state.lodCacheValid = false;
        state.lodReadError.reset();

        for (std::uint64_t tileY = minY; tileY < maxY; ++tileY)
        {
            const std::uint32_t tileRow = static_cast<std::uint32_t>(tileY);
            const std::uint32_t blockY = tileRow / uomul::mapBlockDimension;
            const std::uint32_t localY = tileRow % uomul::mapBlockDimension;
            std::uint32_t cachedBlockX = std::numeric_limits<std::uint32_t>::max();
            const MapCanvasCachedBlock * cached = nullptr;

            for (std::uint64_t tileX = minX; tileX < maxX; ++tileX)
            {
                const std::uint32_t tileColumn = static_cast<std::uint32_t>(tileX);
                const std::uint32_t blockX = tileColumn / uomul::mapBlockDimension;

                if (blockX != cachedBlockX)
                {
                    cached = &cachedBlock(workspace, state, {.x = blockX, .y = blockY});
                    cachedBlockX = blockX;
                }

                ImU32 color = unavailableColor(tileColumn, tileRow);

                if (cached->block)
                {
                    const std::uint32_t localIndex =
                        (localY * uomul::mapBlockDimension) + (tileColumn % uomul::mapBlockDimension);
                    color = colorForTile(mode, cached->block->at(static_cast<std::size_t>(localIndex)), radarcolState);
                }
                else if (cached->error && !visibleReadError)
                {
                    visibleReadError = cached->error;
                }

                const ImVec2 minimum(canvasOrigin.x + static_cast<float>(state.panX + (static_cast<double>(tileX) * tileSize)),
                                     canvasOrigin.y + static_cast<float>(state.panY + (static_cast<double>(tileY) * tileSize)));
                const ImVec2 maximum(minimum.x + static_cast<float>(tileSize), minimum.y + static_cast<float>(tileSize));
                drawList->AddRectFilled(minimum, maximum, color);
            }
        }
    }

    drawSelection(*drawList, canvasOrigin, state, workspace, dimensions);

    if (!currentTile && workspace.selectedTile())
    {
        const uomul::TileCoord tile = *workspace.selectedTile();
        const ImVec2 minimum(canvasOrigin.x + static_cast<float>(state.panX + (static_cast<double>(tile.x) * tileSize)),
                             canvasOrigin.y + static_cast<float>(state.panY + (static_cast<double>(tile.y) * tileSize)));
        const ImVec2 maximum(minimum.x + static_cast<float>(tileSize), minimum.y + static_cast<float>(tileSize));
        drawList->AddRect(minimum, maximum, theme::packedColor(theme::brass), 0.0F, ImDrawFlags_None, 1.0F);
    }

    if (currentTile)
    {
        const uomul::TileCoord tile = *currentTile;
        const ImVec2 minimum(canvasOrigin.x + static_cast<float>(state.panX + (static_cast<double>(tile.x) * tileSize)),
                             canvasOrigin.y + static_cast<float>(state.panY + (static_cast<double>(tile.y) * tileSize)));
        const ImVec2 maximum(minimum.x + static_cast<float>(tileSize), minimum.y + static_cast<float>(tileSize));
        drawList->AddRect(minimum, maximum, theme::packedColor(theme::parchmentText), 0.0F, ImDrawFlags_None, 1.0F);
    }

    const uomul::MapReader * reader = workspace.reader();

    if (reader != nullptr && reader->missingBlocks() > 0U)
    {
        const std::string missingMessage = "Incomplete map: " + std::to_string(reader->missingBlocks()) + " blocks missing";
        drawList->AddText(ImVec2(canvasOrigin.x + static_cast<float>(errorTextLeftPadding),
                                 canvasOrigin.y + static_cast<float>(errorTextTopPadding)),
                          theme::packedColor(theme::amber),
                          missingMessage.c_str());
    }

    if (visibleReadError)
    {
        const std::string errorMessage = "Map read error: " + visibleReadError->message;
        drawList->AddText(ImVec2(canvasOrigin.x + static_cast<float>(errorTextLeftPadding),
                                 canvasOrigin.y + static_cast<float>(readErrorTextOffset)),
                          theme::packedColor(theme::coral),
                          errorMessage.c_str());
    }
    else if (workspace.error())
    {
        const std::string errorMessage = "Map error: " + workspace.error()->message;
        drawList->AddText(ImVec2(canvasOrigin.x + static_cast<float>(errorTextLeftPadding),
                                 canvasOrigin.y + static_cast<float>(errorTextTopPadding)),
                          theme::packedColor(theme::coral),
                          errorMessage.c_str());
    }

    drawList->PopClipRect();
}

void drawLayerButton(MapWorkspace & workspace, MapDisplayMode mode, const char * label)
{
    const bool selected = workspace.displayMode() == mode;

    if (selected)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }

    if (ImGui::Button(label))
    {
        workspace.setDisplayMode(mode);
    }

    if (selected)
    {
        ImGui::PopStyleColor();
    }
}

void drawLayerControls(MapWorkspace & workspace, const MapCanvasPanelState & state)
{
    drawLayerButton(workspace, MapDisplayMode::tileIds, "Tile IDs");
    ImGui::SameLine();
    drawLayerButton(workspace, MapDisplayMode::z, "Z");
    ImGui::SameLine();
    drawLayerButton(workspace, MapDisplayMode::combined, "Combined");

    if (workspace.editMode())
    {
        ImGui::SameLine();
        ImGui::TextColored(theme::imguiColor(theme::sageGreen), "EDIT MODE");
    }

    ImGui::SameLine();
    ImGui::Text("Zoom %.3gx", state.zoom);
}

void resetForDocument(const MapWorkspace & workspace, MapCanvasPanelState & state, uomul::Dimensions dimensions)
{
    const bool hasDocument = workspace.reader() != nullptr;
    const std::filesystem::path & currentPath = workspace.mapPath();

    if (state.hasDocument == hasDocument && state.documentPath == currentPath && state.documentDimensions == dimensions &&
        state.workingPath == workspace.workingPath())
    {
        return;
    }

    state.blockCache.clear();
    state.blockCacheIndex.clear();
    state.cacheClock = 0;
    state.lodCacheValid = false;
    state.lodReadError.reset();
    state.panX = 0.0;
    state.panY = 0.0;
    state.dragStart.reset();
    state.dragEnd.reset();
    state.panning = false;
    state.documentPath = currentPath;
    state.workingPath = workspace.workingPath();
    state.documentDimensions = dimensions;
    state.hasDocument = hasDocument;
}

void updateHover(MapWorkspace & workspace, std::optional<uomul::TileCoord> tile)
{
    if (!tile)
    {
        workspace.clearHover();
        return;
    }

    if (!workspace.hoveredTile() || workspace.hoveredTile()->x != tile->x || workspace.hoveredTile()->y != tile->y ||
        !workspace.hoveredValue())
    {
        const auto result = workspace.updateHover(*tile);
        static_cast<void>(result);
    }
}

void updateNavigation(MapCanvasPanelState & state, ImVec2 mouse, ImVec2 canvasOrigin, bool canvasHovered)
{
    if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
    {
        state.panning = true;
        state.panStartMouseX = mouse.x;
        state.panStartMouseY = mouse.y;
        state.panStartX = state.panX;
        state.panStartY = state.panY;
    }

    if (state.panning && ImGui::IsMouseDown(ImGuiMouseButton_Middle))
    {
        state.panX = std::clamp(state.panStartX + mouse.x - state.panStartMouseX, -maximumPanMagnitude, maximumPanMagnitude);
        state.panY = std::clamp(state.panStartY + mouse.y - state.panStartMouseY, -maximumPanMagnitude, maximumPanMagnitude);
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle))
    {
        state.panning = false;
    }

    if (!canvasHovered || std::abs(ImGui::GetIO().MouseWheel) <= std::numeric_limits<float>::epsilon())
    {
        return;
    }

    const double oldZoom = state.zoom;
    const double newZoom =
        std::clamp(oldZoom * std::pow(canvasZoomFactor, ImGui::GetIO().MouseWheel), minimumCanvasZoom, maximumCanvasZoom);
    const double mapX = (static_cast<double>(mouse.x) - canvasOrigin.x - state.panX) / oldZoom;
    const double mapY = (static_cast<double>(mouse.y) - canvasOrigin.y - state.panY) / oldZoom;
    state.panX = static_cast<double>(mouse.x) - canvasOrigin.x - (mapX * newZoom);
    state.panY = static_cast<double>(mouse.y) - canvasOrigin.y - (mapY * newZoom);
    state.zoom = newZoom;
}

void updateSelectionDrag(MapWorkspace & workspace,
                         MapCanvasPanelState & state,
                         std::optional<uomul::TileCoord> currentTile,
                         uomul::Dimensions dimensions,
                         bool canvasHovered)
{
    if (canvasHovered && currentTile && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        state.dragStart = currentTile;
        state.dragEnd = currentTile;
    }

    if (state.dragStart && ImGui::IsMouseDown(ImGuiMouseButton_Left) && currentTile)
    {
        state.dragEnd = currentTile;
    }

    if (!state.dragStart || !ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        return;
    }

    if (state.dragEnd)
    {
        if (workspace.selectionMode() == MapSelectionMode::tiles)
        {
            workspace.setTileSelection(tileSelectionFromDrag(*state.dragStart, *state.dragEnd, dimensions));
        }
        else
        {
            workspace.setBlockSelection(blockSelectionFromDrag(*state.dragStart, *state.dragEnd, dimensions));
        }

        const uomul::TileCoord inspectorTile = selectionInspectorTile(workspace.selectionMode(), *state.dragEnd);
        const auto result = workspace.updateSelectedTile(inspectorTile);
        static_cast<void>(result);
    }

    state.dragStart.reset();
    state.dragEnd.reset();
}

void drawEmptyCanvas(ImVec2 canvasOrigin, ImVec2 canvasMaximum, const MapWorkspace & workspace)
{
    ImDrawList * drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(canvasOrigin, canvasMaximum, theme::packedColor(theme::background));
    drawList->AddText(
        ImVec2(canvasOrigin.x + static_cast<float>(noMapTextPadding), canvasOrigin.y + static_cast<float>(noMapTextPadding)),
        theme::packedColor(theme::parchmentText),
        "No map open");

    if (workspace.error())
    {
        const std::string errorMessage = "Map error: " + workspace.error()->message;
        drawList->AddText(ImVec2(canvasOrigin.x + static_cast<float>(errorTextLeftPadding),
                                 canvasOrigin.y + static_cast<float>(readErrorTextOffset)),
                          theme::packedColor(theme::coral),
                          errorMessage.c_str());
    }
}

} // namespace

MapCanvasPanelState::~MapCanvasPanelState()
{
    if (lodTexture != 0U)
    {
        const GLuint texture = lodTexture;
        glDeleteTextures(1, &texture);
    }
}

void drawMapCanvasPanel(MapWorkspace & workspace,
                        RadarcolPanelState & radarcolState,
                        MapCanvasPanelState & state,
                        bool & visible)
{
    state.panX = std::isfinite(state.panX) ? state.panX : 0.0;
    state.panY = std::isfinite(state.panY) ? state.panY : 0.0;
    state.zoom = std::isfinite(state.zoom) ? state.zoom : defaultMapCanvasZoom;
    state.panX = std::clamp(state.panX, -maximumPanMagnitude, maximumPanMagnitude);
    state.panY = std::clamp(state.panY, -maximumPanMagnitude, maximumPanMagnitude);
    state.zoom = std::clamp(state.zoom, minimumCanvasZoom, maximumCanvasZoom);

    const uomul::Dimensions dimensions = workspace.dimensions();
    const bool hasDocument = workspace.reader() != nullptr;
    resetForDocument(workspace, state, dimensions);

    const bool contentsVisible = ImGui::Begin("Map Canvas", &visible);

    if (!visible || !contentsVisible)
    {
        workspace.clearHover();
        ImGui::End();
        return;
    }

    drawLayerControls(workspace, state);
    const float statusHeight = ImGui::GetFrameHeightWithSpacing();
    const bool canvasContentsVisible = ImGui::BeginChild("MapCanvasViewport",
                                                         ImVec2(0.0F, -statusHeight),
                                                         ImGuiChildFlags_Borders,
                                                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const ImVec2 canvasOrigin = ImGui::GetCursorScreenPos();
    const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    const ImVec2 canvasMaximum(canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y);
    const bool canvasHovered = canvasContentsVisible && ImGui::IsWindowHovered() &&
                               ImGui::IsMouseHoveringRect(canvasOrigin, canvasMaximum, false) &&
                               !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) && !ImGui::GetIO().WantTextInput;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    std::optional<uomul::TileCoord> currentTile;
    std::optional<uomul::Error> visibleReadError;

    if (hasDocument && canvasContentsVisible)
    {
        updateNavigation(state, mouse, canvasOrigin, canvasHovered);
        currentTile = tileAtMouse(mouse, canvasOrigin, canvasSize, state, dimensions, canvasHovered);
        updateHover(workspace, currentTile);
        updateSelectionDrag(workspace, state, currentTile, dimensions, canvasHovered);
    }
    else
    {
        workspace.clearHover();
    }

    if (canvasContentsVisible)
    {
        if (!hasDocument)
        {
            drawEmptyCanvas(canvasOrigin, canvasMaximum, workspace);
        }
        else
        {
            drawCanvasContents(workspace, radarcolState, state, canvasOrigin, canvasSize, currentTile, visibleReadError);
        }
    }

    ImGui::EndChild();

    ImGui::Separator();

    if (!hasDocument || !canvasHovered || !currentTile)
    {
        ImGui::TextUnformatted("No tile under pointer");
    }
    else if (workspace.selectionMode() == MapSelectionMode::tiles)
    {
        ImGui::Text("Tile (%u, %u)", currentTile->x, currentTile->y);
    }
    else
    {
        const uomul::BlockCoord block = tileToBlock(*currentTile);
        ImGui::Text("Block (%u, %u)", block.x, block.y);
    }

    ImGui::End();
}

} // namespace uo_mule::gui
