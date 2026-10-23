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
    [[nodiscard]] bool tilePickRequested() const noexcept;

    [[nodiscard]] ToolStateChange activate(ToolId tool,
                                            const ToolContext & context,
                                            const ToolRegistry & registry);
    [[nodiscard]] ToolStateChange setSelectionGranularity(ToolSelectionGranularity granularity,
                                                            const ToolContext & context,
                                                            const ToolRegistry & registry);
    void setActiveTile(uomul::MapTile tile) noexcept;
    void requestTilePick() noexcept;
    void clearTilePickRequest() noexcept;

private:
    ToolId activeTool_ = ToolId::move;
    ToolSelectionGranularity selectionGranularity_ = ToolSelectionGranularity::tiles;
    std::optional<uomul::MapTile> activeTile_;
    bool tilePickRequested_ = false;
};

} // namespace uo_mule::gui
