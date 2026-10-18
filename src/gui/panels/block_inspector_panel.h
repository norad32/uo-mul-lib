#pragma once

#include "gui/map_workspace.h"
#include "uomul/map.h"
#include "uomul/mul_common/mul_common.h"
#include <filesystem>
#include <optional>

namespace uo_mule::gui
{

struct RadarcolPanelState;

struct BlockInspectorPanelState
{
    std::filesystem::path mapPath;
    std::optional<uomul::BlockCoord> blockCoordinate;
    std::optional<uomul::MapBlock> block;
    std::optional<uomul::Error> error;
    bool cacheValid = false;
};

void drawBlockInspectorPanel(MapWorkspace & workspace,
                             RadarcolPanelState & radarcolState,
                             BlockInspectorPanelState & state,
                             bool & visible);

} // namespace uo_mule::gui
