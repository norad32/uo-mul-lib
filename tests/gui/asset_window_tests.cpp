#include "gui/assets/asset_window.h"
#include <catch2/catch_test_macros.hpp>

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
    REQUIRE(uo_mule::gui::assets::gridLayout(0U, 640.0F, 64.0F, 4.0F).rows == 0U);
}
