#include "gui_model/tools/tool_contracts.h"
#include <catch2/catch_test_macros.hpp>
#include <set>
#include <string>

using uo_mule::gui::ToolCategory;
using uo_mule::gui::ToolContext;
using uo_mule::gui::ToolId;
using uo_mule::gui::ToolRegistry;
using uo_mule::gui::ToolSelectionGranularity;

TEST_CASE("defaultTools_Should_ExposeStableMetadata")
{
    const ToolRegistry registry;

    REQUIRE(registry.tools().size() == 6);
    REQUIRE(registry.find(ToolId::move)->stableId == "tool.move");
    REQUIRE(registry.find("tool.rectangle-select")->label == "Rectangle Select");
    REQUIRE(registry.find(ToolId::fill)->defaultShortcut == "Shift+B");
    REQUIRE(registry.find(ToolId::replace)->category == ToolCategory::editing);
}

TEST_CASE("defaultTools_Should_HaveUniqueStableIdsAndShortcuts")
{
    const ToolRegistry registry;
    std::set<std::string> stableIds;
    std::set<std::string> shortcuts;

    for (const auto & tool : registry.tools())
    {
        REQUIRE(stableIds.insert(tool.stableId).second);
        REQUIRE(shortcuts.insert(tool.defaultShortcut).second);
        REQUIRE_FALSE(tool.description.empty());
    }
}

TEST_CASE("tools_Should_RequireAnOpenMap")
{
    const ToolRegistry registry;
    const ToolContext context;

    REQUIRE_FALSE(registry.availability(ToolId::move, context).enabled);
    REQUIRE(registry.availability(ToolId::move, context).disabledReason == "No map is open");
    REQUIRE_FALSE(registry.availability(ToolId::fill, context).enabled);
}

TEST_CASE("editingTools_Should_RequireAnEditableMap")
{
    const ToolRegistry registry;
    const ToolContext readOnlyContext{.mapDocumentOpen = true, .mapEditable = false};

    REQUIRE_FALSE(registry.availability(ToolId::fill, readOnlyContext).enabled);
    REQUIRE(registry.availability(ToolId::replace, readOnlyContext).disabledReason ==
            "The active map is not editable");
}

TEST_CASE("fuzzySelect_Should_BeUnavailableForBlockGranularity")
{
    const ToolRegistry registry;
    const ToolContext blockContext{
        .mapDocumentOpen = true,
        .mapEditable = true,
        .selectionGranularity = ToolSelectionGranularity::blocks,
    };

    REQUIRE_FALSE(registry.availability(ToolId::fuzzySelect, blockContext).enabled);
    REQUIRE(registry.availability(ToolId::fuzzySelect, blockContext).disabledReason ==
            "Fuzzy Select supports tiles only");
}

TEST_CASE("selectionTools_Should_BeAvailableForAnOpenMap")
{
    const ToolRegistry registry;
    const ToolContext context{.mapDocumentOpen = true, .mapEditable = false};

    REQUIRE(registry.availability(ToolId::rectangleSelect, context).enabled);
    REQUIRE(registry.availability(ToolId::lassoSelect, context).enabled);
    REQUIRE(registry.availability(ToolId::fuzzySelect, context).enabled);
}
