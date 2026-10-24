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

TEST_CASE("toolState_Should_StoreActiveTileAndClearPickRequest")
{
    ToolState state;

    state.requestTilePick();
    REQUIRE(state.tilePickRequested());
    state.setActiveTile({.id = 321, .z = -7});

    REQUIRE(state.activeTile() == uomul::MapTile{.id = 321, .z = -7});
    REQUIRE_FALSE(state.tilePickRequested());
}

TEST_CASE("toolState_Should_ValidateManualActiveTileFields")
{
    ToolState state;

    REQUIRE(state.setActiveTileId(1234).accepted);
    REQUIRE(state.setActiveTileZ(-12).accepted);
    REQUIRE(state.activeTile() == uomul::MapTile{.id = 1234, .z = -12});
    REQUIRE_FALSE(state.setActiveTileId(-1).accepted);
    REQUIRE_FALSE(state.setActiveTileId(65536).accepted);
    REQUIRE_FALSE(state.setActiveTileZ(-129).accepted);
    REQUIRE_FALSE(state.setActiveTileZ(128).accepted);
    REQUIRE(state.activeTile() == uomul::MapTile{.id = 1234, .z = -12});
}

TEST_CASE("toolState_Should_StoreFillAndIndependentReplaceOptions")
{
    ToolState state;

    REQUIRE(state.fillMode() == uo_mule::gui::FillMode::idAndZ);
    REQUIRE(state.replaceMode() == uo_mule::gui::ReplaceMode::idAndZ);
    state.setFillMode(uo_mule::gui::FillMode::id);
    state.setReplaceMode(uo_mule::gui::ReplaceMode::z);
    REQUIRE(state.setReplaceSourceId(1234).accepted);
    REQUIRE(state.setReplaceSourceZ(-12).accepted);
    REQUIRE(state.setReplaceTargetId(2345).accepted);
    REQUIRE(state.setReplaceTargetZ(34).accepted);

    REQUIRE(state.fillMode() == uo_mule::gui::FillMode::id);
    REQUIRE(state.replaceMode() == uo_mule::gui::ReplaceMode::z);
    REQUIRE(state.replaceSource() == uomul::MapTile{.id = 1234, .z = -12});
    REQUIRE(state.replaceTarget() == uomul::MapTile{.id = 2345, .z = 34});
    REQUIRE_FALSE(state.setReplaceSourceId(65536).accepted);
    REQUIRE_FALSE(state.setReplaceTargetZ(-129).accepted);
    REQUIRE(state.replaceSource() == uomul::MapTile{.id = 1234, .z = -12});
    REQUIRE(state.replaceTarget() == uomul::MapTile{.id = 2345, .z = 34});
}

TEST_CASE("toolState_Should_TrackActiveTilePickerSource")
{
    ToolState state;

    state.requestTilePick();
    REQUIRE(state.tilePickRequested());
    REQUIRE(state.tilePickTarget() == uo_mule::gui::TilePickTarget::activeTile);
    state.setPickedTile({.id = 321, .z = -7});
    REQUIRE(state.activeTile() == uomul::MapTile{.id = 321, .z = -7});
    REQUIRE_FALSE(state.tilePickRequested());
}

TEST_CASE("toolState_ShouldRouteReplacePicksAndCopyActiveTileToTarget")
{
    ToolState state;
    state.setActiveTile({.id = 100, .z = -5});

    REQUIRE(state.setReplaceSourceFromActiveTile().accepted);
    REQUIRE(state.replaceSource() == uomul::MapTile{.id = 100, .z = -5});

    state.requestTilePick(uo_mule::gui::TilePickTarget::replaceSource);
    REQUIRE(state.tilePickTarget() == uo_mule::gui::TilePickTarget::replaceSource);
    state.setPickedTile({.id = 200, .z = 10});
    REQUIRE(state.replaceSource() == uomul::MapTile{.id = 200, .z = 10});
    REQUIRE(state.activeTile() == uomul::MapTile{.id = 100, .z = -5});

    state.requestTilePick(uo_mule::gui::TilePickTarget::replaceTarget);
    REQUIRE(state.setPickedTileId(300).accepted);
    REQUIRE(state.replaceTarget() == uomul::MapTile{.id = 300, .z = 0});
    REQUIRE_FALSE(state.tilePickRequested());
    REQUIRE(state.setReplaceTargetFromActiveTile().accepted);
    REQUIRE(state.replaceTarget() == uomul::MapTile{.id = 100, .z = -5});
}

TEST_CASE("toolState_ShouldStoreFuzzyOptionsAndBounds")
{
    ToolState state;

    state.setFuzzyScope(uo_mule::gui::FuzzyScope::wholeMap);
    state.setFuzzyIdEnabled(true);
    state.setFuzzyZEnabled(true);
    state.setFuzzyNameEnabled(true);
    REQUIRE(state.setFuzzyId(4321).accepted);
    REQUIRE(state.setFuzzyZ(-9).accepted);
    state.setFuzzyName("  Granite   floor ");

    REQUIRE(state.fuzzyOptions().scope == uo_mule::gui::FuzzyScope::wholeMap);
    REQUIRE(state.fuzzyOptions().idEnabled);
    REQUIRE(state.fuzzyOptions().id == 4321);
    REQUIRE(state.fuzzyOptions().zEnabled);
    REQUIRE(state.fuzzyOptions().z == -9);
    REQUIRE(state.fuzzyOptions().nameEnabled);
    REQUIRE(state.fuzzyOptions().name == "  Granite   floor ");
    REQUIRE_FALSE(state.setFuzzyId(-1).accepted);
    REQUIRE_FALSE(state.setFuzzyId(65536).accepted);
    REQUIRE_FALSE(state.setFuzzyZ(-129).accepted);
    REQUIRE_FALSE(state.setFuzzyZ(128).accepted);

    state.requestTilePick(uo_mule::gui::TilePickTarget::fuzzyName);
    state.setPickedTileName("granite floor");
    REQUIRE(state.fuzzyOptions().nameEnabled);
    REQUIRE(state.fuzzyOptions().name == "granite floor");
}
