#pragma once

#include "gui/providers/data_providers.h"
#include "gui/workspaces/map_workspace.h"
#include "gui/world/map_context.h"
#include "uomul/map.h"
#include "uomul/mul_common/mul_common.h"
#include <filesystem>
#include <optional>

namespace uo_mule::gui
{

struct BlockInspectorPanelState
{
    std::filesystem::path mapPath;
    std::optional<uomul::BlockCoord> blockCoordinate;
    std::optional<uomul::MapBlock> block;
    std::optional<uomul::Error> error;
    bool cacheValid = false;
};

void drawBlockInspectorPanel(MapWorkspace & workspace,
                             const MapContextSnapshot & context,
                             const DataProviders & providers,
                             BlockInspectorPanelState & state,
                             bool & visible);

} // namespace uo_mule::gui
