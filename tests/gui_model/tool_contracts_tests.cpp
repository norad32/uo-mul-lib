#include "gui_model/tools/tool_contracts.h"
#include "gui_model/tools/tool_state.h"
#include <catch2/catch_test_macros.hpp>
#include <set>
#include <string>

using uo_mule::gui::ToolCategory;
using uo_mule::gui::ToolContext;
using uo_mule::gui::ToolId;
using uo_mule::gui::ToolRegistry;
using uo_mule::gui::ToolSelectionGranularity;
using uo_mule::gui::ToolState;

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

TEST_CASE("toolState_Should_DefaultToMoveAndTiles")
{
    const ToolState state;

    REQUIRE(state.activeTool() == ToolId::move);
    REQUIRE(state.selectionGranularity() == ToolSelectionGranularity::tiles);
}

TEST_CASE("toolState_Should_RejectUnavailableActivationWithoutChangingState")
{
    const ToolRegistry registry;
    ToolState state;
    const ToolContext noMap;

    const auto result = state.activate(ToolId::rectangleSelect, noMap, registry);

    REQUIRE_FALSE(result.accepted);
    REQUIRE_FALSE(result.changed);
    REQUIRE(result.reason == "No map is open");
    REQUIRE(state.activeTool() == ToolId::move);
}

TEST_CASE("toolState_Should_ActivateAvailableTools")
{
    const ToolRegistry registry;
    ToolState state;
    const ToolContext context{.mapDocumentOpen = true, .mapEditable = true};

    const auto result = state.activate(ToolId::rectangleSelect, context, registry);

    REQUIRE(result.accepted);
    REQUIRE(result.changed);
    REQUIRE(state.activeTool() == ToolId::rectangleSelect);
}

TEST_CASE("toolState_Should_ShareGranularityAcrossSelectionTools")
{
    const ToolRegistry registry;
    ToolState state;
    const ToolContext context{.mapDocumentOpen = true, .mapEditable = true};

    REQUIRE(state.activate(ToolId::rectangleSelect, context, registry).accepted);
    REQUIRE(state.setSelectionGranularity(ToolSelectionGranularity::blocks, context, registry).accepted);
    REQUIRE(state.selectionGranularity() == ToolSelectionGranularity::blocks);
    REQUIRE(state.activate(ToolId::lassoSelect, context, registry).accepted);
    REQUIRE(state.selectionGranularity() == ToolSelectionGranularity::blocks);
}

TEST_CASE("toolState_Should_KeepFuzzySelectTileOnly")
{
    const ToolRegistry registry;
    ToolState state;
    const ToolContext context{.mapDocumentOpen = true, .mapEditable = true};

    REQUIRE(state.activate(ToolId::fuzzySelect, context, registry).accepted);
    const auto result = state.setSelectionGranularity(ToolSelectionGranularity::blocks, context, registry);

    REQUIRE_FALSE(result.accepted);
    REQUIRE_FALSE(result.changed);
    REQUIRE(result.reason == "Fuzzy Select supports tiles only");
    REQUIRE(state.activeTool() == ToolId::fuzzySelect);
    REQUIRE(state.selectionGranularity() == ToolSelectionGranularity::tiles);
}

TEST_CASE("toolState_Should_AllowGranularityBeforeMapOpens")
{
    const ToolRegistry registry;
    ToolState state;
    const ToolContext noMap;

    const auto result = state.setSelectionGranularity(ToolSelectionGranularity::blocks, noMap, registry);

    REQUIRE(result.accepted);
    REQUIRE(state.selectionGranularity() == ToolSelectionGranularity::blocks);
}
