#pragma once

#include "gui/context/map_context.h"
#include "gui/documents/map_workspace.h"
#include "gui/providers/data_providers.h"
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
