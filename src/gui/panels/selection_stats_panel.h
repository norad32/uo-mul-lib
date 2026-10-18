#pragma once

#include "gui/map_workspace.h"
#include "uomul/map.h"
#include "uomul/mul_common/mul_common.h"
#include <filesystem>
#include <optional>

namespace uo_mule::gui
{

struct SelectionStatsPanelState
{
    std::filesystem::path mapPath;
    uomul::Dimensions dimensions{};
    MapSelectionMode selectionMode = MapSelectionMode::tiles;
    std::optional<uomul::TileRegion> tileSelection;
    std::optional<uomul::BlockRegion> blockSelection;
    std::optional<uomul::MapStats> stats;
    std::optional<uomul::Error> error;
    bool cacheValid = false;
};

void drawSelectionStatsPanel(MapWorkspace & workspace, SelectionStatsPanelState & state, bool & visible);

} // namespace uo_mule::gui
