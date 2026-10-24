#pragma once

#include "gui_model/tools/tool_contracts.h"
#include "uomul/map/map.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_set>
#include <vector>

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
    void forEachSelectedTile(const std::function<void(uomul::TileCoord)> & callback) const;
    void forEachSelectedBlock(const std::function<void(uomul::BlockCoord)> & callback) const;

    void clear() noexcept;
    void selectTile(uomul::TileCoord coordinate, uomul::Dimensions dimensions);
    void setGranularity(ToolSelectionGranularity granularity);
    void replaceTileRegion(uomul::TileRegion region, uomul::Dimensions dimensions);
    void replaceBlockRegion(uomul::BlockRegion region, uomul::Dimensions dimensions);
    void replaceLasso(const std::vector<uomul::TileCoord> & path, uomul::Dimensions dimensions);
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

[[nodiscard]] bool selectionCellHasExposedEdge(const MapSelectionMask & mask,
                                               std::int32_t x,
                                               std::int32_t y,
                                               std::int32_t dx,
                                               std::int32_t dy,
                                               uomul::Dimensions dimensions) noexcept;

} // namespace uo_mule::gui
