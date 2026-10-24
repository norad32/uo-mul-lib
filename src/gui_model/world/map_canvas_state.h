#pragma once

#include "gui_model/input/coordinate_parser.h"
#include "gui_model/workspaces/map_workspace.h"
#include "gui_model/world/map_color.h"
#include "gui_model/persistence/preferences.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

namespace uo_mule::gui
{

inline constexpr double defaultMapCanvasZoom = 12.0;
inline constexpr std::size_t coordinateInputBufferSize = 64;

struct MapCanvasCachedBlock
{
    uomul::BlockCoord coordinate{};
    std::optional<uomul::MapBlock> block;
    std::optional<uomul::Error> error;
    std::uint64_t lastUsed = 0;
};

struct MapCanvasCachedTexture
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<MapColor> pixels;
    std::vector<std::uint8_t> rgbaPixels;
    std::uint32_t gpuTexture = 0;
};

struct MapCanvasStatus
{
    std::string canvasId = "Map Canvas";
    bool hasDocument = false;
    std::optional<uomul::TileCoord> tile;
    double zoom = defaultMapCanvasZoom;
};

enum class MapCanvasViewRequest : std::uint8_t
{
    none,
    fitMap,
    fitSelection,
    centerSelection,
    resetView,
};

struct MapCanvasNavigationRequest
{
    MapCanvasViewRequest request = MapCanvasViewRequest::none;
    std::optional<uomul::TileCoord> coordinate;
};

struct MapCanvasViewState
{
    MapCanvasViewState() = default;
    ~MapCanvasViewState()
    {
        if (releaseTexture)
        {
            if (lodTexture != 0U)
            {
                releaseTexture(lodTexture);
            }
            for (const auto & [id, texture] : textureCache)
            {
                static_cast<void>(id);
                if (texture.gpuTexture != 0U)
                {
                    releaseTexture(texture.gpuTexture);
                }
            }
        }
    }
    MapCanvasViewState(const MapCanvasViewState &) = delete;
    MapCanvasViewState & operator=(const MapCanvasViewState &) = delete;
    MapCanvasViewState(MapCanvasViewState &&) = delete;
    MapCanvasViewState & operator=(MapCanvasViewState &&) = delete;

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
    std::unordered_map<std::uint32_t, MapCanvasCachedTexture> textureCache;
    std::uint64_t textureProviderRevision = 0;
    std::function<void(std::uint32_t)> releaseTexture;
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
    std::uint64_t lodTextureRevision = 0;
    std::uint64_t lodRadarColorRevision = 0;
    MapViewTileSource lodTileSource = MapViewTileSource::textures;
    std::optional<uomul::Error> lodReadError;
    bool lodCacheValid = false;
    std::filesystem::path documentPath;
    std::optional<std::filesystem::path> workingPath;
    uomul::Dimensions documentDimensions{};
    bool hasDocument = false;
    bool panning = false;
    bool scrollResetPending = false;
    MapCanvasNavigationRequest navigationRequest;
    bool coordinatePopupRequested = false;
    bool coordinateDialogVisible = false;
    CoordinateKind coordinateInputKind = CoordinateKind::tile;
    bool coordinateInputFocusRequested = false;
    std::array<char, coordinateInputBufferSize> coordinateInput{};
};

} // namespace uo_mule::gui
