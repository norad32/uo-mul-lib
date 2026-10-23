#include "gui_model/providers/data_providers.h"
#include "gui_model/providers/embedded_radarcol.h"
#include "uomul/radarcol/radarcol.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>

TEST_CASE("EmbeddedRadarcol_Should_ExposeTheSupportedColorCount")
{
    REQUIRE(uo_mule::gui::embeddedRadarcolColorCount() == uomul::radarColorCount);
    REQUIRE(uo_mule::gui::embeddedRadarcolSourceColorCount == 81884U);
    REQUIRE(uo_mule::gui::embeddedRadarcolColor(uomul::radarColorCount).has_value() == false);
}

TEST_CASE("EmbeddedRadarcol_Should_PreserveRepresentativeSourceColors")
{
    REQUIRE(uo_mule::gui::embeddedRadarcolColor(0U).value_or(0U) == 0x0842U);
    REQUIRE(uo_mule::gui::embeddedRadarcolColor(3U).value_or(0U) == 0x1502U);
    REQUIRE(uo_mule::gui::embeddedRadarcolColor(uomul::radarStaticColorOffset).value_or(0U) == 0x526BU);
    REQUIRE(uo_mule::gui::embeddedRadarcolColor(uomul::radarColorCount - 2U).value_or(0U) == 0x24C4U);
    REQUIRE(uo_mule::gui::embeddedRadarcolColor(uomul::radarColorCount - 1U).value_or(0U) == 0x0000U);
}

TEST_CASE("EmbeddedRadarcolProvider_Should_ProvideMapColorsWithoutAFile")
{
    const auto provider = uo_mule::gui::makeEmbeddedRadarColorProvider();
    REQUIRE(provider != nullptr);
    REQUIRE(provider->status().state == uo_mule::gui::ProviderAssetState::available);
    REQUIRE(provider->status().message == "Embedded Radarcol");
    REQUIRE(provider->status(0U).state == uo_mule::gui::ProviderAssetState::available);
    REQUIRE(provider->mapColor(0U).value_or(uo_mule::gui::MapColor{}) ==
            uo_mule::gui::MapColor{.red = 16, .green = 16, .blue = 16});
    REQUIRE(provider->status(static_cast<std::uint16_t>(uomul::radarMapColorCount)).state ==
            uo_mule::gui::ProviderAssetState::invalid);
    REQUIRE_FALSE(provider->mapColor(static_cast<std::uint16_t>(uomul::radarMapColorCount)).has_value());
}
