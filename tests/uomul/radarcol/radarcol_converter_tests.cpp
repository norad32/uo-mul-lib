#include "uomul/radarcol/radarcol.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("RadarColorConverter_fromRgb888_Should_PackChannels_If_ConvertingEightBitRgb")
{
    REQUIRE(uomul::RadarColorConverter::fromRgb888(255, 128, 7) == 0x7E00);
    REQUIRE(uomul::RadarColorConverter::fromRgb888(0, 0, 0) == 0);
    REQUIRE(uomul::RadarColorConverter::fromRgb888(255, 255, 255) == 0x7FFF);
}

TEST_CASE("RadarColorConverter_fromRgb24_Should_IgnoreBitsAboveRgb_If_PackedValueHasHighBits")
{
    REQUIRE(uomul::RadarColorConverter::fromRgb24(0xFF8007) == uomul::RadarColorConverter::fromRgb888(255, 128, 7));
    REQUIRE(uomul::RadarColorConverter::fromRgb24(0xFFFFFFFF) == 0x7FFF);
}

TEST_CASE("RadarColorConverter_fromArgb8888_Should_IgnoreAlpha_If_ConvertingArgbValue")
{
    REQUIRE(uomul::RadarColorConverter::fromArgb8888(0xAA123456) == uomul::RadarColorConverter::fromRgb24(0x123456));
    REQUIRE(uomul::RadarColorConverter::fromArgb8888(0x00123456) == uomul::RadarColorConverter::fromRgb24(0x123456));
}

TEST_CASE("RadarColorConverter_fromRgba8888_Should_IgnoreAlpha_If_ConvertingRgbaValue")
{
    REQUIRE(uomul::RadarColorConverter::fromRgba8888(0x123456AA) == uomul::RadarColorConverter::fromRgb24(0x123456));
    REQUIRE(uomul::RadarColorConverter::fromRgba8888(0x12345600) == uomul::RadarColorConverter::fromRgb24(0x123456));
}

TEST_CASE("RadarColorConverter_fromRgb565_Should_DiscardLowestGreenBit_If_ConvertingRgb565Value")
{
    REQUIRE(uomul::RadarColorConverter::fromRgb565(0xFFFF) == 0x7FFF);
    REQUIRE(uomul::RadarColorConverter::fromRgb565(0x0000) == 0);
}

TEST_CASE("RadarColorConverter_fromArgb1555_Should_ClearAlphaBit_If_ConvertingArgb1555Value")
{
    REQUIRE(uomul::RadarColorConverter::fromArgb1555(0xFFFF) == 0x7FFF);
    REQUIRE(uomul::RadarColorConverter::fromArgb1555(0x8000) == 0);
}

TEST_CASE("RadarColorConverter_fromRgb555_Should_ClearHighBit_If_NormalizingRgb555Value")
{
    REQUIRE(uomul::RadarColorConverter::fromRgb555(0xFFFF) == 0x7FFF);
    REQUIRE(uomul::RadarColorConverter::fromRgb555(0x8000) == 0);
}
