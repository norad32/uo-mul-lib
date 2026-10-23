#pragma once

#include "gui_model/tools/tool_contracts.h"

namespace uo_mule::gui
{

class ToolState
{
public:
    [[nodiscard]] ToolId activeTool() const noexcept;
    [[nodiscard]] ToolSelectionGranularity selectionGranularity() const noexcept;

    [[nodiscard]] ToolStateChange activate(ToolId tool,
                                            const ToolContext & context,
                                            const ToolRegistry & registry);
    [[nodiscard]] ToolStateChange setSelectionGranularity(ToolSelectionGranularity granularity,
                                                            const ToolContext & context,
                                                            const ToolRegistry & registry);

private:
    ToolId activeTool_ = ToolId::move;
    ToolSelectionGranularity selectionGranularity_ = ToolSelectionGranularity::tiles;
};

} // namespace uo_mule::gui
