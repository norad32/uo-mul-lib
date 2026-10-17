#pragma once

#include "gui/map_workspace.h"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace im_cpp::gui
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
    std::uint64_t cacheClock = 0;
    std::filesystem::path documentPath;
    std::optional<std::filesystem::path> workingPath;
    uomul::Dimensions documentDimensions{};
    bool hasDocument = false;
    bool panning = false;
};

void drawMapCanvasPanel(MapWorkspace & workspace, MapCanvasPanelState & state, bool & visible);

} // namespace im_cpp::gui
