#include "gui/selection_stats_panel.h"
#include "gui/map_workspace.h"
#include "gui/theme.h"
#include "uomul/map.h"
#include <expected>
#include <imgui.h>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace uo_mule::gui
{
namespace
{

[[nodiscard]] bool sameTileRegion(const std::optional<uomul::TileRegion> & left,
                                  const std::optional<uomul::TileRegion> & right) noexcept
{
    return left.has_value() == right.has_value() &&
           (!left || (left->min.x == right->min.x && left->min.y == right->min.y &&
                      left->maxExclusive.x == right->maxExclusive.x && left->maxExclusive.y == right->maxExclusive.y));
}

[[nodiscard]] bool sameBlockRegion(const std::optional<uomul::BlockRegion> & left,
                                   const std::optional<uomul::BlockRegion> & right) noexcept
{
    return left.has_value() == right.has_value() &&
           (!left || (left->min.x == right->min.x && left->min.y == right->min.y &&
                      left->maxExclusive.x == right->maxExclusive.x && left->maxExclusive.y == right->maxExclusive.y));
}

[[nodiscard]] bool selectionChanged(const MapWorkspace & workspace, const SelectionStatsPanelState & state) noexcept
{
    return !state.cacheValid || state.mapPath != workspace.mapPath() || state.dimensions != workspace.dimensions() ||
           state.selectionMode != workspace.selectionMode() ||
           !sameTileRegion(state.tileSelection, workspace.tileSelection()) ||
           !sameBlockRegion(state.blockSelection, workspace.blockSelection());
}

void refreshStats(MapWorkspace & workspace, SelectionStatsPanelState & state)
{
    state.mapPath = workspace.mapPath();
    state.dimensions = workspace.dimensions();
    state.selectionMode = workspace.selectionMode();
    state.tileSelection = workspace.tileSelection();
    state.blockSelection = workspace.blockSelection();
    state.stats.reset();
    state.error.reset();
    state.cacheValid = true;
    if (workspace.reader() == nullptr ||
        (state.selectionMode == MapSelectionMode::tiles ? !state.tileSelection : !state.blockSelection))
    {
        return;
    }

    std::expected<std::optional<uomul::MapStats>, uomul::Error> result = workspace.selectionStats();

    if (result)
    {
        state.stats = std::move(*result);
    }
    else
    {
        state.error = result.error();
    }
}

void drawDistribution(const char * label, const std::vector<uomul::ValueStats> & values)
{
    ImGui::TextUnformatted(label);
    if (values.empty())
    {
        ImGui::TextUnformatted("No samples");
        return;
    }

    for (const uomul::ValueStats & value : values)
    {
        ImGui::Text("%d: %llu (%.2f%%)", value.value, static_cast<unsigned long long>(value.count), value.percentage);
    }
}

void drawStats(const uomul::MapStats & stats)
{
    ImGui::Text("Tile count: %llu", static_cast<unsigned long long>(stats.tileCount));
    if (stats.tileCount == 0U)
    {
        ImGui::TextUnformatted("No samples in selection");
        return;
    }

    if (stats.averageZ)
    {
        ImGui::Text("Average Z: %.2f", *stats.averageZ);
    }

    if (stats.medianZ)
    {
        ImGui::Text("Median Z: %.2f", *stats.medianZ);
    }

    drawDistribution("Tile ID distribution", stats.tileIds);
    drawDistribution("Elevation distribution", stats.zs);
}

} // namespace

void drawSelectionStatsPanel(MapWorkspace & workspace, SelectionStatsPanelState & state, bool & visible)
{
    if (selectionChanged(workspace, state))
    {
        refreshStats(workspace, state);
    }

    const bool contentsVisible = ImGui::Begin("Selection Statistics", &visible);
    if (contentsVisible)
    {
        if (workspace.reader() == nullptr)
        {
            ImGui::TextUnformatted("No map open");
        }
        else if (state.error)
        {
            ImGui::TextColored(theme::imguiColor(theme::coral), "Statistics error: %s", state.error->message.c_str());
        }
        else if (!state.stats)
        {
            ImGui::TextUnformatted("No selection");
        }
        else
        {
            drawStats(*state.stats);
        }
    }

    ImGui::End();
}

} // namespace uo_mule::gui
