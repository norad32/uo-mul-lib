#pragma once

#include "gui/map_workspace.h"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <vector>

namespace uo_mule::gui
{

inline constexpr double defaultMapCanvasZoom = 12.0;

struct MapCanvasCachedBlock
{
    uomul::BlockCoord coordinate{};
    std::optional<uomul::MapBlock> block;
    std::optional<uomul::Error> error;
    std::uint64_t lastUsed = 0;
};

struct MapCanvasPanelState
{
    MapCanvasPanelState() = default;
    ~MapCanvasPanelState();
    MapCanvasPanelState(const MapCanvasPanelState &) = delete;
    MapCanvasPanelState & operator=(const MapCanvasPanelState &) = delete;
    MapCanvasPanelState(MapCanvasPanelState &&) = delete;
    MapCanvasPanelState & operator=(MapCanvasPanelState &&) = delete;

    double panX = 0.0;
    double panY = 0.0;
    double zoom = defaultMapCanvasZoom;
    double panStartMouseX = 0.0;
    double panStartMouseY = 0.0;
    double panStartX = 0.0;
    double panStartY = 0.0;
    std::optional<uomul::TileCoord> dragStart;
    std::optional<uomul::TileCoord> dragEnd;
    std::vector<MapCanvasCachedBlock> blockCache;
    std::unordered_map<std::uint64_t, std::size_t> blockCacheIndex;
    std::uint64_t cacheClock = 0;
    std::vector<std::uint8_t> lodPixels;
    std::uint32_t lodTexture = 0;
    std::uint32_t lodTextureWidth = 0;
    std::uint32_t lodTextureHeight = 0;
    float lodCanvasWidth = 0.0F;
    float lodCanvasHeight = 0.0F;
    float lodScaleX = 0.0F;
    float lodScaleY = 0.0F;
    double lodPanX = 0.0;
    double lodPanY = 0.0;
    double lodZoom = 0.0;
    MapDisplayMode lodDisplayMode = MapDisplayMode::tileIds;
    std::optional<uomul::Error> lodReadError;
    bool lodCacheValid = false;
    std::filesystem::path documentPath;
    std::optional<std::filesystem::path> workingPath;
    uomul::Dimensions documentDimensions{};
    bool hasDocument = false;
    bool panning = false;
};

void drawMapCanvasPanel(MapWorkspace & workspace, MapCanvasPanelState & state, bool & visible);

} // namespace uo_mule::gui
