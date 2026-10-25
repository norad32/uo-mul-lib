#include "gui/documents/map_document_window.h"
#include "gui/workbench/workbench_state.h"
#include "gui_model/documents/map_document_state.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/providers/data_providers.h"
#include "gui_model/world/map_context.h"
#include "uomul/map/map.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("mapContextSnapshot_Should_ExposeNoActiveLocationWithoutMap")
{
    const uo_mule::gui::WorkbenchState state;
    const auto context = state.session.contextSnapshot(state.mapDocuments);

    REQUIRE_FALSE(context.hasDocument);
    REQUIRE_FALSE(context.activeTile.has_value());
    REQUIRE_FALSE(context.activeValue.has_value());
    REQUIRE_FALSE(context.activeBlock.has_value());
    REQUIRE(context.selectionMode == uo_mule::gui::MapSelectionMode::tiles);
}

TEST_CASE("dataProviders_Should_ReportEmbeddedRadarcolAndUnavailableOptionalAssets_If_ProfileHasNoPaths")
{
    uo_mule::gui::DataProviders providers;
    providers.sync(uo_mule::gui::DataProfile{});

    REQUIRE(providers.radarColors().status().state == uo_mule::gui::ProviderAssetState::available);
    REQUIRE(providers.radarColors().status().source == uo_mule::gui::ProviderSourceKind::embeddedDefault);
    REQUIRE(providers.textures().status().state == uo_mule::gui::ProviderAssetState::unavailable);
    REQUIRE(providers.tiledata().status().state == uo_mule::gui::ProviderAssetState::unavailable);
}

TEST_CASE("dataProviders_Should_InvalidateAllProviders")
{
    uo_mule::gui::DataProviders providers;
    providers.sync(uo_mule::gui::DataProfile{});
    const auto firstRevision = providers.radarColors().status().revision;

    providers.invalidate();

    REQUIRE(providers.radarColors().status().revision > firstRevision);
    REQUIRE(providers.textures().status().state == uo_mule::gui::ProviderAssetState::unavailable);
    REQUIRE(providers.tiledata().status().state == uo_mule::gui::ProviderAssetState::unavailable);
}

TEST_CASE("MapCanvas_Should_UseTheEffectiveRadarColorProvider")
{
    uo_mule::gui::DataProviders providers;
    providers.sync(uo_mule::gui::DataProfile{});
    const uomul::MapTile tile{.id = 0, .z = 0};

    REQUIRE(
        uo_mule::gui::mapCanvasTileColor(uo_mule::gui::MapDisplayMode::tileIds, tile, providers.radarColors()) ==
        uo_mule::gui::tileDisplayColor(uo_mule::gui::MapDisplayMode::tileIds, tile, providers.radarColors().mapColor(tile.id)));
}
