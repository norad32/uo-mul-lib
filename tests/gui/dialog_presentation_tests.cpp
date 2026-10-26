#include "gui/presentation/dialog_presentation.h"
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
