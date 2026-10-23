#pragma once

#include "gui_model/tools/tool_contracts.h"
#include "uomul/map/map.h"
#include <cstddef>
#include <cstdint>
#include <unordered_set>

namespace uo_mule::gui
{

enum class SelectionCombineMode : std::uint8_t
{
    replace,
    add,
    subtract,
};

class MapSelectionMask
{
public:
    [[nodiscard]] ToolSelectionGranularity granularity() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool contains(uomul::TileCoord coordinate) const noexcept;
    [[nodiscard]] bool contains(uomul::BlockCoord coordinate) const noexcept;

    void clear() noexcept;
    void setGranularity(ToolSelectionGranularity granularity);
    void replaceTileRegion(uomul::TileRegion region, uomul::Dimensions dimensions);
    void replaceBlockRegion(uomul::BlockRegion region, uomul::Dimensions dimensions);
    void selectAll(uomul::Dimensions dimensions);
    void invert(uomul::Dimensions dimensions);
    void add(const MapSelectionMask & other);
    void subtract(const MapSelectionMask & other);
    void convertTilesToCoveringBlocks(uomul::Dimensions dimensions);

private:
    [[nodiscard]] std::uint64_t tileKey(uomul::TileCoord coordinate) const noexcept;
    [[nodiscard]] std::uint64_t blockKey(uomul::BlockCoord coordinate) const noexcept;

    ToolSelectionGranularity granularity_ = ToolSelectionGranularity::tiles;
    std::unordered_set<std::uint64_t> selected_;
};

} // namespace uo_mule::gui
