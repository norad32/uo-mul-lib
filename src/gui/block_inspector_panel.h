#pragma once

#include <filesystem>
#include <optional>

#include "gui/map_workspace.h"

namespace im_cpp::gui
{

struct BlockInspectorPanelState
{
    std::filesystem::path mapPath;
    std::optional<uomul::BlockCoord> blockCoordinate;
    std::optional<uomul::MapBlock> block;
    std::optional<uomul::Error> error;
    bool cacheValid = false;
};

void drawBlockInspectorPanel(MapWorkspace & workspace, BlockInspectorPanelState & state, bool & visible);

} // namespace im_cpp::gui
