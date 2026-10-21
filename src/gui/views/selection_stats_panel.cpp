#include "gui/views/selection_stats_panel.h"
#include "gui/context/map_context.h"
#include "gui/documents/map_workspace.h"
#include "gui/frontend/theme.h"
#include "uomul/map.h"
#include "uomul/mul_common/mul_common.h"
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

[[nodiscard]] bool selectionChanged(const MapWorkspace & workspace,
                                    const MapContextSnapshot & context,
                                    const SelectionStatsPanelState & state) noexcept
{
    return !state.cacheValid || state.mapPath != workspace.mapPath() || state.dimensions != workspace.dimensions() ||
           state.selectionMode != context.selectionMode || !sameTileRegion(state.tileSelection, context.tileSelection) ||
           !sameBlockRegion(state.blockSelection, context.blockSelection);
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
    state.status = SelectionStatsState::loading;
    state.cacheValid = true;
    if (workspace.reader() == nullptr ||
        (state.selectionMode == MapSelectionMode::tiles ? !state.tileSelection : !state.blockSelection))
    {
        state.status = SelectionStatsState::empty;
        return;
    }

    std::expected<std::optional<uomul::MapStats>, uomul::Error> result = workspace.selectionStats();

    if (result)
    {
        state.stats = std::move(*result);
        state.status = state.stats ? SelectionStatsState::ready : SelectionStatsState::empty;
    }
    else
    {
        state.error = result.error();
        state.status = SelectionStatsState::error;
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

void drawSelectionStatsPanel(MapWorkspace & workspace,
                             const MapContextSnapshot & context,
                             SelectionStatsPanelState & state,
                             bool & visible)
{
    if (selectionChanged(workspace, context, state))
    {
        refreshStats(workspace, state);
    }

    const bool contentsVisible = ImGui::Begin("Selection Statistics###view.selection-statistics", &visible);
    if (contentsVisible)
    {
        if (workspace.reader() == nullptr)
        {
            ImGui::TextUnformatted("No map open");
        }
        else if (state.status == SelectionStatsState::error && state.error)
        {
            ImGui::TextColored(theme::imguiColor(theme::red), "Statistics error: %s", state.error->message.c_str());
        }
        else if (state.status == SelectionStatsState::loading)
        {
            ImGui::TextUnformatted("Calculating selection statistics...");
        }
        else if (state.status == SelectionStatsState::empty || !state.stats)
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
