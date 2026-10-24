#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_descriptors.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/documents/tiledata_document.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/source/tiledata_source.h"
#include "uomul/tiledata/tiledata.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <optional>

namespace
{

[[nodiscard]] std::filesystem::path fixturePath()
{
    return std::filesystem::path(UOMUL_SOURCE_DIR) / "testfiles" / "tiledata.mul";
}

} // namespace

TEST_CASE("TiledataLayoutSelection_Should_RoundTripNames")
{
    using uo_mule::gui::TiledataLayoutSelection;

    REQUIRE(uo_mule::gui::tiledataLayoutSelectionFromName("auto") == TiledataLayoutSelection::autoDetect);
    REQUIRE(uo_mule::gui::tiledataLayoutSelectionFromName("legacy32") == TiledataLayoutSelection::legacy32);
    REQUIRE(uo_mule::gui::tiledataLayoutSelectionFromName("extended64") == TiledataLayoutSelection::extended64);
    REQUIRE_FALSE(uo_mule::gui::tiledataLayoutSelectionFromName("unknown"));
}

TEST_CASE("TiledataLayout_Should_ResolveExactFixtureSize")
{
    REQUIRE(uo_mule::gui::resolveTiledataLayout(fixturePath(), uo_mule::gui::TiledataLayoutSelection::autoDetect) ==
            uomul::TiledataLayout::extended64);
    REQUIRE_FALSE(uo_mule::gui::resolveTiledataLayout(fixturePath(), uo_mule::gui::TiledataLayoutSelection::legacy32));
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("TiledataDocument_Should_OpenReadOnlyAndReuseDescriptorIdentity")
{
    REQUIRE(std::filesystem::is_regular_file(fixturePath()));
    const auto document = uo_mule::gui::TiledataDocument::open(1, fixturePath(), uomul::TiledataLayout::extended64);
    REQUIRE(document);
    REQUIRE(document->readOnly());
    REQUIRE_FALSE(document->dirty());
    const auto & reader = document->reader();
    REQUIRE(reader.has_value());
    if (reader)
    {
        const auto mapTile = reader->readMapTile(3);
        REQUIRE(mapTile);
        REQUIRE(mapTile.value_or(uomul::MapTileRecord{}).name == "grass");
    }

    const uo_mule::gui::DocumentDescriptorRegistry registry;
    const auto summary = registry.createSummary({
        .kind = uo_mule::gui::DocumentKind::tiledata,
        .source = uo_mule::gui::makeDocumentSourceIdentity({fixturePath()}),
        .displayName = "tiledata.mul",
        .access = uo_mule::gui::DocumentAccess::readOnly,
        .validation = uo_mule::gui::DocumentValidationState::valid,
        .facetKey = std::nullopt,
        .dimensions = std::nullopt,
    });
    REQUIRE(summary);
    REQUIRE(summary->identity.kind == uo_mule::gui::DocumentKind::tiledata);
}
