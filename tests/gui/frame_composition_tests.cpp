#include "gui/frontend/frame_composition.h"
#include "gui/frontend/document_surface.h"
#include "gui/documents/document_identity.h"
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

TEST_CASE("documentSurface_Should_UseThePersistedCentralWindowIdentity")
{
    REQUIRE(std::string_view(uo_mule::gui::documentSurfaceWindowName()) == "Map Canvas###view.map-canvas");
}

TEST_CASE("documentTabLabel_Should_ShowKindDirtyAndReadOnlyState")
{
    const uo_mule::gui::DocumentSummary summary{
        .identity = {.kind = uo_mule::gui::DocumentKind::radarcol, .source = {}},
        .displayName = "radarcol.mul",
        .access = uo_mule::gui::DocumentAccess::readOnly,
        .dirty = true,
    };

    REQUIRE(uo_mule::gui::documentTabLabel(summary) == "Radarcol: radarcol.mul * [Read-only]");
}

TEST_CASE("documentTabLabel_Should_UseUntitledForEmptyDisplayName")
{
    const uo_mule::gui::DocumentSummary summary{
        .identity = {.kind = uo_mule::gui::DocumentKind::texmapPair, .source = {}},
        .displayName = {},
    };

    REQUIRE(uo_mule::gui::documentTabLabel(summary) == "Texmap Pair: Untitled");
}
