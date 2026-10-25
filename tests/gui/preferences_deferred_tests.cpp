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
