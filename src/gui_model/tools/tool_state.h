#pragma once

#include "gui_model/tools/tool_contracts.h"
#include "uomul/map/map.h"
#include <optional>

namespace uo_mule::gui
{

class ToolState
{
public:
    [[nodiscard]] ToolId activeTool() const noexcept;
    [[nodiscard]] ToolSelectionGranularity selectionGranularity() const noexcept;
    [[nodiscard]] const std::optional<uomul::MapTile> & activeTile() const noexcept;
    [[nodiscard]] FillMode fillMode() const noexcept;
    [[nodiscard]] ReplaceMode replaceMode() const noexcept;
    [[nodiscard]] bool fillIdEnabled() const noexcept;
    [[nodiscard]] bool fillZEnabled() const noexcept;
    [[nodiscard]] bool replaceIdEnabled() const noexcept;
    [[nodiscard]] bool replaceZEnabled() const noexcept;
    [[nodiscard]] const uomul::MapTile & replaceSource() const noexcept;
    [[nodiscard]] const FuzzySelectionOptions & fuzzyOptions() const noexcept;
    [[nodiscard]] const uomul::MapTile & replaceTarget() const noexcept;
    [[nodiscard]] bool tilePickRequested() const noexcept;
    [[nodiscard]] TilePickTarget tilePickTarget() const noexcept;

    [[nodiscard]] ToolStateChange activate(ToolId tool,
                                            const ToolContext & context,
                                            const ToolRegistry & registry);
    [[nodiscard]] ToolStateChange setSelectionGranularity(ToolSelectionGranularity granularity,
                                                            const ToolContext & context,
                                                            const ToolRegistry & registry);
    void setActiveTile(uomul::MapTile tile) noexcept;
    [[nodiscard]] ToolStateChange setActiveTileId(std::int32_t id) noexcept;
    [[nodiscard]] ToolStateChange setActiveTileZ(std::int32_t z) noexcept;
    void setFillMode(FillMode mode) noexcept;
    void setReplaceMode(ReplaceMode mode) noexcept;
    [[nodiscard]] ToolStateChange setFillIdEnabled(bool enabled) noexcept;
    [[nodiscard]] ToolStateChange setFillZEnabled(bool enabled) noexcept;
    [[nodiscard]] ToolStateChange setReplaceIdEnabled(bool enabled) noexcept;
    [[nodiscard]] ToolStateChange setReplaceZEnabled(bool enabled) noexcept;
    [[nodiscard]] ToolStateChange setReplaceSourceId(std::int32_t id) noexcept;
    [[nodiscard]] ToolStateChange setReplaceSourceZ(std::int32_t z) noexcept;
    [[nodiscard]] ToolStateChange setReplaceTargetId(std::int32_t id) noexcept;
    [[nodiscard]] ToolStateChange setReplaceTargetZ(std::int32_t z) noexcept;
    void setFuzzyScope(FuzzyScope scope) noexcept;
    void setFuzzyIdEnabled(bool enabled) noexcept;
    [[nodiscard]] ToolStateChange setFuzzyId(std::int32_t id) noexcept;
    void setFuzzyZEnabled(bool enabled) noexcept;
    [[nodiscard]] ToolStateChange setFuzzyZ(std::int32_t z) noexcept;
    void setFuzzyNameEnabled(bool enabled) noexcept;
    void setFuzzyName(std::string name);
    void setPickedTileName(std::string name);
    [[nodiscard]] ToolStateChange setReplaceSourceFromActiveTile() noexcept;
    [[nodiscard]] ToolStateChange setReplaceTargetFromActiveTile() noexcept;
    void setPickedTile(uomul::MapTile tile) noexcept;
    [[nodiscard]] ToolStateChange setPickedTileId(std::int32_t id) noexcept;
    void requestTilePick(TilePickTarget target = TilePickTarget::activeTile) noexcept;
    void clearTilePickRequest() noexcept;

private:
    ToolId activeTool_ = ToolId::move;
    ToolSelectionGranularity selectionGranularity_ = ToolSelectionGranularity::tiles;
    std::optional<uomul::MapTile> activeTile_;
    FillMode fillMode_ = FillMode::idAndZ;
    ReplaceMode replaceMode_ = ReplaceMode::idAndZ;
    bool fillIdEnabled_ = true;
    bool fillZEnabled_ = true;
    bool replaceIdEnabled_ = true;
    bool replaceZEnabled_ = true;
    uomul::MapTile replaceSource_{};
    uomul::MapTile replaceTarget_{};
    FuzzySelectionOptions fuzzyOptions_{};
    bool tilePickRequested_ = false;
    TilePickTarget tilePickTarget_ = TilePickTarget::activeTile;
};

} // namespace uo_mule::gui
