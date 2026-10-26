#include "gui/dialogs/map_dimension_fields.h"
#include "uomul/facet_sizes.h"
#include <catch2/catch_test_macros.hpp>
#include <cstddef>

TEST_CASE("mapDimensions_Should_ResolveEveryKnownFacetPreset")
{
    const auto facets = uomul::knownFacetSizes();
    std::size_t index = 0;
    for (const auto & facet : facets)
    {
        const auto dimensions = uo_mule::gui::resolveMapDimensions(static_cast<int>(index + 1U), 0U, 0U);
        REQUIRE(dimensions);
        REQUIRE(dimensions->widthTiles == facet.widthTiles);
        REQUIRE(dimensions->heightTiles == facet.heightTiles);
        index++;
    }
}

TEST_CASE("mapDimensions_Should_ValidateCustomDimensions")
{
    REQUIRE(uo_mule::gui::resolveMapDimensions(0, 16U, 24U));
    REQUIRE_FALSE(uo_mule::gui::resolveMapDimensions(0, 15U, 24U));
    REQUIRE_FALSE(uo_mule::gui::resolveMapDimensions(0, 16U, 23U));
}

TEST_CASE("mapDimensions_Should_RejectOutOfRangeFacetPreset")
{
    const auto invalidPreset = static_cast<int>(uomul::knownFacetSizes().size() + 1U);
    REQUIRE_FALSE(uo_mule::gui::resolveMapDimensions(invalidPreset, 0U, 0U));
}
