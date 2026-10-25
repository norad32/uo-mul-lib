#include "gui_model/documents/map_changes.h"
#include "gui_model/documents/map_document_state.h"
#include "gui_model/documents/map_view_state.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("mapChanges_Should_StartCleanWithoutAWorkingPath")
{
    const uo_mule::gui::MapChanges changes;

    REQUIRE_FALSE(changes.dirty());
    REQUIRE_FALSE(changes.reader_.has_value());
    REQUIRE(changes.mapPath_.empty());
    REQUIRE(changes.originalPath_.empty());
}

TEST_CASE("mapViewState_Should_StartWithIndependentPresentationDefaults")
{
    const uo_mule::gui::MapViewState view;

    REQUIRE(view.displayMode_ == uo_mule::gui::MapDisplayMode::tileIds);
    REQUIRE(view.selectionMode_ == uo_mule::gui::MapSelectionMode::tiles);
    REQUIRE(view.selectionMask_.empty());
    REQUIRE_FALSE(view.hoveredTile_.has_value());
    REQUIRE_FALSE(view.selectedTile_.has_value());
}

TEST_CASE("mapDocumentState_Should_ExposeSeparatedChangesAndViewState")
{
    uo_mule::gui::MapDocumentState state;

    REQUIRE_FALSE(state.changes().dirty());
    REQUIRE(state.viewState().displayMode_ == uo_mule::gui::MapDisplayMode::tileIds);
    REQUIRE(state.viewState().selectionMode_ == uo_mule::gui::MapSelectionMode::tiles);

    state.viewState().displayMode_ = uo_mule::gui::MapDisplayMode::combined;
    REQUIRE(state.viewState().displayMode_ == uo_mule::gui::MapDisplayMode::combined);
}
