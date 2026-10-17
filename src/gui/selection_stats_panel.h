#pragma once

#include <filesystem>
#include <optional>

#include "gui/map_workspace.h"

namespace im_cpp::gui
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

} // namespace im_cpp::gui
