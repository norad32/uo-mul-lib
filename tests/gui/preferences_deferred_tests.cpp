#include "gui/panels/log_panel.h"
#include "gui/workbench/workbench_state.h"
#include <catch2/catch_test_macros.hpp>

using uo_mule::gui::PendingPreferenceChanges;

TEST_CASE("pendingPreferences_Should_StartEmpty")
{
    const PendingPreferenceChanges changes;
    REQUIRE(changes.empty());
}

TEST_CASE("pendingPreferences_Should_TrackTexmapChanges")
{
    PendingPreferenceChanges changes;
    changes.texmapsHideUnused = false;
    REQUIRE_FALSE(changes.empty());
    changes.texmapsTablePresentation = true;
    REQUIRE(*changes.texmapsHideUnused == false);
    REQUIRE(*changes.texmapsTablePresentation == true);
}

TEST_CASE("pendingPreferences_Should_TrackWelcomeScreenChanges")
{
    PendingPreferenceChanges changes;
    changes.showWelcomeOnStartup = false;

    REQUIRE_FALSE(changes.empty());
    REQUIRE(*changes.showWelcomeOnStartup == false);
}

TEST_CASE("logPanelRequest_Should_DefaultWithoutPersistentChanges")
{
    const uo_mule::gui::LogPanelRequest request;

    REQUIRE_FALSE(request.visible.has_value());
    REQUIRE_FALSE(request.autoScroll.has_value());
    REQUIRE_FALSE(request.collapsed.has_value());
    REQUIRE_FALSE(request.height.has_value());
    REQUIRE_FALSE(request.clearEntries);
}
