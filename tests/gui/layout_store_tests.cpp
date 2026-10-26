#include "core/app_info.h"
#include "gui/application/layout_store.h"
#include "gui_model/layout/panel_registry.h"
#include "gui_model/layout/window_registry.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <string>

namespace
{
} // namespace

TEST_CASE("appLayout_Should_RoundTripPanelsWindowsAndDockSettings")
{
    constexpr int expectedX = 37;
    constexpr int expectedY = 42;
    constexpr int expectedWidth = 1440;
    constexpr int expectedHeight = 900;
    const auto configDirectory = std::filesystem::temp_directory_path() / "uo-mule-layout-roundtrip";
    std::filesystem::remove_all(configDirectory);
    const auto layoutFile = configDirectory / std::string(uo_mule::core::applicationName()) / "layout.ini";

    uo_mule::gui::PanelRegistry expectedPanels;
    expectedPanels.find(uo_mule::gui::PanelId::toolPalette)->visible = false;
    uo_mule::gui::WindowRegistry expectedWindows;
    expectedWindows.find(uo_mule::gui::WindowId::texmaps)->visible = false;
    uo_mule::gui::AppLayout expectedLayout{
        .window =
            {
                .geometryValid = true,
                .x = expectedX,
                .y = expectedY,
                .width = expectedWidth,
                .height = expectedHeight,
            },
        .dockingIni = "[Window][Radarcol###window.radarcol]\nPos=400,200\nDockId=0x1234,2\n",
    };

    uo_mule::gui::saveLayout(layoutFile, expectedPanels, expectedWindows, expectedLayout);

    uo_mule::gui::PanelRegistry actualPanels;
    uo_mule::gui::WindowRegistry actualWindows;
    uo_mule::gui::AppLayout actualLayout;
    uo_mule::gui::loadLayout(layoutFile, actualPanels, actualWindows, actualLayout);

    REQUIRE(std::filesystem::is_regular_file(layoutFile));
    REQUIRE_FALSE(actualPanels.find(uo_mule::gui::PanelId::toolPalette)->visible);
    REQUIRE(actualWindows.find(uo_mule::gui::WindowId::radarcol)->visible);
    REQUIRE_FALSE(actualWindows.find(uo_mule::gui::WindowId::texmaps)->visible);
    REQUIRE(actualLayout.window.geometryValid);
    REQUIRE(actualLayout.window.x == expectedX);
    REQUIRE(actualLayout.window.y == expectedY);
    REQUIRE(actualLayout.window.width == expectedWidth);
    REQUIRE(actualLayout.window.height == expectedHeight);
    REQUIRE(actualLayout.dockingIni == expectedLayout.dockingIni);

    std::filesystem::remove_all(configDirectory);
}
