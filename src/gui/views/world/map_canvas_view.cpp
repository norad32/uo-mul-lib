#include "gui/views/world/map_canvas_view.h"
#include "gui/presentation/theme/theme.h"
#include "gui_model/input/coordinate_parser.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/providers/data_providers.h"
#include "gui_model/tools/tool_contracts.h"
#include "gui_model/tools/tool_state.h"
#include "gui_model/workspaces/map_workspace.h"
#include "gui_model/workspaces/map_workspace_manager.h"
#include "gui_model/world/map_canvas_state.h"
#include "gui_model/world/map_color.h"
#include "gui_model/world/map_selection.h"
#include "uomul/map/map.h"
#include "uomul/mul_common/mul_common.h"
#include <GL/gl.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <imgui.h>
#include <iterator>
#include <limits>
#include <optional>
#include <ranges>
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
constexpr double coordinateCenterDivisor = 2.0;
constexpr float coordinateDialogPivot = 0.5F;
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
constexpr double noMapButtonOffset = 28.0;
constexpr float marchingDashShortLineFactor = 1.5F;
constexpr double tileCenterOffset = 0.5;
constexpr float lassoLineSpacing = 4.0F;
constexpr float selectionLineSpacing = 4.0F;
constexpr float blockSelectionLineThickness = 2.0F;
constexpr float blockSelectionLineSpacing = 8.0F;

[[nodiscard]] float canvasContentExtent(std::uint32_t tileCount, double zoom, float viewportExtent) noexcept
{
    const double extent = static_cast<double>(tileCount) * zoom;
    return std::max(viewportExtent,
                    static_cast<float>(std::min(extent, static_cast<double>(std::numeric_limits<float>::max()))));
}

[[nodiscard]] double minimumZoomForCanvas(uomul::Dimensions dimensions, ImVec2 canvasSize) noexcept
{
    const double widthZoom = static_cast<double>(canvasSize.x) / static_cast<double>(dimensions.widthTiles);
    const double heightZoom = static_cast<double>(canvasSize.y) / static_cast<double>(dimensions.heightTiles);
    return std::clamp(std::min(widthZoom, heightZoom), std::numeric_limits<double>::epsilon(), maximumCanvasZoom);
}

[[nodiscard]] double fitZoomForCanvas(uomul::Dimensions dimensions, ImVec2 canvasSize) noexcept
{
    const double widthZoom = static_cast<double>(canvasSize.x) / static_cast<double>(dimensions.widthTiles);
    const double heightZoom = static_cast<double>(canvasSize.y) / static_cast<double>(dimensions.heightTiles);
    return std::clamp(std::min(widthZoom, heightZoom), minimumCanvasZoom, maximumCanvasZoom);
}

[[nodiscard]] ImU32 packedMapColor(MapColor color) noexcept
{
    return IM_COL32(color.red, color.green, color.blue, std::numeric_limits<std::uint8_t>::max());
}

[[nodiscard]] ImU32 textureTint(MapDisplayMode mode, uomul::MapTile tile) noexcept
{
    if (mode != MapDisplayMode::combined)
    {
        return IM_COL32(255, 255, 255, 255);
    }

    constexpr double combinedBrightnessMinimum = 0.5;
    constexpr double combinedBrightnessRange = 1.0;
    const double brightness =
        combinedBrightnessMinimum + ((static_cast<double>(zGrayscale(tile.z)) / 255.0) * combinedBrightnessRange);
    const auto channel = static_cast<std::uint8_t>(std::clamp(static_cast<int>(brightness * 255.0), 0, 255));
    return IM_COL32(channel, channel, channel, 255);
}

[[nodiscard]] MapCanvasCachedTexture &
cachedTexture(MapCanvasViewState & state, const TextureProvider & textures, std::uint32_t id)
{
    const auto existing = state.textureCache.find(id);
    if (existing != state.textureCache.end())
    {
        return existing->second;
    }

    MapCanvasCachedTexture entry;
    if (const auto texture = textures.texture(id);
        texture && texture->width != 0U && texture->height != 0U && !texture->pixels.empty())
    {
        entry.width = texture->width;
        entry.height = texture->height;
        entry.pixels = texture->pixels;
        entry.rgbaPixels.reserve(entry.pixels.size() * 4U);
        for (const MapColor pixel : entry.pixels)
        {
            entry.rgbaPixels.push_back(pixel.red);
            entry.rgbaPixels.push_back(pixel.green);
            entry.rgbaPixels.push_back(pixel.blue);
            entry.rgbaPixels.push_back(std::numeric_limits<std::uint8_t>::max());
        }
    }

    return state.textureCache.emplace(id, std::move(entry)).first->second;
}

void uploadTileTexture(MapCanvasCachedTexture & texture)
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

[[nodiscard]] std::optional<MapColor>
texturePixelColor(const MapCanvasCachedTexture & texture, double localX, double localY) noexcept
{
    if (texture.pixels.empty() || texture.width == 0U || texture.height == 0U)
    {
        return std::nullopt;
    }
    const auto pixelX =
        std::min(texture.width - 1U, static_cast<std::uint32_t>(std::max(0.0, std::floor(localX * texture.width))));
    const auto pixelY =
        std::min(texture.height - 1U, static_cast<std::uint32_t>(std::max(0.0, std::floor(localY * texture.height))));
    const std::size_t index = (static_cast<std::size_t>(pixelY) * texture.width) + pixelX;
    return index < texture.pixels.size() ? std::optional<MapColor>(texture.pixels.at(index)) : std::nullopt;
}

[[nodiscard]] std::optional<MapColor> selectedTexturePixel(const MapCanvasCachedTexture & texture,
                                                           MapViewTileSource tileSource,
                                                           double localX,
                                                           double localY) noexcept
{
    return tileSource == MapViewTileSource::textures ? texturePixelColor(texture, localX, localY) : std::nullopt;
}

void clearTextureCache(MapCanvasViewState & state)
{
    if (state.releaseTexture)
    {
        for (auto & [id, texture] : state.textureCache)
        {
            static_cast<void>(id);
            if (texture.gpuTexture != 0U)
            {
                state.releaseTexture(texture.gpuTexture);
            }
        }
    }
    state.textureCache.clear();
}

