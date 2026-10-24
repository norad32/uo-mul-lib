#include "gui_model/input/radarcol_color_input.h"
#include "uomul/radarcol/radarcol.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("radarcolColorInput_Should_ParseBoundedRgbChannels")
{
    REQUIRE(uo_mule::gui::parseRadarcolChannel("0").value() == 0U);
    REQUIRE(uo_mule::gui::parseRadarcolChannel("255").value() == 255U);
    REQUIRE_FALSE(uo_mule::gui::parseRadarcolChannel("256"));
    REQUIRE_FALSE(uo_mule::gui::parseRadarcolChannel("12x"));
}

TEST_CASE("radarcolColorInput_Should_ParseRgbHexAndRawValues")
{
    REQUIRE(uo_mule::gui::parseRadarcolRgbHex("#123456").value() ==
            uomul::RadarColorConverter::fromRgb24(
                0x123456U)); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE_FALSE(uo_mule::gui::parseRadarcolRgbHex("123456"));
    REQUIRE_FALSE(uo_mule::gui::parseRadarcolRgbHex("#12345G"));
    REQUIRE(uo_mule::gui::parseRadarcolRaw("7FFF").value() ==
            0x7FFFU); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE_FALSE(uo_mule::gui::parseRadarcolRaw("8000"));
    REQUIRE_FALSE(uo_mule::gui::parseRadarcolRaw("FFF"));
}

TEST_CASE("radarcolColorInput_Should_FormatCanonicalValues")
{
    const uomul::RadarColor color = 0x1234U; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE(uo_mule::gui::formatRadarcolRgbHex(color) == "#218CA5");
    REQUIRE(uo_mule::gui::formatRadarcolRaw(color) == "1234");
}
