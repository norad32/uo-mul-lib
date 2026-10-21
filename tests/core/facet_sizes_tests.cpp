#include "core/facet_sizes.h"
#include <catch2/catch_test_macros.hpp>
#include <string_view>

TEST_CASE("knownFacetSizes_Should_ExposeCanonicalFacetPresets")
{
    const auto facets = uo_mule::core::knownFacetSizes();

    REQUIRE(facets.size() == 6);
    REQUIRE(facets.front().name == "legacy");
    REQUIRE(std::string_view(facets.front().label) == "Legacy");
    REQUIRE(std::string_view(facets.front().mapLabel) == "Trammel/Felucca");
    REQUIRE(facets.front().widthTiles == 6144);
    REQUIRE(facets.front().heightTiles == 4096);
    REQUIRE(facets[1].name == "ml");
    REQUIRE(std::string_view(facets[1].label) == "ML");
    REQUIRE(std::string_view(facets[1].mapLabel) == "Trammel/Felucca (ML)");
    REQUIRE(facets[1].widthTiles == 7168);
    REQUIRE(facets[1].heightTiles == 4096);
}

TEST_CASE("facetSize_Should_ResolveMondainsLegacy")
{
    const auto expected = uo_mule::core::FacetSize{
        .name = "ml",
        .label = "ML",
        .mapLabel = "Trammel/Felucca (ML)",
        .widthTiles = 7168,
        .heightTiles = 4096,
    };

    REQUIRE(uo_mule::core::facetSize("ML") == expected);
    REQUIRE(uo_mule::core::facetSize("Mondain's Legacy") == expected);
}

TEST_CASE("facetSize_Should_ResolveLegacyTrammelAndWorldAliases")
{
    const auto expected = uo_mule::core::FacetSize{
        .name = "legacy",
        .label = "Legacy",
        .mapLabel = "Trammel/Felucca",
        .widthTiles = 6144,
        .heightTiles = 4096,
    };

    REQUIRE(uo_mule::core::facetSize("Legacy Trammel") == expected);
    REQUIRE(uo_mule::core::facetSize("Legacy") == expected);
    REQUIRE(uo_mule::core::facetSize("Felucca") == expected);
    REQUIRE(uo_mule::core::facetSize("TRAMMEL") == expected);
}

TEST_CASE("facetSize_Should_IgnoreCaseAndSeparators")
{
    const auto facet = uo_mule::core::facetSize("TER MUR");

    REQUIRE(facet);
    REQUIRE(facet->name == "ter-mur");
    REQUIRE(facet->widthTiles == 1280);
    REQUIRE(facet->heightTiles == 4096);
}

TEST_CASE("facetSize_Should_ReturnEmptyForUnknownFacet")
{
    REQUIRE_FALSE(uo_mule::core::facetSize("unknown"));
}

TEST_CASE("facetMapFileBytes_Should_CalculateKnownMapFileSizes")
{
    const auto ml = uo_mule::core::facetSize("ml");
    const auto legacy = uo_mule::core::facetSize("legacy");

    REQUIRE(ml);
    REQUIRE(legacy);
    REQUIRE(uo_mule::core::facetMapFileBytes(*ml) == 89915392U);
    REQUIRE(uo_mule::core::facetMapFileBytes(*legacy) == 77070336U);
}

TEST_CASE("facetSizesForMapFileBytes_Should_PreserveAmbiguousCandidates")
{
    const auto malas = uo_mule::core::facetSize("malas");
    const auto termur = uo_mule::core::facetSize("ter-mur");

    REQUIRE(malas);
    REQUIRE(termur);
    const auto matches = uo_mule::core::facetSizesForMapFileBytes(uo_mule::core::facetMapFileBytes(*malas));

    REQUIRE(matches.size() == 2);
    REQUIRE(matches[0].name == malas->name);
    REQUIRE(matches[1].name == termur->name);
}
