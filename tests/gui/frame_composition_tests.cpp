#include "gui/frontend/frame_composition.h"
#include "gui/frontend/document_surface.h"
#include "gui/documents/document_identity.h"
#include <catch2/catch_test_macros.hpp>

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
