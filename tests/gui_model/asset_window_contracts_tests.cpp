#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/model/assets/asset_window_contracts.h"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <string>

namespace
{

constexpr auto grid = uo_mule::gui::assets::AssetPresentationCapability::grid;
constexpr auto table = uo_mule::gui::assets::AssetPresentationCapability::table;
constexpr auto filter = uo_mule::gui::assets::AssetQueryCapability::filter;
constexpr auto sort = uo_mule::gui::assets::AssetQueryCapability::sort;
constexpr std::size_t indexedSelection = 7U;

} // namespace

TEST_CASE("assetCapabilities_Should_ReportDeclaredPresentationAndQuerySupport")
{
    const uo_mule::gui::assets::AssetCapabilitySet capabilities{
        .presentations = static_cast<std::uint8_t>(grid) | static_cast<std::uint8_t>(table),
        .queries = static_cast<std::uint8_t>(filter) | static_cast<std::uint8_t>(sort),
        .selection = uo_mule::gui::assets::AssetSelectionMode::multiple,
    };

    REQUIRE(capabilities.supports(grid));
    REQUIRE(capabilities.supports(table));
    REQUIRE(capabilities.supports(filter));
    REQUIRE(capabilities.supports(sort));
}

TEST_CASE("assetRangeRequest_Should_DeclareTablePresentation")
{
    const uo_mule::gui::assets::AssetRangeRequest request{
        .first = 4U,
        .count = 8U,
        .presentation = uo_mule::gui::assets::AssetPresentation::table,
    };

    REQUIRE(request.presentation == uo_mule::gui::assets::AssetPresentation::table);
    REQUIRE(request.first == 4U);
    REQUIRE(request.count == 8U);
}

TEST_CASE("assetWindowState_Should_RetainPresentationPerContext")
{
    uo_mule::gui::assets::AssetWindowState state;
    state.presentations["radarcol|source-a"] = uo_mule::gui::assets::AssetPresentation::table;
    state.presentations["radarcol|source-b"] = uo_mule::gui::assets::AssetPresentation::grid;

    REQUIRE(state.presentations.at("radarcol|source-a") == uo_mule::gui::assets::AssetPresentation::table);
    REQUIRE(state.presentations.at("radarcol|source-b") == uo_mule::gui::assets::AssetPresentation::grid);
}

TEST_CASE("assetSourceContext_Should_KeepMetadataAndDocumentContextsSeparate")
{
    const auto source = uo_mule::gui::makeDocumentSourceIdentity({"radarcol.mul"});
    const uo_mule::gui::assets::AssetSourceContext metadata{
        .kind = uo_mule::gui::DocumentKind::radarcol,
        .contextKind = uo_mule::gui::assets::AssetSourceContext::Kind::metadataView,
        .source = source,
        .contextId = "view.radarcol",
    };
    const uo_mule::gui::assets::AssetSourceContext document{
        .kind = uo_mule::gui::DocumentKind::radarcol,
        .contextKind = uo_mule::gui::assets::AssetSourceContext::Kind::document,
        .source = source,
        .contextId = "document.radarcol.7",
    };

    REQUIRE_FALSE(metadata == document);
    REQUIRE(metadata.source == document.source);
}

TEST_CASE("assetDragPayload_Should_ExposeAReusableDisplayLabel")
{
    uo_mule::gui::assets::AssetDragPayload payload;
    payload.type = std::string(uo_mule::gui::assets::assetValueDragPayloadType);
    payload.label = "Radarcol color";

    REQUIRE(payload.label == "Radarcol color");
}

TEST_CASE("assetSelection_Should_ReplaceSingleSelectionAndTrackAnchor")
{
    uo_mule::gui::assets::AssetSelection selection;
    uo_mule::gui::assets::selectSingle(selection, {.value = "42"});

    REQUIRE(selection.selected.size() == 1);
    REQUIRE(uo_mule::gui::assets::contains(selection, {.value = "42"}));
    REQUIRE(selection.anchor.has_value());
    REQUIRE(selection.selected.front().value == "42");

    uo_mule::gui::assets::selectSingle(selection, {.value = "43"});
    REQUIRE(selection.selected.size() == 1);
    REQUIRE_FALSE(uo_mule::gui::assets::contains(selection, {.value = "42"}));
    REQUIRE(uo_mule::gui::assets::contains(selection, {.value = "43"}));
}

TEST_CASE("assetSelection_Should_ClearItemsAndAnchor")
{
    uo_mule::gui::assets::AssetSelection selection;
    uo_mule::gui::assets::selectSingle(selection, {.value = "42"});
    uo_mule::gui::assets::clear(selection);

    REQUIRE(selection.selected.empty());
    REQUIRE_FALSE(selection.anchor.has_value());
}

TEST_CASE("assetSelection_Should_ToggleItemsAndTrackIndexedAnchor")
{
    uo_mule::gui::assets::AssetSelection selection;
    uo_mule::gui::assets::selectSingleAt(selection, {.value = "42"}, 4U);
    uo_mule::gui::assets::toggleAt(selection, {.value = "43"}, indexedSelection);

    REQUIRE(selection.selected.size() == 2U);
    REQUIRE(selection.anchor.has_value());
    REQUIRE(selection.selected.back().value == "43");
    REQUIRE(selection.anchorIndex == indexedSelection);

    uo_mule::gui::assets::toggleAt(selection, {.value = "43"}, indexedSelection);
    REQUIRE(selection.selected.size() == 1U);
    REQUIRE_FALSE(uo_mule::gui::assets::contains(selection, {.value = "43"}));
}

TEST_CASE("assetSelection_Should_TrackIndexedSingleSelection")
{
    uo_mule::gui::assets::AssetSelection selection;
    uo_mule::gui::assets::selectSingleAt(selection, {.value = "42"}, 4U);

    REQUIRE(selection.anchor.has_value());
    REQUIRE(selection.selected.front().value == "42");
    REQUIRE(selection.anchorIndex == 4U);
}

TEST_CASE("assetFilter_Should_MatchSpaceSeparatedTermsAcrossValues")
{
    uo_mule::gui::assets::AssetQuery query;
    query.filter = "dimensions 0x0001";
    const std::array<std::string, 2> values{"Dimensions: 64 x 64", "ID: 0x0001"};

    REQUIRE(uo_mule::gui::assets::filterMatches(query, values));
}

TEST_CASE("assetFilter_Should_RequireCommaSeparatedTerms")
{
    uo_mule::gui::assets::AssetQuery query;
    query.filter = "dimensions,64";
    const std::array<std::string, 1> values{"Dimensions: 64 x 64"};

    REQUIRE(uo_mule::gui::assets::filterMatches(query, values));
}

TEST_CASE("assetFilter_Should_MatchQuotedPhrase")
{
    uo_mule::gui::assets::AssetQuery query;
    query.filter = "\"64 x 64\"";
    const std::array<std::string, 1> values{"Dimensions: 64 x 64"};

    REQUIRE(uo_mule::gui::assets::filterMatches(query, values));
}

TEST_CASE("assetFilter_Should_MatchSimpleRegex")
{
    uo_mule::gui::assets::AssetQuery query;
    query.filter = "/0x00[0-9A-F]{2}/";
    const std::array<std::string, 1> values{"ID: 0x003A"};

    REQUIRE(uo_mule::gui::assets::filterMatches(query, values));
}