[[nodiscard]] ImU32 unavailableColor(std::uint32_t tileX, std::uint32_t tileY) noexcept
{
    const bool alternate = ((tileX / uomul::mapBlockDimension) + (tileY / uomul::mapBlockDimension)) % 2U == 0U;
    return theme::packedColor(alternate ? theme::unavailable : theme::error);
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

[[nodiscard]] ImVec2
tileScreenPosition(ImVec2 canvasOrigin, const MapCanvasViewState & state, std::uint32_t tileX, std::uint32_t tileY) noexcept
{
    return {canvasOrigin.x + static_cast<float>(state.panX + (static_cast<double>(tileX) * state.zoom)),
            canvasOrigin.y + static_cast<float>(state.panY + (static_cast<double>(tileY) * state.zoom))};
}

struct ScreenRect
{
    ImVec2 minimum;
    ImVec2 maximum;
};

[[nodiscard]] ScreenRect
tileScreenRect(ImVec2 canvasOrigin, const MapCanvasViewState & state, std::uint32_t tileX, std::uint32_t tileY) noexcept
{
    const ImVec2 minimum = tileScreenPosition(canvasOrigin, state, tileX, tileY);
    return {
        .minimum = minimum,
        .maximum = ImVec2(minimum.x + static_cast<float>(state.zoom), minimum.y + static_cast<float>(state.zoom)),
    };
}

void drawCanvasTile(ImDrawList & drawList,
                    MapCanvasViewState & state,
                    const TextureProvider & textures,
                    const RadarColorProvider & radarColors,
                    MapViewTileSource tileSource,
                    MapDisplayMode mode,
                    uomul::MapTile tile,
                    ScreenRect screenRect)
{
    if (mode != MapDisplayMode::z && tileSource == MapViewTileSource::textures)
    {
        MapCanvasCachedTexture & texture = cachedTexture(state, textures, tile.id);
        if (!texture.pixels.empty())
        {
            if (texture.gpuTexture == 0U)
            {
                uploadTileTexture(texture);
            }
            if (texture.gpuTexture != 0U)
            {
                drawList.AddImage(static_cast<ImTextureID>(texture.gpuTexture),
                                  screenRect.minimum,
                                  screenRect.maximum,
                                  ImVec2(0.0F, 0.0F),
                                  ImVec2(1.0F, 1.0F),
                                  textureTint(mode, tile));
                return;
            }
        }
    }

    drawList.AddRectFilled(
        screenRect.minimum, screenRect.maximum, packedMapColor(tileDisplayColor(mode, tile, radarColors.mapColor(tile.id))));
}

[[nodiscard]] bool hasPendingNavigation(const MapCanvasViewState & state) noexcept
{
    return state.navigationRequest.request != MapCanvasViewRequest::none || state.navigationRequest.coordinate.has_value() ||
           state.scrollResetPending;
}

[[nodiscard]] const MapCanvasCachedBlock &
cachedBlock(const MapWorkspace & workspace, MapCanvasViewState & state, uomul::BlockCoord coordinate)
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
                state.cacheClock++;
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
                                                          const MapCanvasViewState & state,
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

struct BoundaryEdge
{
    std::int32_t x1;
    std::int32_t y1;
    std::int32_t x2;
    std::int32_t y2;
};

void drawMarchingLine(ImDrawList & drawList, double marchingDistance, ImVec2 start, ImVec2 end, float thickness, float spacing)
{
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float length = std::sqrt((dx * dx) + (dy * dy));
    if (length <= 0.0F)
    {
        return;
    }

    const float unitX = dx / length;
    const float unitY = dy / length;
    const float cycle = spacing * 2.0F;
    if (length < cycle * marchingDashShortLineFactor)
    {
        drawList.AddLine(start, end, theme::packedColor(theme::selection), thickness);
        return;
    }

    const float phaseCoordinate = std::abs(dy) > std::abs(dx) ? std::min(start.y, end.y) : std::min(start.x, end.x);
    const float phase =
        static_cast<float>(std::fmod(marchingDistance + static_cast<double>(phaseCoordinate), static_cast<double>(cycle)));
    for (std::size_t dashIndex = 0U;; dashIndex++)
    {
        const float offset = -phase + (static_cast<float>(dashIndex) * cycle);
        if (offset >= length)
        {
            break;
        }
        const float dashStart = std::max(0.0F, offset);
        const float dashEnd = std::min(length, offset + spacing);
        if (dashEnd <= dashStart)
        {
            continue;
        }
        drawList.AddLine(ImVec2(start.x + (unitX * dashStart), start.y + (unitY * dashStart)),
                         ImVec2(start.x + (unitX * dashEnd), start.y + (unitY * dashEnd)),
                         theme::packedColor(theme::selection),
                         thickness);
    }
}

void drawMarchingRect(ImDrawList & drawList,
                      double marchingDistance,
                      ImVec2 minimum,
                      ImVec2 maximum,
                      float thickness,
                      float spacing)
{
    drawMarchingLine(drawList, marchingDistance, minimum, ImVec2(maximum.x, minimum.y), thickness, spacing);
    drawMarchingLine(drawList, marchingDistance, ImVec2(maximum.x, minimum.y), maximum, thickness, spacing);
    drawMarchingLine(drawList, marchingDistance, maximum, ImVec2(minimum.x, maximum.y), thickness, spacing);
    drawMarchingLine(drawList, marchingDistance, ImVec2(minimum.x, maximum.y), minimum, thickness, spacing);
}

void drawMarchingPolyline(ImDrawList & drawList,
                          double marchingDistance,
                          const std::vector<ImVec2> & points,
                          float thickness,
                          float spacing)
{
    if (points.size() < 2U)
    {
        return;
    }

    const float cycle = spacing * 2.0F;
    const float phase = static_cast<float>(std::fmod(marchingDistance, static_cast<double>(cycle)));
    float totalLength = 0.0F;
    for (std::size_t index = 1U; index < points.size(); index++)
    {
        const float dx = points.at(index).x - points.at(index - 1U).x;
        const float dy = points.at(index).y - points.at(index - 1U).y;
        totalLength += std::sqrt((dx * dx) + (dy * dy));
    }
    if (totalLength <= 0.0F)
    {
        return;
    }

    for (std::size_t dashIndex = 0U;; dashIndex++)
    {
        const float dashOffset = -phase + (static_cast<float>(dashIndex) * cycle);
        if (dashOffset >= totalLength)
        {
            break;
        }
        const float dashStart = std::max(0.0F, dashOffset);
        const float dashEnd = std::min(totalLength, dashOffset + spacing);
        if (dashEnd <= dashStart)
        {
            continue;
        }

        float segmentStart = 0.0F;
        for (std::size_t index = 1U; index < points.size(); index++)
        {
            const ImVec2 start = points.at(index - 1U);
            const ImVec2 end = points.at(index);
            const float dx = end.x - start.x;
            const float dy = end.y - start.y;
            const float segmentLength = std::sqrt((dx * dx) + (dy * dy));
            const float segmentEnd = segmentStart + segmentLength;
            const float visibleStart = std::max(dashStart, segmentStart);
            const float visibleEnd = std::min(dashEnd, segmentEnd);
            if (visibleEnd > visibleStart && segmentLength > 0.0F)
            {
                const float startRatio = (visibleStart - segmentStart) / segmentLength;
                const float endRatio = (visibleEnd - segmentStart) / segmentLength;
                drawList.AddLine(ImVec2(start.x + (dx * startRatio), start.y + (dy * startRatio)),
                                 ImVec2(start.x + (dx * endRatio), start.y + (dy * endRatio)),
                                 theme::packedColor(theme::selection),
                                 thickness);
            }
            segmentStart = segmentEnd;
        }
    }
}

void appendBoundaryEdge(std::vector<BoundaryEdge> & edges, std::int32_t x1, std::int32_t y1, std::int32_t x2, std::int32_t y2)
{
    if (x1 == x2 && y1 > y2)
    {
        std::swap(y1, y2);
    }
    else if (y1 == y2 && x1 > x2)
    {
        std::swap(x1, x2);
    }
    edges.push_back({.x1 = x1, .y1 = y1, .x2 = x2, .y2 = y2});
}

[[nodiscard]] bool boundaryEdgeLess(const BoundaryEdge & left, const BoundaryEdge & right) noexcept
{
    const bool leftHorizontal = left.y1 == left.y2;
    const bool rightHorizontal = right.y1 == right.y2;
    if (leftHorizontal != rightHorizontal)
    {
        return leftHorizontal;
    }
    const std::int32_t leftAxis = leftHorizontal ? left.y1 : left.x1;
    const std::int32_t rightAxis = rightHorizontal ? right.y1 : right.x1;
    if (leftAxis != rightAxis)
    {
        return leftAxis < rightAxis;
    }
    return (leftHorizontal ? left.x1 : left.y1) < (rightHorizontal ? right.x1 : right.y1);
}

void drawMergedBoundaries(ImDrawList & drawList,
                          ImVec2 canvasOrigin,
                          const MapCanvasViewState & state,
                          double marchingDistance,
                          std::vector<BoundaryEdge> edges,
                          float thickness,
                          float spacing)
{
    std::ranges::sort(edges, boundaryEdgeLess);
    std::vector<BoundaryEdge> merged;
    for (const BoundaryEdge edge : edges)
    {
        if (!merged.empty())
        {
            BoundaryEdge & previous = merged.back();
            const bool sameHorizontal = (previous.y1 == previous.y2) == (edge.y1 == edge.y2);
            const bool sameAxis = previous.y1 == previous.y2 ? previous.y1 == edge.y1 : previous.x1 == edge.x1;
            const bool adjacent = previous.x2 == edge.x1 && previous.y2 == edge.y1;
            if (sameHorizontal && sameAxis && adjacent)
            {
                previous.x2 = edge.x2;
                previous.y2 = edge.y2;
                continue;
            }
        }
        merged.push_back(edge);
    }

    for (const BoundaryEdge edge : merged)
    {
        const ImVec2 start(canvasOrigin.x + static_cast<float>(state.panX + (static_cast<double>(edge.x1) * state.zoom)),
                           canvasOrigin.y + static_cast<float>(state.panY + (static_cast<double>(edge.y1) * state.zoom)));
        const ImVec2 end(canvasOrigin.x + static_cast<float>(state.panX + (static_cast<double>(edge.x2) * state.zoom)),
                         canvasOrigin.y + static_cast<float>(state.panY + (static_cast<double>(edge.y2) * state.zoom)));
        drawMarchingLine(drawList, marchingDistance, start, end, thickness, spacing);
    }
}

void drawLassoSelection(ImDrawList & drawList, ImVec2 canvasOrigin, const MapCanvasViewState & state, double marchingDistance)
{
    if (state.lassoPath.size() <= 1U)
    {
        return;
    }

    std::vector<ImVec2> lassoPoints;
    lassoPoints.reserve(state.lassoPath.size());
    for (const uomul::TileCoord tile : state.lassoPath)
    {
        lassoPoints.emplace_back(
            canvasOrigin.x + static_cast<float>(state.panX + ((static_cast<double>(tile.x) + tileCenterOffset) * state.zoom)),
            canvasOrigin.y + static_cast<float>(state.panY + ((static_cast<double>(tile.y) + tileCenterOffset) * state.zoom)));
    }
    drawMarchingPolyline(drawList, marchingDistance, lassoPoints, 1.0F, lassoLineSpacing);
}

[[nodiscard]] uomul::TileRegion blockRegionToTileRegion(uomul::BlockRegion blocks, uomul::Dimensions dimensions) noexcept
{
    return uomul::TileRegion{
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

[[nodiscard]] std::optional<uomul::TileRegion>
selectionRegion(const MapCanvasViewState & state, const MapWorkspace & workspace, uomul::Dimensions dimensions)
{
    std::optional<uomul::TileRegion> selection = workspace.selectionBounds();
    if (workspace.selectionMode() == MapSelectionMode::blocks && workspace.blockSelection())
    {
        selection = blockRegionToTileRegion(*workspace.blockSelection(), dimensions);
    }
    if (state.dragStart && state.dragEnd)
    {
        if (workspace.selectionMode() == MapSelectionMode::tiles)
        {
            selection = tileSelectionFromDrag(*state.dragStart, *state.dragEnd, dimensions);
        }
        else
        {
            selection =
                blockRegionToTileRegion(blockSelectionFromDrag(*state.dragStart, *state.dragEnd, dimensions), dimensions);
        }
    }
    return selection;
}

void drawTileMaskBoundaries(ImDrawList & drawList,
                            ImVec2 canvasOrigin,
                            const MapCanvasViewState & state,
                            const MapSelectionMask & mask,
                            uomul::Dimensions dimensions,
                            double marchingDistance,
                            std::uint32_t visibleMinX,
                            std::uint32_t visibleMaxX,
                            std::uint32_t visibleMinY,
                            std::uint32_t visibleMaxY)
{
    std::vector<BoundaryEdge> edges;
    for (std::uint32_t y = visibleMinY; y < visibleMaxY; y++)
    {
        for (std::uint32_t x = visibleMinX; x < visibleMaxX; x++)
        {
            if (!mask.contains(uomul::TileCoord{.x = x, .y = y}))
            {
                continue;
            }
            const auto appendEdge =
                [&edges, &mask, dimensions, x, y](
                    std::int32_t dx, std::int32_t dy, std::int32_t x1, std::int32_t y1, std::int32_t x2, std::int32_t y2)
            {
                if (selectionCellHasExposedEdge(
                        mask, static_cast<std::int32_t>(x), static_cast<std::int32_t>(y), dx, dy, dimensions))
                {
                    appendBoundaryEdge(edges, x1, y1, x2, y2);
                }
            };
            appendEdge(-1,
                       0,
                       static_cast<std::int32_t>(x),
                       static_cast<std::int32_t>(y),
                       static_cast<std::int32_t>(x),
                       static_cast<std::int32_t>(y + 1U));
            appendEdge(1,
                       0,
                       static_cast<std::int32_t>(x + 1U),
                       static_cast<std::int32_t>(y),
                       static_cast<std::int32_t>(x + 1U),
                       static_cast<std::int32_t>(y + 1U));
            appendEdge(0,
                       -1,
                       static_cast<std::int32_t>(x),
                       static_cast<std::int32_t>(y),
                       static_cast<std::int32_t>(x + 1U),
                       static_cast<std::int32_t>(y));
            appendEdge(0,
                       1,
                       static_cast<std::int32_t>(x),
                       static_cast<std::int32_t>(y + 1U),
                       static_cast<std::int32_t>(x + 1U),
                       static_cast<std::int32_t>(y + 1U));
        }
    }
    drawMergedBoundaries(drawList, canvasOrigin, state, marchingDistance, std::move(edges), 1.0F, selectionLineSpacing);
}

void drawBlockMaskBoundaries(ImDrawList & drawList,
                             ImVec2 canvasOrigin,
                             const MapCanvasViewState & state,
                             const MapSelectionMask & mask,
                             uomul::Dimensions dimensions,
                             double marchingDistance,
                             std::uint32_t visibleMinX,
                             std::uint32_t visibleMaxX,
                             std::uint32_t visibleMinY,
                             std::uint32_t visibleMaxY)
{
    const std::uint32_t blockWidth = (dimensions.widthTiles / uomul::mapBlockDimension) +
                                     static_cast<std::uint32_t>(dimensions.widthTiles % uomul::mapBlockDimension != 0U);
    const std::uint32_t blockHeight = (dimensions.heightTiles / uomul::mapBlockDimension) +
                                      static_cast<std::uint32_t>(dimensions.heightTiles % uomul::mapBlockDimension != 0U);
    const std::uint32_t visibleBlockMinX = visibleMinX / uomul::mapBlockDimension;
    const std::uint32_t visibleBlockMaxX =
        std::min(blockWidth, (visibleMaxX + uomul::mapBlockDimension - 1U) / uomul::mapBlockDimension);
    const std::uint32_t visibleBlockMinY = visibleMinY / uomul::mapBlockDimension;
    const std::uint32_t visibleBlockMaxY =
        std::min(blockHeight, (visibleMaxY + uomul::mapBlockDimension - 1U) / uomul::mapBlockDimension);
    std::vector<BoundaryEdge> edges;
    for (std::uint32_t y = visibleBlockMinY; y < visibleBlockMaxY; y++)
    {
        for (std::uint32_t x = visibleBlockMinX; x < visibleBlockMaxX; x++)
        {
            if (!mask.contains(uomul::BlockCoord{.x = x, .y = y}))
            {
                continue;
            }
            const std::uint32_t tileMinX = x * uomul::mapBlockDimension;
            const std::uint32_t tileMinY = y * uomul::mapBlockDimension;
            const std::uint32_t tileMaxX = std::min(dimensions.widthTiles, tileMinX + uomul::mapBlockDimension);
            const std::uint32_t tileMaxY = std::min(dimensions.heightTiles, tileMinY + uomul::mapBlockDimension);
            const auto appendEdge =
                [&edges, &mask, dimensions, x, y](
                    std::int32_t dx, std::int32_t dy, std::int32_t x1, std::int32_t y1, std::int32_t x2, std::int32_t y2)
            {
                if (selectionCellHasExposedEdge(
                        mask, static_cast<std::int32_t>(x), static_cast<std::int32_t>(y), dx, dy, dimensions))
                {
                    appendBoundaryEdge(edges, x1, y1, x2, y2);
                }
            };
            appendEdge(-1,
                       0,
                       static_cast<std::int32_t>(tileMinX),
                       static_cast<std::int32_t>(tileMinY),
                       static_cast<std::int32_t>(tileMinX),
                       static_cast<std::int32_t>(tileMaxY));
            appendEdge(1,
                       0,
                       static_cast<std::int32_t>(tileMaxX),
                       static_cast<std::int32_t>(tileMinY),
                       static_cast<std::int32_t>(tileMaxX),
                       static_cast<std::int32_t>(tileMaxY));
            appendEdge(0,
                       -1,
                       static_cast<std::int32_t>(tileMinX),
                       static_cast<std::int32_t>(tileMinY),
                       static_cast<std::int32_t>(tileMaxX),
                       static_cast<std::int32_t>(tileMinY));
            appendEdge(0,
                       1,
                       static_cast<std::int32_t>(tileMinX),
                       static_cast<std::int32_t>(tileMaxY),
                       static_cast<std::int32_t>(tileMaxX),
                       static_cast<std::int32_t>(tileMaxY));
        }
    }
    drawMergedBoundaries(drawList,
                         canvasOrigin,
                         state,
                         marchingDistance,
                         std::move(edges),
                         blockSelectionLineThickness,
                         blockSelectionLineSpacing);
}

void drawSelectionMask(ImDrawList & drawList,
                       ImVec2 canvasOrigin,
                       const MapCanvasViewState & state,
                       const MapSelectionMask & mask,
                       uomul::Dimensions dimensions,
                       double marchingDistance,
                       std::uint32_t visibleMinX,
                       std::uint32_t visibleMaxX,
                       std::uint32_t visibleMinY,
                       std::uint32_t visibleMaxY)
{
    if (mask.granularity() == ToolSelectionGranularity::tiles)
    {
        drawTileMaskBoundaries(drawList,
                               canvasOrigin,
                               state,
                               mask,
                               dimensions,
                               marchingDistance,
                               visibleMinX,
                               visibleMaxX,
                               visibleMinY,
                               visibleMaxY);
        return;
    }
    drawBlockMaskBoundaries(
        drawList, canvasOrigin, state, mask, dimensions, marchingDistance, visibleMinX, visibleMaxX, visibleMinY, visibleMaxY);
}

void drawSelection(ImDrawList & drawList,
                   ImVec2 canvasOrigin,
                   const MapCanvasViewState & state,
                   const MapWorkspace & workspace,
                   uomul::Dimensions dimensions,
                   std::uint32_t visibleMinX,
                   std::uint32_t visibleMaxX,
                   std::uint32_t visibleMinY,
                   std::uint32_t visibleMaxY)
{
    const double marchingDistance = ImGui::GetTime() * 24.0;
    drawLassoSelection(drawList, canvasOrigin, state, marchingDistance);
    const std::optional<uomul::TileRegion> selection = selectionRegion(state, workspace, dimensions);
    const MapSelectionMask & mask = workspace.selectionMask();
    if ((!state.dragStart || !state.dragEnd) && !mask.empty())
    {
        drawSelectionMask(drawList,
                          canvasOrigin,
                          state,
                          mask,
                          dimensions,
                          marchingDistance,
                          visibleMinX,
                          visibleMaxX,
                          visibleMinY,
                          visibleMaxY);
        return;
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
    drawMarchingRect(
        drawList, marchingDistance, minimum, maximum, static_cast<float>(selectionOutlineThickness), selectionLineSpacing);
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
                     const TextureProvider & textures,
                     const RadarColorProvider & radarColors,
                     MapViewTileSource tileSource,
                     MapCanvasViewState & state,
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

    for (std::uint32_t pixelX = 0; pixelX < rasterWidth; pixelX++)
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
            const uomul::MapTile tile = loadedBlock->block->at(static_cast<std::size_t>(localIndex));
            const double localX = mapX - static_cast<double>(tileX);
            const double localTileY = mapY - static_cast<double>(tileY);
            const MapCanvasCachedTexture & texture = cachedTexture(state, textures, tile.id);
            const auto texturePixel =
                mode == MapDisplayMode::z ? std::nullopt : selectedTexturePixel(texture, tileSource, localX, localTileY);
            const auto fallback = radarColors.mapColor(tile.id);
            color = packedMapColor(tileDisplayColor(mode, tile, texturePixel ? texturePixel : fallback));
        }
        else if (loadedBlock->error && !state.lodReadError)
        {
            state.lodReadError = loadedBlock->error;
        }

        writeLodPixel(pixels, (static_cast<std::size_t>(pixelY) * rasterWidth) + pixelX, color);
    }
}

void rasterizeLod(const MapWorkspace & workspace,
                  const TextureProvider & textures,
                  const RadarColorProvider & radarColors,
                  MapViewTileSource tileSource,
                  MapCanvasViewState & state,
                  std::uint32_t rasterWidth,
                  std::uint32_t rasterHeight,
                  double scaleX,
                  double scaleY,
                  MapDisplayMode mode)
{
    const std::size_t pixelCount = static_cast<std::size_t>(rasterWidth) * rasterHeight;
    state.lodPixels.resize(pixelCount * lodPixelChannels);
    const ImU32 backgroundColor = theme::packedColor(theme::canvasBackground);

    for (std::size_t pixel = 0; pixel < pixelCount; pixel++)
    {
        writeLodPixel(state.lodPixels, pixel, backgroundColor);
    }

    state.lodReadError.reset();

    for (std::uint32_t pixelY = 0; pixelY < rasterHeight; pixelY++)
    {
        rasterizeLodRow(
            workspace, textures, radarColors, tileSource, state, state.lodPixels, rasterWidth, pixelY, scaleX, scaleY, mode);
    }
}

void uploadLodTexture(MapCanvasViewState & state, std::uint32_t rasterWidth, std::uint32_t rasterHeight)
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

[[nodiscard]] bool lodCacheMatches(const MapCanvasViewState & state,
                                   const TextureProvider & textures,
                                   const RadarColorProvider & radarColors,
                                   MapViewTileSource tileSource,
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
           state.lodTextureRevision == textures.status().revision &&
           state.lodRadarColorRevision == radarColors.status().revision && state.lodTileSource == tileSource;
}

void saveLodCacheKey(MapCanvasViewState & state,
                     const TextureProvider & textures,
                     const RadarColorProvider & radarColors,
                     MapViewTileSource tileSource,
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
    state.lodTextureRevision = textures.status().revision;
    state.lodRadarColorRevision = radarColors.status().revision;
    state.lodTileSource = tileSource;
    state.lodCacheValid = true;
}

[[nodiscard]] bool drawScreenSpaceLod(const MapWorkspace & workspace,
                                      const TextureProvider & textures,
                                      const RadarColorProvider & radarColors,
                                      MapViewTileSource tileSource,
                                      MapCanvasViewState & state,
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
    if (!lodCacheMatches(
            state, textures, radarColors, tileSource, rasterWidth, rasterHeight, canvasSize, framebufferScale, mode))
    {
        rasterizeLod(workspace, textures, radarColors, tileSource, state, rasterWidth, rasterHeight, scaleX, scaleY, mode);
        uploadLodTexture(state, rasterWidth, rasterHeight);

        if (state.lodTexture == 0U)
        {
            return false;
        }

        saveLodCacheKey(state, textures, radarColors, tileSource, rasterWidth, rasterHeight, canvasSize, framebufferScale);
        state.lodDisplayMode = mode;
    }

    visibleReadError = state.lodReadError;
    drawList.AddImage(static_cast<ImTextureID>(state.lodTexture),
                      canvasOrigin,
                      ImVec2(canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y),
                      ImVec2(0.0F, 0.0F),
                      ImVec2(1.0F, 1.0F));

    return true;
}

void drawCanvasContents(const MapWorkspace & workspace,
                        const TextureProvider & textures,
                        const RadarColorProvider & radarColors,
                        MapViewTileSource tileSource,
                        MapCanvasViewState & state,
                        ImVec2 canvasOrigin,
                        ImVec2 canvasSize,
                        std::optional<uomul::TileCoord> currentTile,
                        std::optional<uomul::Error> & visibleReadError)
{
    ImDrawList * drawList = ImGui::GetWindowDrawList();
    const ImVec2 canvasMaximum(canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y);
    drawList->AddRectFilled(canvasOrigin, canvasMaximum, theme::packedColor(theme::canvasBackground));
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
        !drawScreenSpaceLod(
            workspace, textures, radarColors, tileSource, state, *drawList, canvasOrigin, canvasSize, visibleReadError))
    {
        state.lodCacheValid = false;
        state.lodReadError.reset();

        for (std::uint64_t tileY = minY; tileY < maxY; tileY++)
        {
            const std::uint32_t tileRow = static_cast<std::uint32_t>(tileY);
            const std::uint32_t blockY = tileRow / uomul::mapBlockDimension;
            const std::uint32_t localY = tileRow % uomul::mapBlockDimension;
            std::uint32_t cachedBlockX = std::numeric_limits<std::uint32_t>::max();
            const MapCanvasCachedBlock * cached = nullptr;

            for (std::uint64_t tileX = minX; tileX < maxX; tileX++)
            {
                const std::uint32_t tileColumn = static_cast<std::uint32_t>(tileX);
                const std::uint32_t blockX = tileColumn / uomul::mapBlockDimension;

                if (blockX != cachedBlockX)
                {
                    cached = &cachedBlock(workspace, state, {.x = blockX, .y = blockY});
                    cachedBlockX = blockX;
                }

                if (cached->block)
                {
                    const std::uint32_t localIndex =
                        (localY * uomul::mapBlockDimension) + (tileColumn % uomul::mapBlockDimension);
                    const uomul::MapTile tile = cached->block->at(static_cast<std::size_t>(localIndex));
                    drawCanvasTile(*drawList,
                                   state,
                                   textures,
                                   radarColors,
                                   tileSource,
                                   mode,
                                   tile,
                                   tileScreenRect(canvasOrigin, state, tileColumn, tileRow));
                }
                else if (cached->error && !visibleReadError)
                {
                    visibleReadError = cached->error;
                }
                else
                {
                    const ScreenRect tileRect = tileScreenRect(canvasOrigin, state, tileColumn, tileRow);
                    drawList->AddRectFilled(tileRect.minimum, tileRect.maximum, unavailableColor(tileColumn, tileRow));
                }
            }
        }
    }

    drawSelection(*drawList, canvasOrigin, state, workspace, dimensions, minX, maxX, minY, maxY);

    if (!currentTile && workspace.selectedTile())
    {
        const uomul::TileCoord tile = *workspace.selectedTile();
        const ScreenRect selectedTile = tileScreenRect(canvasOrigin, state, tile.x, tile.y);
        drawList->AddRect(
            selectedTile.minimum, selectedTile.maximum, theme::packedColor(theme::selection), 0.0F, ImDrawFlags_None, 1.0F);
    }

    if (currentTile)
    {
        const uomul::TileCoord tile = *currentTile;
        const ScreenRect currentTileRect = tileScreenRect(canvasOrigin, state, tile.x, tile.y);
        drawList->AddRect(currentTileRect.minimum,
                          currentTileRect.maximum,
                          theme::packedColor(theme::primaryText),
                          0.0F,
                          ImDrawFlags_None,
                          1.0F);
    }

    const uomul::MapReader * reader = workspace.reader();

    if (reader != nullptr && reader->missingBlocks() > 0U)
    {
        const std::string missingMessage = "Incomplete map: " + std::to_string(reader->missingBlocks()) + " blocks missing";
        drawList->AddText(ImVec2(canvasOrigin.x + static_cast<float>(errorTextLeftPadding),
                                 canvasOrigin.y + static_cast<float>(errorTextTopPadding)),
                          theme::packedColor(theme::warning),
                          missingMessage.c_str());
    }

    if (visibleReadError)
    {
        const std::string errorMessage = "Map read error: " + visibleReadError->message;
        drawList->AddText(ImVec2(canvasOrigin.x + static_cast<float>(errorTextLeftPadding),
                                 canvasOrigin.y + static_cast<float>(readErrorTextOffset)),
                          theme::packedColor(theme::error),
                          errorMessage.c_str());
    }
    else if (workspace.error())
    {
        const std::string errorMessage = "Map error: " + workspace.error()->message;
        drawList->AddText(ImVec2(canvasOrigin.x + static_cast<float>(errorTextLeftPadding),
                                 canvasOrigin.y + static_cast<float>(errorTextTopPadding)),
                          theme::packedColor(theme::error),
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

void drawLayerControls(MapWorkspace & workspace, MapCanvasViewState & state)
{
    drawLayerButton(workspace, MapDisplayMode::tileIds, "Textures");
    ImGui::SameLine();
    drawLayerButton(workspace, MapDisplayMode::z, "Z");
    ImGui::SameLine();
    drawLayerButton(workspace, MapDisplayMode::combined, "Combined");

    ImGui::SameLine();
    if (ImGui::Button("Go to Coordinate..."))
    {
        state.coordinateInputKind = CoordinateKind::tile;
        state.coordinateInputFocusRequested = true;
        state.coordinateInput.fill('\0');
        state.coordinatePopupRequested = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Go to Block..."))
    {
        state.coordinateInputKind = CoordinateKind::block;
        state.coordinateInputFocusRequested = true;
        state.coordinateInput.fill('\0');
        state.coordinatePopupRequested = true;
    }
}

void resetForDocument(const MapWorkspace & workspace, MapCanvasViewState & state, uomul::Dimensions dimensions)
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
    state.scrollResetPending = true;
}

void applyNavigationRequest(MapCanvasViewState & state, const MapWorkspace & workspace, ImVec2 canvasSize)
{
    const MapCanvasViewRequest request = state.navigationRequest.request;
    if (request == MapCanvasViewRequest::none || workspace.reader() == nullptr)
    {
        return;
    }

    const auto selectionBounds = [](const MapWorkspace & selectionWorkspace) -> std::optional<uomul::TileRegion>
    { return selectionWorkspace.selectionBounds(); };

    if (request == MapCanvasViewRequest::fitMap)
    {
        state.zoom = fitZoomForCanvas(workspace.dimensions(), canvasSize);
        state.panX =
            (static_cast<double>(canvasSize.x) - (static_cast<double>(workspace.dimensions().widthTiles) * state.zoom)) /
            coordinateCenterDivisor;
        state.panY =
            (static_cast<double>(canvasSize.y) - (static_cast<double>(workspace.dimensions().heightTiles) * state.zoom)) /
            coordinateCenterDivisor;
    }
    else if (request == MapCanvasViewRequest::fitSelection || request == MapCanvasViewRequest::centerSelection)
    {
        const auto selection = selectionBounds(workspace);
        if (selection)
        {
            if (request == MapCanvasViewRequest::fitSelection)
            {
                const double widthZoom = static_cast<double>(canvasSize.x) /
                                         static_cast<double>(std::max(1U, selection->maxExclusive.x - selection->min.x));
                const double heightZoom = static_cast<double>(canvasSize.y) /
                                          static_cast<double>(std::max(1U, selection->maxExclusive.y - selection->min.y));
                state.zoom = std::clamp(std::min(widthZoom, heightZoom), minimumCanvasZoom, maximumCanvasZoom);
            }
            state.panX = (static_cast<double>(canvasSize.x) -
                          (static_cast<double>(selection->maxExclusive.x + selection->min.x) * state.zoom)) /
                         coordinateCenterDivisor;
            state.panY = (static_cast<double>(canvasSize.y) -
                          (static_cast<double>(selection->maxExclusive.y + selection->min.y) * state.zoom)) /
                         coordinateCenterDivisor;
        }
    }
    if (state.navigationRequest.coordinate)
    {
        const auto coordinate = *state.navigationRequest.coordinate;
        state.panX =
            (static_cast<double>(canvasSize.x) / coordinateCenterDivisor) - (static_cast<double>(coordinate.x) * state.zoom);
        state.panY =
            (static_cast<double>(canvasSize.y) / coordinateCenterDivisor) - (static_cast<double>(coordinate.y) * state.zoom);
        state.navigationRequest.coordinate.reset();
    }
    const auto clampPan = [](double pan, std::uint32_t tileCount, double zoom, float extent)
    {
        const double mapExtent = static_cast<double>(tileCount) * zoom;
        const double minimum = std::min(0.0, static_cast<double>(extent) - mapExtent);
        return std::clamp(pan, std::max(-maximumPanMagnitude, minimum), 0.0);
    };
    state.panX = clampPan(state.panX, workspace.dimensions().widthTiles, state.zoom, canvasSize.x);
    state.panY = clampPan(state.panY, workspace.dimensions().heightTiles, state.zoom, canvasSize.y);
    state.scrollResetPending = true;
    state.lodCacheValid = false;
    state.navigationRequest.request = MapCanvasViewRequest::none;
}

void drawCoordinatePopup(MapCanvasViewState & state)
{
    if (state.coordinatePopupRequested)
    {
        state.coordinatePopupRequested = false;
        state.coordinateDialogVisible = true;
    }
    if (!state.coordinateDialogVisible)
    {
        return;
    }
    ImGui::SetNextWindowPos(
        ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(coordinateDialogPivot, coordinateDialogPivot));
    constexpr auto coordinateDialogFlags = static_cast<ImGuiWindowFlags>(
        static_cast<unsigned int>(ImGuiWindowFlags_AlwaysAutoResize) | static_cast<unsigned int>(ImGuiWindowFlags_NoDocking) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoTitleBar) | static_cast<unsigned int>(ImGuiWindowFlags_NoSavedSettings));
    if (!ImGui::Begin("Go to Coordinate###map.coordinate-dialog", &state.coordinateDialogVisible, coordinateDialogFlags))
    {
        ImGui::End();
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        state.coordinateInput.fill('\0');
        state.coordinateDialogVisible = false;
        ImGui::End();
        return;
    }
    ImGui::TextUnformatted(state.coordinateInputKind == CoordinateKind::tile ? "Enter x y." : "Enter block x y.");
    if (state.coordinateInputFocusRequested)
    {
        ImGui::SetKeyboardFocusHere();
        state.coordinateInputFocusRequested = false;
    }
    const bool submitted = ImGui::InputText(
        "##Coordinate", state.coordinateInput.data(), state.coordinateInput.size(), ImGuiInputTextFlags_EnterReturnsTrue);
    const auto parsed = parseCoordinate(state.coordinateInput.data(), state.coordinateInputKind);
    if (submitted)
    {
        if (parsed)
        {
            state.navigationRequest.coordinate = parsed->kind == CoordinateKind::block
                                                     ? uomul::TileCoord{.x = parsed->value.x * uomul::mapBlockDimension,
                                                                        .y = parsed->value.y * uomul::mapBlockDimension,}
                                                     : parsed->value;
            state.navigationRequest.request = MapCanvasViewRequest::centerSelection;
            state.coordinateInput.fill('\0');
            state.coordinateDialogVisible = false;
        }
    }
    if (!parsed && state.coordinateInput.front() != '\0')
    {
        ImGui::TextColored(theme::imguiColor(theme::warning), "%s", parsed.error().c_str());
    }
    ImGui::End();
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

void updateNavigation(MapCanvasViewState & state,
                      const ToolState & toolState,
                      ImVec2 mouse,
                      ImVec2 canvasOrigin,
                      bool canvasHovered,
                      double minimumZoom)
{
    if (!state.scrollResetPending)
    {
        const double scrollX = static_cast<double>(ImGui::GetScrollX());
        const double scrollY = static_cast<double>(ImGui::GetScrollY());
        if (std::abs(scrollX - std::max(0.0, -state.panX)) > floatCacheTolerance)
        {
            state.panX = -scrollX;
        }
        if (std::abs(scrollY - std::max(0.0, -state.panY)) > floatCacheTolerance)
        {
            state.panY = -scrollY;
        }
    }
    else
    {
        state.scrollResetPending = false;
    }

    const bool temporaryPan = ImGui::IsKeyDown(ImGuiKey_Space);
    const bool moveTool = toolState.activeTool() == ToolId::move;
    const bool panButtonPressed =
        ImGui::IsMouseClicked(ImGuiMouseButton_Left) && (moveTool || temporaryPan) && !ImGui::GetIO().KeyAlt;
    const bool legacyPanButtonPressed = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    if (canvasHovered && (panButtonPressed || legacyPanButtonPressed))
    {
        state.panning = true;
        state.panningWithLeftButton = panButtonPressed;
        state.panStartMouseX = mouse.x;
        state.panStartMouseY = mouse.y;
        state.panStartX = state.panX;
        state.panStartY = state.panY;
    }

    const bool panButtonDown =
        state.panningWithLeftButton ? ImGui::IsMouseDown(ImGuiMouseButton_Left) : ImGui::IsMouseDown(ImGuiMouseButton_Right);
    if (state.panning && panButtonDown)
    {
        state.panX = std::clamp(state.panStartX + mouse.x - state.panStartMouseX, -maximumPanMagnitude, 0.0);
        state.panY = std::clamp(state.panStartY + mouse.y - state.panStartMouseY, -maximumPanMagnitude, 0.0);
        ImGui::SetScrollX(static_cast<float>(-state.panX));
        ImGui::SetScrollY(static_cast<float>(-state.panY));
        state.panX = -static_cast<double>(ImGui::GetScrollX());
        state.panY = -static_cast<double>(ImGui::GetScrollY());
    }

    if ((state.panningWithLeftButton && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) ||
        (!state.panningWithLeftButton && ImGui::IsMouseReleased(ImGuiMouseButton_Right)))
    {
        state.panning = false;
        state.panningWithLeftButton = false;
    }

    if (!canvasHovered || std::abs(ImGui::GetIO().MouseWheel) <= std::numeric_limits<float>::epsilon())
    {
        return;
    }

    const double oldZoom = state.zoom;
    const double newZoom =
        std::clamp(oldZoom * std::pow(canvasZoomFactor, ImGui::GetIO().MouseWheel), minimumZoom, maximumCanvasZoom);
    const double mapX = (static_cast<double>(mouse.x) - canvasOrigin.x - state.panX) / oldZoom;
    const double mapY = (static_cast<double>(mouse.y) - canvasOrigin.y - state.panY) / oldZoom;
    state.panX = static_cast<double>(mouse.x) - canvasOrigin.x - (mapX * newZoom);
    state.panY = static_cast<double>(mouse.y) - canvasOrigin.y - (mapY * newZoom);
    state.zoom = newZoom;
    ImGui::SetScrollX(static_cast<float>(std::max(0.0, -state.panX)));
    ImGui::SetScrollY(static_cast<float>(std::max(0.0, -state.panY)));
    state.panX = -static_cast<double>(ImGui::GetScrollX());
    state.panY = -static_cast<double>(ImGui::GetScrollY());
}

[[nodiscard]] SelectionCombineMode selectionCombineMode() noexcept
{
    if (ImGui::GetIO().KeyCtrl)
    {
        return SelectionCombineMode::subtract;
    }
    if (ImGui::GetIO().KeyShift)
    {
        return SelectionCombineMode::add;
    }
    return SelectionCombineMode::replace;
}

void clearSelectionDrag(MapCanvasViewState & state)
{
    state.dragStart.reset();
    state.dragEnd.reset();
    state.lassoing = false;
    state.lassoPath.clear();
}

void updateLassoSelection(MapWorkspace & workspace,
                          MapCanvasViewState & state,
                          std::optional<uomul::TileCoord> currentTile,
                          bool canvasHovered)
{
    if (canvasHovered && currentTile && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        state.lassoing = true;
        state.lassoPath.clear();
        state.lassoPath.push_back(*currentTile);
        state.lassoCombineMode = selectionCombineMode();
    }
    if (state.lassoing && ImGui::IsMouseDown(ImGuiMouseButton_Left) && currentTile &&
        (state.lassoPath.empty() || state.lassoPath.back().x != currentTile->x || state.lassoPath.back().y != currentTile->y))
    {
        state.lassoPath.push_back(*currentTile);
    }
    if (!state.lassoing || !ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        return;
    }
    workspace.applyLassoSelection(state.lassoPath, state.lassoCombineMode);
    state.lassoing = false;
    state.lassoPath.clear();
}

void updateRectangleSelection(MapWorkspace & workspace,
                              MapCanvasViewState & state,
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
        const SelectionCombineMode mode = selectionCombineMode();
        if (workspace.selectionMode() == MapSelectionMode::tiles)
        {
            workspace.applyTileSelection(tileSelectionFromDrag(*state.dragStart, *state.dragEnd, dimensions), mode);
        }
        else
        {
            workspace.applyBlockSelection(blockSelectionFromDrag(*state.dragStart, *state.dragEnd, dimensions), mode);
        }
        const uomul::TileCoord inspectorTile = selectionInspectorTile(workspace.selectionMode(), *state.dragEnd);
        const auto result = workspace.updateSelectedTile(inspectorTile);
        static_cast<void>(result);
    }
    state.dragStart.reset();
    state.dragEnd.reset();
}

void updateSelectionDrag(MapWorkspace & workspace,
                         MapCanvasViewState & state,
                         const ToolState & toolState,
                         std::optional<uomul::TileCoord> currentTile,
                         uomul::Dimensions dimensions,
                         bool canvasHovered,
                         bool clickHandled)
{
    const bool lassoTool = toolState.activeTool() == ToolId::lassoSelect;
    const bool selectionTool = toolState.activeTool() == ToolId::rectangleSelect || lassoTool;
    if (!selectionTool || ImGui::IsKeyDown(ImGuiKey_Space) || ImGui::GetIO().KeyAlt || toolState.tilePickRequested() ||
        clickHandled)
    {
        clearSelectionDrag(state);
        return;
    }
    if (lassoTool)
    {
        updateLassoSelection(workspace, state, currentTile, canvasHovered);
        return;
    }
    updateRectangleSelection(workspace, state, currentTile, dimensions, canvasHovered);
}

bool updateTilePick(const MapWorkspace & workspace,
                    ToolState & toolState,
                    std::optional<uomul::TileCoord> currentTile,
                    bool canvasHovered,
                    const std::function<std::optional<std::string>(std::uint16_t)> & tiledataNameLookup)
{
    const bool altClick = canvasHovered && ImGui::GetIO().KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    const bool requestedPick = toolState.tilePickRequested() && canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    if ((!altClick && !requestedPick) || !currentTile || !workspace.hoveredValue())
    {
        return false;
    }

    if (toolState.tilePickRequested() && toolState.tilePickTarget() == TilePickTarget::fuzzyName)
    {
        if (const auto name = tiledataNameLookup(workspace.hoveredValue()->id))
        {
            toolState.setPickedTileName(*name);
        }
        return true;
    }

    toolState.setPickedTile(*workspace.hoveredValue());
    return true;
}

bool updateEditTool(const ToolState & toolState,
                    std::optional<uomul::TileCoord> currentTile,
                    bool canvasHovered,
                    bool tilePickHandled,
                    const std::function<void(uomul::TileCoord)> & executeEdit)
{
    if ((toolState.activeTool() != ToolId::fill && toolState.activeTool() != ToolId::replace &&
         toolState.activeTool() != ToolId::fuzzySelect) ||
        tilePickHandled || ImGui::IsKeyDown(ImGuiKey_Space) || ImGui::GetIO().KeyAlt || !canvasHovered || !currentTile ||
        !ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        return false;
    }

    executeEdit(*currentTile);
    return true;
}

void drawEmptyCanvas(ImVec2 canvasOrigin,
                     ImVec2 canvasMaximum,
                     const MapWorkspace & workspace,
                     const std::function<void()> & openMap,
                     const std::function<void()> & createMap)
{
    ImDrawList * drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(canvasOrigin, canvasMaximum, theme::packedColor(theme::canvasBackground));
    drawList->AddText(
        ImVec2(canvasOrigin.x + static_cast<float>(noMapTextPadding), canvasOrigin.y + static_cast<float>(noMapTextPadding)),
        theme::packedColor(theme::primaryText),
        "No map open");

    ImGui::SetCursorScreenPos(ImVec2(canvasOrigin.x + static_cast<float>(noMapTextPadding),
                                     canvasOrigin.y + static_cast<float>(noMapTextPadding + noMapButtonOffset)));
    if (ImGui::Button("Open..."))
    {
        openMap();
    }
    ImGui::SameLine();
    if (ImGui::Button("Create New..."))
    {
        createMap();
    }

    if (workspace.error())
    {
        const std::string errorMessage = "Map error: " + workspace.error()->message;
        drawList->AddText(ImVec2(canvasOrigin.x + static_cast<float>(errorTextLeftPadding),
                                 canvasOrigin.y + static_cast<float>(readErrorTextOffset)),
                          theme::packedColor(theme::error),
                          errorMessage.c_str());
    }
}

} // namespace

MapColor mapCanvasTileColor(MapDisplayMode mode, uomul::MapTile tile, const RadarColorProvider & radarColors) noexcept
{
    return tileDisplayColor(mode, tile, radarColors.mapColor(tile.id));
}

void drawMapCanvasView(MapWorkspaceManager & workspaceManager,
                       ToolState & toolState,
                       const TextureProvider & textures,
                       const RadarColorProvider & radarColors,
                       MapViewTileSource tileSource,
                       const std::string & windowName,
                       const std::function<void()> & activateDocument,
                       const std::function<void()> & openMap,
                       const std::function<void()> & createMap,
                       const std::function<void(uomul::TileCoord)> & executeEdit,
                       const std::function<std::optional<std::string>(std::uint16_t)> & tiledataNameLookup,
                       const std::function<void(const MapCanvasStatus &)> & updateLastHoveredCanvas,
                       bool & visible)
{
    MapCanvasViewState & state = workspaceManager.canvasState();
    if (!state.releaseTexture)
    {
        state.releaseTexture = [](std::uint32_t texture)
        {
            const GLuint handle = texture;
            glDeleteTextures(1, &handle);
        };
    }
    if (state.textureProviderRevision != textures.status().revision)
    {
        clearTextureCache(state);
        state.textureProviderRevision = textures.status().revision;
        state.lodCacheValid = false;
    }
    const bool contentsVisible = ImGui::Begin(windowName.c_str(), &visible);

    if (!visible || !contentsVisible)
    {
        ImGui::End();
        return;
    }

    activateDocument();
    MapWorkspace & workspace = workspaceManager.active();
    state.panX = std::isfinite(state.panX) ? state.panX : 0.0;
    state.panY = std::isfinite(state.panY) ? state.panY : 0.0;
    state.zoom = std::isfinite(state.zoom) ? state.zoom : defaultMapCanvasZoom;
    state.panX = std::clamp(state.panX, -maximumPanMagnitude, maximumPanMagnitude);
    state.panY = std::clamp(state.panY, -maximumPanMagnitude, maximumPanMagnitude);
    state.zoom = std::clamp(state.zoom, minimumCanvasZoom, maximumCanvasZoom);
    resetForDocument(workspace, state, workspace.dimensions());
    const uomul::Dimensions dimensions = workspace.dimensions();
    const bool hasDocument = workspace.reader() != nullptr;

    drawLayerControls(workspace, state);
    const bool canvasContentsVisible =
        ImGui::BeginChild("MapCanvasViewport",
                          ImVec2(0.0F, 0.0F),
                          ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const ImVec2 canvasOrigin(ImGui::GetWindowPos().x + ImGui::GetStyle().WindowBorderSize + ImGui::GetStyle().WindowPadding.x,
                              ImGui::GetWindowPos().y + ImGui::GetStyle().WindowBorderSize + ImGui::GetStyle().WindowPadding.y);
    const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    const double minimumZoom = hasDocument ? minimumZoomForCanvas(dimensions, canvasSize) : minimumCanvasZoom;
    state.zoom = std::clamp(state.zoom, minimumZoom, maximumCanvasZoom);
    if (!hasPendingNavigation(state))
    {
        state.panX = -static_cast<double>(ImGui::GetScrollX());
        state.panY = -static_cast<double>(ImGui::GetScrollY());
    }
    const ImVec2 canvasMaximum(canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y);
    const bool canvasHovered = canvasContentsVisible && ImGui::IsWindowHovered() &&
                               ImGui::IsMouseHoveringRect(canvasOrigin, canvasMaximum, false) &&
                               !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) && !ImGui::GetIO().WantTextInput;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    std::optional<uomul::TileCoord> currentTile;
    std::optional<uomul::Error> visibleReadError;

    if (state.scrollResetPending)
    {
        ImGui::SetScrollX(static_cast<float>(std::max(0.0, -state.panX)));
        ImGui::SetScrollY(static_cast<float>(std::max(0.0, -state.panY)));
    }

    if (hasDocument && canvasContentsVisible)
    {
        updateNavigation(state, toolState, mouse, canvasOrigin, canvasHovered, minimumZoom);
        currentTile = tileAtMouse(mouse, canvasOrigin, canvasSize, state, dimensions, canvasHovered);
        updateHover(workspace, currentTile);
        const bool tilePickHandled = updateTilePick(workspace, toolState, currentTile, canvasHovered, tiledataNameLookup);
        const bool editHandled = updateEditTool(toolState, currentTile, canvasHovered, tilePickHandled, executeEdit);
        updateSelectionDrag(
            workspace, state, toolState, currentTile, dimensions, canvasHovered, tilePickHandled || editHandled);
    }
    else
    {
        workspace.clearHover();
    }
    applyNavigationRequest(state, workspace, canvasSize);
    const ImVec2 contentSize(hasDocument ? canvasContentExtent(dimensions.widthTiles, state.zoom, canvasSize.x) : canvasSize.x,
                             hasDocument ? canvasContentExtent(dimensions.heightTiles, state.zoom, canvasSize.y)
                                         : canvasSize.y);
    ImGui::Dummy(contentSize);

    if (state.panning)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    if (canvasContentsVisible && canvasHovered)
    {
        updateLastHoveredCanvas(MapCanvasStatus{
            .canvasId = "Map Canvas",
            .hasDocument = hasDocument,
            .tile = currentTile,
            .zoom = state.zoom,
        });
    }

    if (canvasContentsVisible)
    {
        if (!hasDocument)
        {
            drawEmptyCanvas(canvasOrigin, canvasMaximum, workspace, openMap, createMap);
        }
        else
        {
            drawCanvasContents(
                workspace, textures, radarColors, tileSource, state, canvasOrigin, canvasSize, currentTile, visibleReadError);
        }
    }

    ImGui::EndChild();

    drawCoordinatePopup(state);

    ImGui::End();
}

} // namespace uo_mule::gui
