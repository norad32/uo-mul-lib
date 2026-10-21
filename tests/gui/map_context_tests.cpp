#include "gui/providers/data_providers.h"
#include "gui/profile/data_profile.h"
#include "gui/context/map_context.h"
#include "gui/documents/map_workspace.h"
#include "gui/views/selection_stats_panel.h"
#include "gui/views/map_canvas_panel.h"
#include "gui/application/workbench_state.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("mapContextSnapshot_Should_ExposeNoActiveLocationWithoutMap")
{
    const uo_mule::gui::WorkbenchState state;
    const auto context = state.session.contextSnapshot(state.workspace);

    REQUIRE_FALSE(context.hasDocument);
    REQUIRE_FALSE(context.activeTile.has_value());
    REQUIRE_FALSE(context.activeValue.has_value());
    REQUIRE_FALSE(context.activeBlock.has_value());
    REQUIRE(context.selectionMode == uo_mule::gui::MapSelectionMode::tiles);
}

TEST_CASE("dataProviders_Should_ReportEmbeddedRadarcolAndUnavailableOptionalAssets_WhenProfileHasNoPaths")
{
    uo_mule::gui::DataProviders providers;
    providers.sync(uo_mule::gui::DataProfile{});

    REQUIRE(providers.radarColors().status().state == uo_mule::gui::ProviderAssetState::available);
    REQUIRE(providers.radarColors().status().source == uo_mule::gui::ProviderSourceKind::embeddedDefault);
    REQUIRE(providers.textures().status().state == uo_mule::gui::ProviderAssetState::unavailable);
    REQUIRE(providers.tileData().status().state == uo_mule::gui::ProviderAssetState::unavailable);
}

TEST_CASE("dataProviders_Should_InvalidateAllProviders")
{
    uo_mule::gui::DataProviders providers;
    providers.sync(uo_mule::gui::DataProfile{});
    const auto firstRevision = providers.radarColors().status().revision;

    providers.invalidate();

    REQUIRE(providers.radarColors().status().revision > firstRevision);
    REQUIRE(providers.textures().status().state == uo_mule::gui::ProviderAssetState::unavailable);
    REQUIRE(providers.tileData().status().state == uo_mule::gui::ProviderAssetState::unavailable);
}

TEST_CASE("MapCanvas_Should_UseTheEffectiveRadarColorProvider")
{
    uo_mule::gui::DataProviders providers;
    providers.sync(uo_mule::gui::DataProfile{});
    const uomul::MapTile tile{.id = 0, .z = 0};

    REQUIRE(uo_mule::gui::mapCanvasTileColor(uo_mule::gui::MapDisplayMode::tileIds, tile, providers.radarColors()) ==
            uo_mule::gui::tileDisplayColor(uo_mule::gui::MapDisplayMode::tileIds,
                                            tile,
                                            providers.radarColors().mapColor(tile.id)));
}

TEST_CASE("selectionStatsState_Should_StartIdle")
{
    const uo_mule::gui::SelectionStatsPanelState state;

    REQUIRE(state.status == uo_mule::gui::SelectionStatsState::idle);
}
