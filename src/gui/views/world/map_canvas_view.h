#pragma once

#include "gui_model/providers/data_providers.h"
#include "gui_model/tools/tool_state.h"
#include "gui_model/world/map_canvas_state.h"
#include "uomul/map/map.h"
#include "uomul/mul_common/mul_common.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace uo_mule::gui
{

class MapWorkspaceManager;

[[nodiscard]] MapColor
mapCanvasTileColor(MapDisplayMode mode, uomul::MapTile tile, const RadarColorProvider & radarColors) noexcept;

void drawMapCanvasView(MapWorkspaceManager & workspace,
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
                       const std::function<bool(uomul::MapTile)> & colorSourcePicked,
                       const std::function<void(const MapCanvasStatus &)> & updateLastHoveredCanvas,
                       bool & visible);

} // namespace uo_mule::gui
