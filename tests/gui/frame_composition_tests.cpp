#include "gui/frontend/frame_composition.h"
#include <catch2/catch_test_macros.hpp>
#include <string_view>

TEST_CASE("defaultDockWindows_Should_PreserveStableViewWindowNames")
{
    const auto windows = uo_mule::gui::defaultDockWindows();

    REQUIRE(windows.size() == 8);
    REQUIRE(std::string_view(windows[0].windowName) == "Map Canvas###view.map-canvas");
    REQUIRE(std::string_view(windows[1].windowName) == "Tile Inspector###view.tile-inspector");
    REQUIRE(std::string_view(windows[2].windowName) == "Block Inspector###view.block-inspector");
    REQUIRE(std::string_view(windows[3].windowName) == "Selection Statistics###view.selection-statistics");
    REQUIRE(std::string_view(windows[4].windowName) == "Map Information###view.map-information");
    REQUIRE(std::string_view(windows[5].windowName) == "Edit Tools###view.edit-tools");
    REQUIRE(std::string_view(windows[6].windowName) == "Texmaps###view.texmaps");
    REQUIRE(std::string_view(windows[7].windowName) == "Radarcol###view.radarcol");
}
