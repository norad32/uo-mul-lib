#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/world/combine_contracts.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>

namespace
{

constexpr std::uint32_t legacyWidth = 6144;
constexpr std::uint32_t legacyHeight = 4096;
constexpr std::uint32_t mismatchedWidth = 3072;

uo_mule::gui::CombineRequest compatibleRequest()
{
    return {
        .map = {.kind = uo_mule::gui::DocumentKind::map, .source = uo_mule::gui::makeDocumentSourceIdentity({"map.mul"})},
        .statics =
            {
                .kind = uo_mule::gui::DocumentKind::statics,
                .source = uo_mule::gui::makeDocumentSourceIdentity({"statics.mul"}),
            },
        .mapFacetKey = "felucca",
        .staticsFacetKey = "felucca",
        .mapDimensions = {.widthTiles = legacyWidth, .heightTiles = legacyHeight},
        .staticsDimensions = {.widthTiles = legacyWidth, .heightTiles = legacyHeight},
    };
}

} // namespace

TEST_CASE("combineContracts_Should_AcceptExactFacetAndDimensionMatch")
{
    const auto result = evaluateCombine(compatibleRequest());
    REQUIRE(result.accepted);
    REQUIRE(result.combinedIdentity.kind == uo_mule::gui::DocumentKind::combined);
    REQUIRE(result.combinedIdentity.source.roles.size() == 2);
}

TEST_CASE("combineContracts_Should_RejectFacetMismatch")
{
    auto request = compatibleRequest();
    request.staticsFacetKey = "trammel";
    const auto result = evaluateCombine(request);
    REQUIRE_FALSE(result.accepted);
    REQUIRE(result.rejection == uo_mule::gui::CombineRejectionReason::facetMismatch);
}

TEST_CASE("combineContracts_Should_RejectDimensionMismatch")
{
    auto request = compatibleRequest();
    request.staticsDimensions.widthTiles = mismatchedWidth;
    const auto result = evaluateCombine(request);
    REQUIRE_FALSE(result.accepted);
    REQUIRE(result.rejection == uo_mule::gui::CombineRejectionReason::dimensionsMismatch);
}

TEST_CASE("splitContracts_Should_RoundTripStandaloneRecreationMetadata")
{
    const uo_mule::gui::SplitMetadata metadata{
        .map =
            {
                .kind = uo_mule::gui::DocumentKind::map,
                .source =
                    {
                        .id = 1,
                        .kind = uo_mule::gui::DocumentKind::map,
                        .source = uo_mule::gui::makeDocumentSourceIdentity({"map.mul"}),
                    },
                .displayName = "Fel Map",
                .facetKey = "felucca",
                .dimensions = {.widthTiles = legacyWidth, .heightTiles = legacyHeight},
            },
        .statics =
            {
                .kind = uo_mule::gui::DocumentKind::statics,
                .source =
                    {
                        .id = 2,
                        .kind = uo_mule::gui::DocumentKind::statics,
                        .source = uo_mule::gui::makeDocumentSourceIdentity({"statics.mul"}),
                    },
                .displayName = "Fel Statics",
                .facetKey = "felucca",
                .dimensions = {.widthTiles = legacyWidth, .heightTiles = legacyHeight},
            },
    };
    const uo_mule::gui::SplitRequest request{
        .combined = {.id = 3, .kind = uo_mule::gui::DocumentKind::combined, .source = {}},
        .metadata = metadata,
    };
    const auto result = evaluateSplit(request);
    REQUIRE(result.accepted);
    REQUIRE(result.metadata.map.displayName == "Fel Map");
    REQUIRE(result.metadata.statics.facetKey == "felucca");
}
