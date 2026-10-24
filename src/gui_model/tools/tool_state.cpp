#include "gui_model/tools/tool_state.h"
#include <limits>
#include <utility>

namespace uo_mule::gui
{

namespace
{

ToolStateChange setTileId(uomul::MapTile & tile, std::int32_t id) noexcept
{
    if (id < 0 || id > std::numeric_limits<std::uint16_t>::max())
    {
        return {.accepted = false, .changed = false, .reason = "Tile ID must be between 0 and 65535"};
    }

    const auto next = static_cast<std::uint16_t>(id);
    const bool changed = tile.id != next;
    tile.id = next;
    return {.accepted = true, .changed = changed, .reason = {}};
}

ToolStateChange setTileZ(uomul::MapTile & tile, std::int32_t z) noexcept
{
    if (z < std::numeric_limits<std::int8_t>::min() || z > std::numeric_limits<std::int8_t>::max())
    {
        return {.accepted = false, .changed = false, .reason = "Tile Z must be between -128 and 127"};
    }

    const auto next = static_cast<std::int8_t>(z);
    const bool changed = tile.z != next;
    tile.z = next;
    return {.accepted = true, .changed = changed, .reason = {}};
}

} // namespace

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

FillMode ToolState::fillMode() const noexcept
{
    return fillMode_;
}

ReplaceMode ToolState::replaceMode() const noexcept
{
    return replaceMode_;
}

const uomul::MapTile & ToolState::replaceSource() const noexcept
{
    return replaceSource_;
}

const uomul::MapTile & ToolState::replaceTarget() const noexcept
{
    return replaceTarget_;
}

const FuzzySelectionOptions & ToolState::fuzzyOptions() const noexcept
{
    return fuzzyOptions_;
}

bool ToolState::tilePickRequested() const noexcept
{
    return tilePickRequested_;
}

TilePickTarget ToolState::tilePickTarget() const noexcept
{
    return tilePickTarget_;
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
    tilePickTarget_ = TilePickTarget::activeTile;
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

void ToolState::setFillMode(FillMode mode) noexcept
{
    fillMode_ = mode;
}

void ToolState::setReplaceMode(ReplaceMode mode) noexcept
{
    replaceMode_ = mode;
}

ToolStateChange ToolState::setReplaceSourceId(std::int32_t id) noexcept
{
    return setTileId(replaceSource_, id);
}

ToolStateChange ToolState::setReplaceSourceZ(std::int32_t z) noexcept
{
    return setTileZ(replaceSource_, z);
}

ToolStateChange ToolState::setReplaceTargetId(std::int32_t id) noexcept
{
    return setTileId(replaceTarget_, id);
}

ToolStateChange ToolState::setReplaceTargetZ(std::int32_t z) noexcept
{
    return setTileZ(replaceTarget_, z);
}

void ToolState::setFuzzyScope(FuzzyScope scope) noexcept
{
    fuzzyOptions_.scope = scope;
}

void ToolState::setFuzzyIdEnabled(bool enabled) noexcept
{
    fuzzyOptions_.idEnabled = enabled;
}

ToolStateChange ToolState::setFuzzyId(std::int32_t id) noexcept
{
    if (id < 0 || id > std::numeric_limits<std::uint16_t>::max())
    {
        return {.accepted = false, .changed = false, .reason = "Tile ID must be between 0 and 65535"};
    }
    const auto next = static_cast<std::uint16_t>(id);
    const bool changed = fuzzyOptions_.id != next;
    fuzzyOptions_.id = next;
    return {.accepted = true, .changed = changed, .reason = {}};
}

void ToolState::setFuzzyZEnabled(bool enabled) noexcept
{
    fuzzyOptions_.zEnabled = enabled;
}

ToolStateChange ToolState::setFuzzyZ(std::int32_t z) noexcept
{
    if (z < std::numeric_limits<std::int8_t>::min() || z > std::numeric_limits<std::int8_t>::max())
    {
        return {.accepted = false, .changed = false, .reason = "Tile Z must be between -128 and 127"};
    }
    const auto next = static_cast<std::int8_t>(z);
    const bool changed = fuzzyOptions_.z != next;
    fuzzyOptions_.z = next;
    return {.accepted = true, .changed = changed, .reason = {}};
}

void ToolState::setFuzzyNameEnabled(bool enabled) noexcept
{
    fuzzyOptions_.nameEnabled = enabled;
}

void ToolState::setFuzzyName(std::string name)
{
    fuzzyOptions_.name = std::move(name);
}

void ToolState::setPickedTileName(std::string name)
{
    fuzzyOptions_.name = std::move(name);
    fuzzyOptions_.nameEnabled = true;
    tilePickRequested_ = false;
    tilePickTarget_ = TilePickTarget::activeTile;
}

ToolStateChange ToolState::setReplaceTargetFromActiveTile() noexcept
{
    if (!activeTile_)
    {
        return {.accepted = false, .changed = false, .reason = "No active tile is available"};
    }

    const bool changed = replaceTarget_ != *activeTile_;
    replaceTarget_ = *activeTile_;
    return {.accepted = true, .changed = changed, .reason = {}};
}

ToolStateChange ToolState::setReplaceSourceFromActiveTile() noexcept
{
    if (!activeTile_)
    {
        return {.accepted = false, .changed = false, .reason = "No active tile is available"};
    }

    const bool changed = replaceSource_ != *activeTile_;
    replaceSource_ = *activeTile_;
    return {.accepted = true, .changed = changed, .reason = {}};
}

void ToolState::setPickedTile(uomul::MapTile tile) noexcept
{
    switch (tilePickTarget_)
    {
    case TilePickTarget::activeTile:
        activeTile_ = tile;
        break;
    case TilePickTarget::replaceSource:
        replaceSource_ = tile;
        break;
    case TilePickTarget::replaceTarget:
        replaceTarget_ = tile;
        break;
    case TilePickTarget::fuzzyId:
        fuzzyOptions_.id = tile.id;
        fuzzyOptions_.idEnabled = true;
        break;
    case TilePickTarget::fuzzyZ:
        fuzzyOptions_.z = tile.z;
        fuzzyOptions_.zEnabled = true;
        break;
    case TilePickTarget::fuzzyName:
        break;
    }
    tilePickRequested_ = false;
    tilePickTarget_ = TilePickTarget::activeTile;
}

ToolStateChange ToolState::setPickedTileId(std::int32_t id) noexcept
{
    ToolStateChange result;
    switch (tilePickTarget_)
    {
    case TilePickTarget::activeTile:
        result = setActiveTileId(id);
        break;
    case TilePickTarget::replaceSource:
        result = setReplaceSourceId(id);
        break;
    case TilePickTarget::replaceTarget:
        result = setReplaceTargetId(id);
        break;
    case TilePickTarget::fuzzyId:
        result = setFuzzyId(id);
        break;
    case TilePickTarget::fuzzyZ:
        result = {.accepted = false, .changed = false, .reason = "Z picking requires a canvas tile"};
        break;
    case TilePickTarget::fuzzyName:
        result = {.accepted = false, .changed = false, .reason = "Name picking requires Tiledata"};
        break;
    }
    if (result.accepted)
    {
        tilePickRequested_ = false;
        tilePickTarget_ = TilePickTarget::activeTile;
    }
    return result;
}

void ToolState::requestTilePick(TilePickTarget target) noexcept
{
    tilePickRequested_ = true;
    tilePickTarget_ = target;
}

void ToolState::clearTilePickRequest() noexcept
{
    tilePickRequested_ = false;
    tilePickTarget_ = TilePickTarget::activeTile;
}

} // namespace uo_mule::gui
