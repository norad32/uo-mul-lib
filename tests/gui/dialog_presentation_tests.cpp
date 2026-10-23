#include "gui/presentation/dialog/dialog_presentation.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("dialogPresentation_Should_ProvideBoundedStandardDialogSize")
{
    const auto spec = uo_mule::gui::standardDialogSpec();

    REQUIRE(spec.initialSize.x >= spec.minimumSize.x);
    REQUIRE(spec.initialSize.y >= spec.minimumSize.y);
}

TEST_CASE("dialogPresentation_Should_ProvideLargerBrowserAndPreferencesSizes")
{
    const auto standard = uo_mule::gui::standardDialogSpec();
    const auto large = uo_mule::gui::largeDialogSpec();
    const auto browser = uo_mule::gui::browserDialogSpec();

    REQUIRE(large.initialSize.x > standard.initialSize.x);
    REQUIRE(large.initialSize.y > standard.initialSize.y);
    REQUIRE(browser.initialSize.x > standard.initialSize.x);
    REQUIRE(browser.initialSize.y > standard.initialSize.y);
}

TEST_CASE("dialogPresentation_Should_ProvideCompactMapFacetSize")
{
    const auto map = uo_mule::gui::mapDialogSpec();
    const auto facet = uo_mule::gui::mapFacetDialogSpec();

    REQUIRE(facet.initialSize.x < map.initialSize.x);
    REQUIRE(facet.initialSize.y < map.initialSize.y);
    REQUIRE(facet.initialSize.x >= facet.minimumSize.x);
    REQUIRE(facet.initialSize.y >= facet.minimumSize.y);
}
