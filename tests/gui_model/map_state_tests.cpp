#include "gui_model/documents/map_changes.h"
#include "gui_model/documents/map_document_state.h"
#include "gui_model/documents/map_view_state.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("mapChanges_Should_StartCleanWithoutAWorkingPath")
{
    const uo_mule::gui::MapChanges changes;

    REQUIRE_FALSE(changes.dirty());
    REQUIRE_FALSE(changes.reader.has_value());
    REQUIRE(changes.mapPath.empty());
    REQUIRE(changes.originalPath.empty());
}

TEST_CASE("mapViewState_Should_StartWithIndependentPresentationDefaults")
{
    const uo_mule::gui::MapViewState view;

    REQUIRE(view.displayMode == uo_mule::gui::MapDisplayMode::tileIds);
    REQUIRE(view.selectionMode == uo_mule::gui::MapSelectionMode::tiles);
    REQUIRE(view.selectionMask.empty());
    REQUIRE_FALSE(view.hoveredTile.has_value());
    REQUIRE_FALSE(view.selectedTile.has_value());
}

TEST_CASE("mapDocumentState_Should_ExposeSeparatedChangesAndViewState")
{
    uo_mule::gui::MapDocumentState state;

    REQUIRE_FALSE(state.changes().dirty());
    REQUIRE(state.viewState().displayMode == uo_mule::gui::MapDisplayMode::tileIds);
    REQUIRE(state.viewState().selectionMode == uo_mule::gui::MapSelectionMode::tiles);

    state.viewState().displayMode = uo_mule::gui::MapDisplayMode::combined;
    REQUIRE(state.viewState().displayMode == uo_mule::gui::MapDisplayMode::combined);
}
