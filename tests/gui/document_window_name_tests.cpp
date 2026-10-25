#include "gui/documents/document_window_name.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include <catch2/catch_test_macros.hpp>
#include <optional>

TEST_CASE("documentWindow_Should_UseStableDocumentIdentity")
{
    const uo_mule::gui::DocumentSummary summary{
        .identity = {.id = 42, .kind = uo_mule::gui::DocumentKind::map, .source = uo_mule::gui::SourceIdentity{}},
        .displayName = "first.mul",
        .facetKey = std::nullopt,
        .dimensions = std::nullopt,
    };
    REQUIRE(uo_mule::gui::documentWindowName(summary) == "Map: first.mul###document.42");
}

TEST_CASE("documentWindow_Should_UseRadarcolAssetWindowIdentity")
{
    const uo_mule::gui::DocumentSummary summary{
        .identity =
            {
                .id = 9,
                .kind = uo_mule::gui::DocumentKind::radarcol,
                .source = uo_mule::gui::SourceIdentity{},
            }, // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
        .displayName = "radarcol.mul",
        .facetKey = std::nullopt,
        .dimensions = std::nullopt,
    };
    REQUIRE(uo_mule::gui::documentWindowName(summary) == "Radarcol: radarcol.mul###document.9");
}

TEST_CASE("documentWindow_Should_KeepCentralIdentity_If_DisplayTitleChanges")
{
    uo_mule::gui::DocumentSummary summary{
        .identity =
            {
                .id = 9, // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
                .kind = uo_mule::gui::DocumentKind::radarcol,
                .source = uo_mule::gui::SourceIdentity{},
            },
        .displayName = "old.mul",
        .facetKey = std::nullopt,
        .dimensions = std::nullopt,
    };
    const auto first = uo_mule::gui::documentWindowName(summary);
    summary.displayName = "new.mul";
    summary.dirty = true;

    REQUIRE(first.substr(first.find("###")) ==
            uo_mule::gui::documentWindowName(summary).substr(uo_mule::gui::documentWindowName(summary).find("###")));
}
