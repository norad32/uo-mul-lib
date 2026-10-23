#include "gui/documents/document_identity.h"
#include "gui/frontend/document_surface.h"
#include "gui/frontend/frame_composition.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("documentWindow_Should_UseStableDocumentIdentity")
{
    const uo_mule::gui::DocumentSummary summary{
        .identity = {.id = 42, .kind = uo_mule::gui::DocumentKind::map, .source = {}},
        .displayName = "first.mul",
    };
    REQUIRE(uo_mule::gui::documentWindowName(summary) == "Map: first.mul###document.42");
}

TEST_CASE("documentWindow_Should_UseRadarcolAssetWindowIdentity")
{
    const uo_mule::gui::DocumentSummary summary{
        .identity = {.id = 9, .kind = uo_mule::gui::DocumentKind::radarcol, .source = {}},
        .displayName = "radarcol.mul",
    };
    REQUIRE(uo_mule::gui::documentWindowName(summary) == "Radarcol: radarcol.mul###document.9");
}
