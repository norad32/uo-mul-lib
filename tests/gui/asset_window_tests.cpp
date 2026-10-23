#include "gui/assets/asset_window.h"
#include <catch2/catch_test_macros.hpp>

namespace
{

constexpr float testCanvasWidth = 640.0F;
constexpr float testCellWidth = 64.0F;
constexpr float testCellSpacing = 4.0F;

} // namespace

TEST_CASE("assetGridLayout_Should_UseAtLeastOneColumn")
{
    const auto layout = uo_mule::gui::assets::gridLayout(5U, 0.0F, 64.0F, 4.0F);

    REQUIRE(layout.columns == 1U);
    REQUIRE(layout.rows == 5U);
}

TEST_CASE("assetGridLayout_Should_CalculateRowsFromAvailableWidth")
{
    const auto layout = uo_mule::gui::assets::gridLayout(10U, 208.0F, 64.0F, 4.0F);

    REQUIRE(layout.columns == 3U);
    REQUIRE(layout.rows == 4U);
}

TEST_CASE("assetGridLayout_Should_NotReserveTrailingSpacing")
{
    const auto layout = uo_mule::gui::assets::gridLayout(3U, 200.0F, 64.0F, 4.0F);

    REQUIRE(layout.columns == 3U);
    REQUIRE(layout.rows == 1U);
}

TEST_CASE("assetGridLayout_Should_ReturnEmptyForNoItems")
{
    REQUIRE(uo_mule::gui::assets::gridLayout(0U, testCanvasWidth, testCellWidth, testCellSpacing).rows == 0U);
}

TEST_CASE("assetWindowState_Should_ResetSelectionAndCachesForNewContext")
{
    uo_mule::gui::assets::AssetWindowState state;
    state.queryCacheContext = "old-context";
    state.queryCacheFilter = "old-filter";
    state.searchItemsValid = true;
    state.queryCacheValid = true;
    state.selection.selected.push_back({.value = "7"});
    state.selection.anchor = state.selection.selected.front();
    state.selection.anchorIndex = 3U;
    state.selectionDragBase = state.selection;
    state.selectionDragActive = true;
    state.scrollPosition = 42.0F;

    uo_mule::gui::assets::resetContext(state);

    REQUIRE(state.queryCacheContext.empty());
    REQUIRE(state.queryCacheFilter.empty());
    REQUIRE_FALSE(state.searchItemsValid);
    REQUIRE_FALSE(state.queryCacheValid);
    REQUIRE(state.selection.selected.empty());
    REQUIRE_FALSE(state.selection.anchor.has_value());
    REQUIRE_FALSE(state.selection.anchorIndex.has_value());
    REQUIRE(state.selectionDragBase.selected.empty());
    REQUIRE_FALSE(state.selectionDragActive);
    REQUIRE(state.scrollPosition <= 0.0F);
}
