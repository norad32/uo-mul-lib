#pragma once

#include "gui_model/documents/map_document_state.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/providers/data_providers.h"
#include "gui_model/world/map_canvas_state.h"
#include "gui_model/world/map_context.h"
#include "uomul/map/map.h"
#include "uomul/mul_common/mul_common.h"
#include <array>
#include <filesystem>
#include <functional>
#include <optional>
#include <unordered_map>

namespace uo_mule::gui
{

struct BlockInspectorPanelState
{
    BlockInspectorPanelState() = default;
    ~BlockInspectorPanelState();
    BlockInspectorPanelState(const BlockInspectorPanelState &) = delete;
    BlockInspectorPanelState & operator=(const BlockInspectorPanelState &) = delete;
    BlockInspectorPanelState(BlockInspectorPanelState &&) = delete;
    BlockInspectorPanelState & operator=(BlockInspectorPanelState &&) = delete;

    std::filesystem::path mapPath;
    std::optional<uomul::BlockCoord> blockCoordinate;
    std::optional<uomul::MapBlock> block;
    std::optional<uomul::Error> error;
    std::unordered_map<std::uint32_t, MapCanvasCachedTexture> textureCache;
    std::uint64_t textureProviderRevision = 0;
    MapViewTileSource textureSource = MapViewTileSource::textures;
    std::function<void(std::uint32_t)> releaseTexture;
    bool cacheValid = false;
};

void drawBlockInspectorPanel(MapDocumentState & mapDocument,
                             const MapContextSnapshot & context,
                             const DataProviders & providers,
                             MapViewTileSource tileSource,
                             BlockInspectorPanelState & state,
                             bool & visible);

} // namespace uo_mule::gui
