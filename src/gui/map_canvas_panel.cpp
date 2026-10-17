#include "gui/map_canvas_panel.h"
#include "gui/map_workspace.h"
#include "uomul/map.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <imgui.h>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace im_cpp::gui
{
namespace
{

constexpr double minimumCanvasZoom = 0.125;
constexpr double maximumCanvasZoom = 64.0;
constexpr double canvasZoomFactor = 1.15;
constexpr double maximumPanMagnitude = 1.0e12;
constexpr std::uint32_t maximumCanvasSamples = 512;
constexpr std::size_t maximumCachedBlocks = 512;
constexpr double combinedBrightnessMinimum = 0.5;
constexpr double combinedBrightnessRange = 1.0;
constexpr double selectionOutlineThickness = 2.0;
constexpr double errorTextLeftPadding = 10.0;
constexpr double errorTextTopPadding = 8.0;
constexpr double readErrorTextOffset = 28.0;
constexpr double noMapTextPadding = 12.0;
constexpr double editBadgeGreen = 0.65;
constexpr double editBadgeBlue = 0.2;
constexpr std::uint32_t channelMaximum = std::numeric_limits<std::uint8_t>::max();

[[nodiscard]] ImU32 colorForTile(MapDisplayMode mode, uomul::MapTile tile) noexcept
{
    if (mode == MapDisplayMode::z)
    {
        const auto gray = zGrayscale(tile.z);
        return IM_COL32(gray, gray, gray, channelMaximum);
    }

    const MapColor idColor = tileIdColor(tile.id);
    if (mode == MapDisplayMode::tileIds)
    {
        return IM_COL32(idColor.red, idColor.green, idColor.blue, channelMaximum);
    }
    const double brightness =
        combinedBrightnessMinimum +
        ((static_cast<double>(zGrayscale(tile.z)) / static_cast<double>(channelMaximum)) * combinedBrightnessRange);

    const auto red = static_cast<std::uint8_t>(
        std::clamp(static_cast<int>(static_cast<double>(idColor.red) * brightness), 0, static_cast<int>(channelMaximum)));
    const auto green = static_cast<std::uint8_t>(
        std::clamp(static_cast<int>(static_cast<double>(idColor.green) * brightness), 0, static_cast<int>(channelMaximum)));
    const auto blue = static_cast<std::uint8_t>(
        std::clamp(static_cast<int>(static_cast<double>(idColor.blue) * brightness), 0, static_cast<int>(channelMaximum)));
    return IM_COL32(red, green, blue, channelMaximum);
}

[[nodiscard]] ImU32 unavailableColor(std::uint32_t tileX, std::uint32_t tileY) noexcept
{
    constexpr ImU32 darkColor = IM_COL32(74, 20, 30, 255);
    constexpr ImU32 brightColor = IM_COL32(190, 35, 55, 255);
    return ((tileX / uomul::mapBlockDimension) + (tileY / uomul::mapBlockDimension)) % 2U == 0U ? darkColor : brightColor;
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
    const auto existing =
        std::ranges::find_if(state.blockCache,
                             [coordinate](const MapCanvasCachedBlock & entry)
                             { return entry.coordinate.x == coordinate.x && entry.coordinate.y == coordinate.y; });
    if (existing != state.blockCache.end())
    {
        if (state.cacheClock == std::numeric_limits<std::uint64_t>::max())
        {
            state.cacheClock = 0;
            for (MapCanvasCachedBlock & entry : state.blockCache)
            {
                entry.lastUsed = state.cacheClock;
                ++state.cacheClock;
            }
        }
        existing->lastUsed = state.cacheClock;
        ++state.cacheClock;
        return *existing;
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
    ++state.cacheClock;

    if (state.blockCache.size() < maximumCachedBlocks)
    {
        state.blockCache.push_back(std::move(entry));
        return state.blockCache.back();
    }
    const auto leastRecentlyUsed = std::ranges::min_element(
        state.blockCache,
        [](const MapCanvasCachedBlock & left, const MapCanvasCachedBlock & right) { return left.lastUsed < right.lastUsed; });
    *leastRecentlyUsed = std::move(entry);
    return *leastRecentlyUsed;
}

[[nodiscard]] std::uint32_t sampleStep(std::uint32_t width, std::uint32_t height) noexcept
{
    const std::uint64_t tileCount = static_cast<std::uint64_t>(width) * height;
    if (tileCount <= maximumCanvasSamples)
    {
        return 1;
    }

    std::uint64_t step = static_cast<std::uint64_t>(
        std::ceil(std::sqrt(static_cast<double>(tileCount) / static_cast<double>(maximumCanvasSamples))));
    while (((static_cast<std::uint64_t>(width) + step - 1U) / step) *
               ((static_cast<std::uint64_t>(height) + step - 1U) / step) >
           maximumCanvasSamples)
    {
        const std::uint64_t sampleCount = ((static_cast<std::uint64_t>(width) + step - 1U) / step) *
                                          ((static_cast<std::uint64_t>(height) + step - 1U) / step);
        const std::uint64_t multiplier = static_cast<std::uint64_t>(
            std::ceil(std::sqrt(static_cast<double>(sampleCount) / static_cast<double>(maximumCanvasSamples))));
        step = std::max(step + 1U, step * multiplier);
    }
    return static_cast<std::uint32_t>(step);
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
    drawList.AddRect(
        minimum, maximum, IM_COL32(255, 245, 120, 255), 0.0F, ImDrawFlags_None, static_cast<float>(selectionOutlineThickness));
}

void drawCanvasContents(const MapWorkspace & workspace,
                        MapCanvasPanelState & state,
                        ImVec2 canvasOrigin,
                        ImVec2 canvasSize,
                        std::optional<uomul::TileCoord> currentTile,
                        std::optional<uomul::Error> & visibleReadError)
{
    ImDrawList * drawList = ImGui::GetWindowDrawList();
    const ImVec2 canvasMaximum(canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y);
    drawList->AddRectFilled(canvasOrigin, canvasMaximum, IM_COL32(25, 28, 32, 255));
    drawList->PushClipRect(canvasOrigin, canvasMaximum, true);

    const uomul::Dimensions dimensions = workspace.dimensions();
    const double tileSize = state.zoom;
    const std::uint32_t minX = visibleStart(state.panX, tileSize, dimensions.widthTiles);
    const std::uint32_t minY = visibleStart(state.panY, tileSize, dimensions.heightTiles);
    const std::uint32_t maxX = visibleEnd(canvasSize.x, state.panX, tileSize, dimensions.widthTiles);
    const std::uint32_t maxY = visibleEnd(canvasSize.y, state.panY, tileSize, dimensions.heightTiles);
    const std::uint32_t step = sampleStep(maxX - minX, maxY - minY);
    const MapDisplayMode mode = workspace.displayMode();

    for (std::uint64_t tileY = minY; tileY < maxY; tileY += step)
    {
        for (std::uint64_t tileX = minX; tileX < maxX; tileX += step)
        {
            const std::uint64_t cellEndX = std::min(tileX + step, static_cast<std::uint64_t>(maxX));
            const std::uint64_t cellEndY = std::min(tileY + step, static_cast<std::uint64_t>(maxY));
            const std::uint32_t sampleX = static_cast<std::uint32_t>(tileX + ((cellEndX - tileX) / 2U));
            const std::uint32_t sampleY = static_cast<std::uint32_t>(tileY + ((cellEndY - tileY) / 2U));
            const uomul::BlockCoord blockCoordinate{
                .x = sampleX / uomul::mapBlockDimension,
                .y = sampleY / uomul::mapBlockDimension,
            };
            const MapCanvasCachedBlock & cached = cachedBlock(workspace, state, blockCoordinate);
            ImU32 color = unavailableColor(sampleX, sampleY);
            if (cached.block)
            {
                const std::uint32_t localIndex =
                    ((sampleY % uomul::mapBlockDimension) * uomul::mapBlockDimension) + (sampleX % uomul::mapBlockDimension);
                color = colorForTile(mode, cached.block->at(static_cast<std::size_t>(localIndex)));
            }
            else if (cached.error && !visibleReadError)
            {
                visibleReadError = cached.error;
            }

            const ImVec2 minimum(canvasOrigin.x + static_cast<float>(state.panX + (static_cast<double>(tileX) * tileSize)),
                                 canvasOrigin.y + static_cast<float>(state.panY + (static_cast<double>(tileY) * tileSize)));
            const ImVec2 maximum(canvasOrigin.x + static_cast<float>(state.panX + (static_cast<double>(cellEndX) * tileSize)),
                                 canvasOrigin.y + static_cast<float>(state.panY + (static_cast<double>(cellEndY) * tileSize)));
            drawList->AddRectFilled(minimum, maximum, color);
        }
    }

    drawSelection(*drawList, canvasOrigin, state, workspace, dimensions);
    const std::optional<uomul::TileCoord> inspectedTile = currentTile ? currentTile : workspace.selectedTile();
    if (inspectedTile)
    {
        const uomul::TileCoord tile = inspectedTile.value();
        const ImVec2 minimum(canvasOrigin.x + static_cast<float>(state.panX + (static_cast<double>(tile.x) * tileSize)),
                             canvasOrigin.y + static_cast<float>(state.panY + (static_cast<double>(tile.y) * tileSize)));
        const ImVec2 maximum(minimum.x + static_cast<float>(tileSize), minimum.y + static_cast<float>(tileSize));
        const ImU32 highlight = currentTile ? IM_COL32(255, 255, 255, 210) : IM_COL32(255, 210, 60, 230);
        drawList->AddRect(minimum, maximum, highlight, 0.0F, ImDrawFlags_None, 1.0F);
    }

    const uomul::MapReader * reader = workspace.reader();
    if (reader != nullptr && reader->missingBlocks() > 0U)
    {
        const std::string missingMessage = "Incomplete map: " + std::to_string(reader->missingBlocks()) + " blocks missing";
        drawList->AddText(ImVec2(canvasOrigin.x + static_cast<float>(errorTextLeftPadding),
                                 canvasOrigin.y + static_cast<float>(errorTextTopPadding)),
                          IM_COL32(255, 145, 125, 255),
                          missingMessage.c_str());
    }
    if (visibleReadError)
    {
        const std::string errorMessage = "Map read error: " + visibleReadError->message;
        drawList->AddText(ImVec2(canvasOrigin.x + static_cast<float>(errorTextLeftPadding),
                                 canvasOrigin.y + static_cast<float>(readErrorTextOffset)),
                          IM_COL32(255, 120, 120, 255),
                          errorMessage.c_str());
    }
    else if (workspace.error())
    {
        const std::string errorMessage = "Map error: " + workspace.error()->message;
        drawList->AddText(ImVec2(canvasOrigin.x + static_cast<float>(errorTextLeftPadding),
                                 canvasOrigin.y + static_cast<float>(errorTextTopPadding)),
                          IM_COL32(255, 120, 120, 255),
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
        ImGui::TextColored(ImVec4(1.0F, static_cast<float>(editBadgeGreen), static_cast<float>(editBadgeBlue), 1.0F),
                           "EDIT MODE");
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
    state.cacheClock = 0;
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
    }
    state.dragStart.reset();
    state.dragEnd.reset();
}

void drawEmptyCanvas(ImVec2 canvasOrigin, ImVec2 canvasMaximum, const MapWorkspace & workspace)
{
    ImDrawList * drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(canvasOrigin, canvasMaximum, IM_COL32(25, 28, 32, 255));
    drawList->AddText(
        ImVec2(canvasOrigin.x + static_cast<float>(noMapTextPadding), canvasOrigin.y + static_cast<float>(noMapTextPadding)),
        IM_COL32(210, 215, 220, 255),
        "No map open");
    if (workspace.error())
    {
        const std::string errorMessage = "Map error: " + workspace.error()->message;
        drawList->AddText(ImVec2(canvasOrigin.x + static_cast<float>(errorTextLeftPadding),
                                 canvasOrigin.y + static_cast<float>(readErrorTextOffset)),
                          IM_COL32(255, 120, 120, 255),
                          errorMessage.c_str());
    }
}

} // namespace

void drawMapCanvasPanel(MapWorkspace & workspace, MapCanvasPanelState & state, bool & visible)
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
            drawCanvasContents(workspace, state, canvasOrigin, canvasSize, currentTile, visibleReadError);
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

} // namespace im_cpp::gui
