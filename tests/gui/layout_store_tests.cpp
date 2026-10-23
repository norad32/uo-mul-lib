#include "core/app_info.h"
#include "gui/persistence/layout_store.h"
#include "gui/views/panel_registry.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <string>

namespace
{
} // namespace

TEST_CASE("workspaceLayout_Should_RoundTripWindowPanelsAndDockSettings")
{
    constexpr int expectedX = 37;
    constexpr int expectedY = 42;
    constexpr int expectedWidth = 1440;
    constexpr int expectedHeight = 900;
    const auto configDirectory = std::filesystem::temp_directory_path() / "uo-mule-layout-roundtrip";
    std::filesystem::remove_all(configDirectory);
    const auto layoutFile = configDirectory / std::string(uo_mule::core::applicationName()) / "layout.ini";

    uo_mule::gui::PanelRegistry expectedRegistry;
    expectedRegistry.find(uo_mule::gui::PanelId::texmaps)->visible = false;
    uo_mule::gui::WorkspaceLayout expectedLayout{
        .window =
            {
                .geometryValid = true,
                .x = expectedX,
                .y = expectedY,
                .width = expectedWidth,
                .height = expectedHeight,
            },
        .dockingIni = "[Window][Radarcol###view.radarcol]\nPos=400,200\nDockId=0x1234,2\n",
    };

    uo_mule::gui::saveLayout(layoutFile, expectedRegistry, expectedLayout);

    uo_mule::gui::PanelRegistry actualRegistry;
    uo_mule::gui::WorkspaceLayout actualLayout;
    uo_mule::gui::loadLayout(layoutFile, actualRegistry, actualLayout);

    REQUIRE(std::filesystem::is_regular_file(layoutFile));
    REQUIRE_FALSE(actualRegistry.find(uo_mule::gui::PanelId::texmaps)->visible);
    REQUIRE(actualRegistry.find(uo_mule::gui::PanelId::radarcol)->visible);
    REQUIRE(actualLayout.window.geometryValid);
    REQUIRE(actualLayout.window.x == expectedX);
    REQUIRE(actualLayout.window.y == expectedY);
    REQUIRE(actualLayout.window.width == expectedWidth);
    REQUIRE(actualLayout.window.height == expectedHeight);
    REQUIRE(actualLayout.dockingIni == expectedLayout.dockingIni);

    std::filesystem::remove_all(configDirectory);
}
