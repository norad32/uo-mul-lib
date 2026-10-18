#include "core/app_info.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("applicationName_Should_ReturnImCpp")
{
    REQUIRE(uo_mule::core::applicationName() == uo_mule::core::metadata::applicationName);
    REQUIRE(uo_mule::core::applicationVersion() == uo_mule::core::metadata::applicationVersion);
    REQUIRE_FALSE(uo_mule::core::applicationDescription().empty());
    REQUIRE_FALSE(uo_mule::core::applicationHomepage().empty());
    REQUIRE(uo_mule::core::applicationLicense() == "MIT");
    REQUIRE(uo_mule::core::applicationAuthor() == "norad32");
}
