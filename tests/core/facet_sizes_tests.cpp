#include "uomul/facet_sizes.h"
#include <catch2/catch_test_macros.hpp>
#include <iterator>
#include <string_view>

TEST_CASE("knownFacetSizes_Should_ExposeCanonicalFacetPresets")
{
    const auto facets = uomul::knownFacetSizes();
    const auto & ml = *std::next(facets.begin());

    REQUIRE(facets.size() == 6);
    REQUIRE(facets.front().name == "legacy");
    REQUIRE(std::string_view(facets.front().label) == "Legacy");
    REQUIRE(std::string_view(facets.front().mapLabel) == "Trammel/Felucca");
    REQUIRE(facets.front().widthTiles == 6144);
    REQUIRE(facets.front().heightTiles == 4096);
    REQUIRE(ml.name == "ml");
    REQUIRE(std::string_view(ml.label) == "ML");
    REQUIRE(std::string_view(ml.mapLabel) == "Trammel/Felucca (ML)");
    REQUIRE(ml.widthTiles == 7168);
    REQUIRE(ml.heightTiles == 4096);
}

TEST_CASE("facetSize_Should_ResolveMondainsLegacy")
{
    const auto expected = uomul::FacetSize{
        .name = "ml",
        .label = "ML",
        .mapLabel = "Trammel/Felucca (ML)",
        .widthTiles = 7168,
        .heightTiles = 4096,
    };

    REQUIRE(uomul::facetSize("ML") == expected);
    REQUIRE(uomul::facetSize("Mondain's Legacy") == expected);
}

TEST_CASE("facetSize_Should_ResolveLegacyTrammelAndWorldAliases")
{
    const auto expected = uomul::FacetSize{
        .name = "legacy",
        .label = "Legacy",
        .mapLabel = "Trammel/Felucca",
        .widthTiles = 6144,
        .heightTiles = 4096,
    };

    REQUIRE(uomul::facetSize("Legacy Trammel") == expected);
    REQUIRE(uomul::facetSize("Legacy") == expected);
    REQUIRE(uomul::facetSize("Felucca") == expected);
    REQUIRE(uomul::facetSize("TRAMMEL") == expected);
}

TEST_CASE("facetSize_Should_IgnoreCaseAndSeparators")
{
    const auto facet = uomul::facetSize("TER MUR");

    REQUIRE(facet);
    const auto resolvedFacet = facet.value_or(uomul::FacetSize{});
    REQUIRE(resolvedFacet.name == "ter-mur");
    REQUIRE(resolvedFacet.widthTiles == 1280);
    REQUIRE(resolvedFacet.heightTiles == 4096);
}

TEST_CASE("facetSize_Should_ReturnEmptyForUnknownFacet")
{
    REQUIRE_FALSE(uomul::facetSize("unknown"));
}

TEST_CASE("facetMapFileBytes_Should_CalculateKnownMapFileSizes")
{
    const auto ml = uomul::facetSize("ml");
    const auto legacy = uomul::facetSize("legacy");

    REQUIRE(ml);
    REQUIRE(legacy);
    REQUIRE(uomul::facetMapFileBytes(ml.value_or(uomul::FacetSize{})) == 89915392U);
    REQUIRE(uomul::facetMapFileBytes(legacy.value_or(uomul::FacetSize{})) == 77070336U);
}

TEST_CASE("facetSizesForMapFileBytes_Should_PreserveAmbiguousCandidates")
{
    const auto malas = uomul::facetSize("malas");
    const auto termur = uomul::facetSize("ter-mur");

    REQUIRE(malas);
    REQUIRE(termur);
    const auto matches = uomul::facetSizesForMapFileBytes(uomul::facetMapFileBytes(malas.value_or(uomul::FacetSize{})));

    REQUIRE(matches.size() == 2);
    REQUIRE(matches.at(0).name == malas.value_or(uomul::FacetSize{}).name);
    REQUIRE(matches.at(1).name == termur.value_or(uomul::FacetSize{}).name);
}
