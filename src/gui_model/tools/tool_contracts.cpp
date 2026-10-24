#include "gui_model/tools/tool_contracts.h"
#include <utility>

namespace uo_mule::gui
{
namespace
{

ToolAvailability enabled()
{
    return {.enabled = true, .disabledReason = {}};
}

ToolAvailability requiresMap(const ToolContext & context)
{
    if (!context.mapDocumentOpen)
    {
        return {.enabled = false, .disabledReason = "No map is open"};
    }
    return enabled();
}

ToolAvailability requiresEditableMap(const ToolContext & context)
{
    if (!context.mapDocumentOpen)
    {
        return {.enabled = false, .disabledReason = "No map is open"};
    }
    if (!context.mapEditable)
    {
        return {.enabled = false, .disabledReason = "The active map is not editable"};
    }
    return enabled();
}

} // namespace

ToolAvailability ToolDescriptor::availability(const ToolContext & context) const
{
    switch (id)
    {
    case ToolId::move:
    case ToolId::rectangleSelect:
    case ToolId::lassoSelect:
        return requiresMap(context);
    case ToolId::fuzzySelect:
        if (const ToolAvailability map = requiresMap(context); !map.enabled)
        {
            return map;
        }
        if (context.selectionGranularity == ToolSelectionGranularity::blocks)
        {
            return {.enabled = false, .disabledReason = "Fuzzy Select supports tiles only"};
        }
        return enabled();
    case ToolId::fill:
    case ToolId::replace:
        return requiresEditableMap(context);
    }

    return {.enabled = false, .disabledReason = "Unknown tool"};
}

ToolRegistry::ToolRegistry()
{
    registerDefaultTools(*this);
}

void ToolRegistry::registerTool(ToolDescriptor descriptor)
{
    tools_.push_back(std::move(descriptor));
}

const ToolDescriptor * ToolRegistry::find(ToolId id) const noexcept
{
    for (const ToolDescriptor & tool : tools_)
    {
        if (tool.id == id)
        {
            return &tool;
        }
    }
    return nullptr;
}

const ToolDescriptor * ToolRegistry::find(std::string_view stableId) const noexcept
{
    for (const ToolDescriptor & tool : tools_)
    {
        if (tool.stableId == stableId)
        {
            return &tool;
        }
    }
    return nullptr;
}

ToolAvailability ToolRegistry::availability(ToolId id, const ToolContext & context) const
{
    const ToolDescriptor * tool = find(id);
    if (tool == nullptr)
    {
        return {.enabled = false, .disabledReason = "Tool is not registered"};
    }
    return tool->availability(context);
}

const std::vector<ToolDescriptor> & ToolRegistry::tools() const noexcept
{
    return tools_;
}

void registerDefaultTools(ToolRegistry & registry)
{
    registry.registerTool({
        .id = ToolId::move,
        .stableId = "tool.move",
        .label = "Pan",
        .category = ToolCategory::navigation,
        .description = "Pan the world view",
        .defaultShortcut = "P",
    });
    registry.registerTool({
        .id = ToolId::rectangleSelect,
        .stableId = "tool.rectangle-select",
        .label = "Rectangle Select",
        .category = ToolCategory::selection,
        .description = "Select a rectangular tile or block area",
        .defaultShortcut = "R",
    });
    registry.registerTool({
        .id = ToolId::lassoSelect,
        .stableId = "tool.lasso-select",
        .label = "Free Select",
        .category = ToolCategory::selection,
        .description = "Select a freehand tile or block area",
        .defaultShortcut = "F",
    });
    registry.registerTool({
        .id = ToolId::fuzzySelect,
        .stableId = "tool.fuzzy-select",
        .label = "Fuzzy Select",
        .category = ToolCategory::selection,
        .description = "Select matching connected or global map tiles",
        .defaultShortcut = "U",
    });
    registry.registerTool({
        .id = ToolId::fill,
        .stableId = "tool.fill",
        .label = "Fill",
        .category = ToolCategory::editing,
        .description = "Fill the current selection from the active tile",
        .defaultShortcut = "Shift+B",
    });
    registry.registerTool({
        .id = ToolId::replace,
        .stableId = "tool.replace",
        .label = "Replace",
        .category = ToolCategory::editing,
        .description = "Replace matching IDs or elevations in the selection",
        .defaultShortcut = "Shift+R",
    });
}

} // namespace uo_mule::gui
