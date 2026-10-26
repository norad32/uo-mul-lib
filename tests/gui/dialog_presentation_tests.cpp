#include "gui/presentation/dialog_presentation.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("dialogPresentation_Should_ProvideBoundedDialogSizes")
{
    const auto large = uo_mule::gui::largeDialogSpec();
    const auto browser = uo_mule::gui::browserDialogSpec();

    REQUIRE(large.initialSize.x >= large.minimumSize.x);
    REQUIRE(large.initialSize.y >= large.minimumSize.y);
    REQUIRE(browser.initialSize.x >= browser.minimumSize.x);
    REQUIRE(browser.initialSize.y >= browser.minimumSize.y);
}

TEST_CASE("dialogPresentation_Should_ProvideLargerBrowserSize")
{
    const auto large = uo_mule::gui::largeDialogSpec();
    const auto browser = uo_mule::gui::browserDialogSpec();

    REQUIRE(browser.initialSize.x > large.initialSize.x);
}
