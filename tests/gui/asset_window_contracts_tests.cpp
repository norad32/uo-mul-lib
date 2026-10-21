#include "gui/assets/asset_window_contracts.h"
#include <catch2/catch_test_macros.hpp>

namespace
{

constexpr auto grid = uo_mule::gui::assets::AssetPresentationCapability::grid;
constexpr auto table = uo_mule::gui::assets::AssetPresentationCapability::table;
constexpr auto filter = uo_mule::gui::assets::AssetQueryCapability::filter;
constexpr auto sort = uo_mule::gui::assets::AssetQueryCapability::sort;

} // namespace

TEST_CASE("assetCapabilities_Should_ReportDeclaredPresentationAndQuerySupport")
{
    uo_mule::gui::assets::AssetCapabilitySet capabilities{
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

TEST_CASE("assetSelection_Should_ReplaceSingleSelectionAndTrackAnchor")
{
    uo_mule::gui::assets::AssetSelection selection;
    uo_mule::gui::assets::selectSingle(selection, {.value = "42"});

    REQUIRE(selection.selected.size() == 1);
    REQUIRE(uo_mule::gui::assets::contains(selection, {.value = "42"}));
    REQUIRE(selection.anchor->value == "42");

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
