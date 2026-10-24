#include "gui_model/tools/tool_state.h"
#include <limits>

namespace uo_mule::gui
{

ToolId ToolState::activeTool() const noexcept
{
    return activeTool_;
}

ToolSelectionGranularity ToolState::selectionGranularity() const noexcept
{
    return selectionGranularity_;
}

const std::optional<uomul::MapTile> & ToolState::activeTile() const noexcept
{
    return activeTile_;
}

bool ToolState::tilePickRequested() const noexcept
{
    return tilePickRequested_;
}

ToolStateChange ToolState::activate(ToolId tool, const ToolContext & context, const ToolRegistry & registry)
{
    ToolContext candidateContext = context;
    candidateContext.selectionGranularity = selectionGranularity_;
    const ToolAvailability available = registry.availability(tool, candidateContext);
    if (!available.enabled)
    {
        return {.accepted = false, .changed = false, .reason = available.disabledReason};
    }

    const bool changed = activeTool_ != tool;
    activeTool_ = tool;
    return {.accepted = true, .changed = changed, .reason = {}};
}

ToolStateChange ToolState::setSelectionGranularity(ToolSelectionGranularity granularity,
                                                   const ToolContext & context,
                                                   const ToolRegistry & registry)
{
    if (activeTool_ == ToolId::fuzzySelect && granularity == ToolSelectionGranularity::blocks)
    {
        return {
            .accepted = false,
            .changed = false,
            .reason = "Fuzzy Select supports tiles only",
        };
    }

    ToolContext candidateContext = context;
    candidateContext.selectionGranularity = granularity;
    const ToolAvailability available = registry.availability(activeTool_, candidateContext);
    if (!available.enabled && context.mapDocumentOpen)
    {
        return {.accepted = false, .changed = false, .reason = available.disabledReason};
    }

    const bool changed = selectionGranularity_ != granularity;
    selectionGranularity_ = granularity;
    return {.accepted = true, .changed = changed, .reason = {}};
}

void ToolState::setActiveTile(uomul::MapTile tile) noexcept
{
    activeTile_ = tile;
    tilePickRequested_ = false;
}

ToolStateChange ToolState::setActiveTileId(std::int32_t id) noexcept
{
    if (id < 0 || id > std::numeric_limits<std::uint16_t>::max())
    {
        return {.accepted = false, .changed = false, .reason = "Tile ID must be between 0 and 65535"};
    }

    const uomul::MapTile previous = activeTile_.value_or(uomul::MapTile{});
    const uomul::MapTile next{.id = static_cast<std::uint16_t>(id), .z = previous.z};
    const bool changed = !activeTile_ || previous != next;
    activeTile_ = next;
    return {.accepted = true, .changed = changed, .reason = {}};
}

ToolStateChange ToolState::setActiveTileZ(std::int32_t z) noexcept
{
    if (z < std::numeric_limits<std::int8_t>::min() || z > std::numeric_limits<std::int8_t>::max())
    {
        return {.accepted = false, .changed = false, .reason = "Tile Z must be between -128 and 127"};
    }

    const uomul::MapTile previous = activeTile_.value_or(uomul::MapTile{});
    const uomul::MapTile next{.id = previous.id, .z = static_cast<std::int8_t>(z)};
    const bool changed = !activeTile_ || previous != next;
    activeTile_ = next;
    return {.accepted = true, .changed = changed, .reason = {}};
}

void ToolState::requestTilePick() noexcept
{
    tilePickRequested_ = true;
}

void ToolState::clearTilePickRequest() noexcept
{
    tilePickRequested_ = false;
}

} // namespace uo_mule::gui
