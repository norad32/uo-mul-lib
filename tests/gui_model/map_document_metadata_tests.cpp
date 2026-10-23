#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/map_document.h"
#include "uomul/facet_sizes.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>

using uo_mule::gui::DocumentKind;
using uo_mule::gui::MapDocument;
using uo_mule::gui::mapFacetKey;

constexpr std::uint32_t customWidth = 1232;
constexpr std::uint32_t customHeight = 784;

TEST_CASE("mapDocument_Should_DeriveStableKnownFacetKeyFromDimensions")
{
    const auto facet = uomul::knownFacetSizes().front();
    REQUIRE(mapFacetKey({.widthTiles = facet.widthTiles, .heightTiles = facet.heightTiles}) == facet.name);
}

TEST_CASE("mapDocument_Should_DeriveStableCustomFacetKeyFromDimensions")
{
    REQUIRE(mapFacetKey({.widthTiles = customWidth, .heightTiles = customHeight}) == "custom-1232x784");
}

TEST_CASE("mapDocumentSummary_Should_ExposeFacetAndDimensions")
{
    MapDocument document;
    document.id = 7; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    document.identity.kind = DocumentKind::map;
    document.sourcePath = "renamed-map.mul";
    document.facetKey = "custom-1232x784";
    document.dimensions = {.widthTiles = customWidth, .heightTiles = customHeight};

    const auto summary = document.summary();
    REQUIRE(summary.facetKey == "custom-1232x784");
    REQUIRE(summary.dimensions.has_value());
    const auto dimensions = summary.dimensions.value_or(uomul::Dimensions{});
    REQUIRE(dimensions.widthTiles == customWidth);
    REQUIRE(dimensions.heightTiles == customHeight);
}
