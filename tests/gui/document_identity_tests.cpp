#include "gui/documents/document_identity.h"
#include "gui/documents/map_document.h"
#include "gui/documents/map_workspace.h"
#include "uomul/map.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <system_error>

namespace
{

const std::filesystem::path testRoot = std::filesystem::temp_directory_path() / "uo-mule-document-identity";

class TestDirectory
{
public:
    TestDirectory()
    {
        std::error_code ignoredError;
        std::filesystem::remove_all(testRoot, ignoredError);
        std::filesystem::create_directories(testRoot, ignoredError);
    }

    TestDirectory(const TestDirectory &) = delete;
    TestDirectory & operator=(const TestDirectory &) = delete;

    ~TestDirectory()
    {
        std::error_code ignoredError;
        std::filesystem::remove_all(testRoot, ignoredError);
    }
};

} // namespace

TEST_CASE("DocumentIdentity_Should_CanonicalizeEquivalentSinglePathSources")
{
    const auto first = uo_mule::gui::makeDocumentSourceIdentity({testRoot / "maps" / ".." / "map0.mul"});
    const auto second = uo_mule::gui::makeDocumentSourceIdentity({testRoot / "map0.mul"});

    REQUIRE(first.canonicalPaths.size() == 1);
    REQUIRE(first == second);
    REQUIRE(uo_mule::gui::sameDocumentIdentity(
        {.id = 1, .kind = uo_mule::gui::DocumentKind::map, .source = first},
        {.id = 2, .kind = uo_mule::gui::DocumentKind::map, .source = second}));
}

TEST_CASE("DocumentIdentity_Should_RepresentTexmapPairAsOneUnorderedSource")
{
    const auto identity = uo_mule::gui::makeDocumentSourceIdentity(
        {testRoot / "texidx.mul", testRoot / "texmaps.mul"});
    const auto reversed = uo_mule::gui::makeDocumentSourceIdentity(
        {testRoot / "texmaps.mul", testRoot / "texidx.mul"});

    REQUIRE(identity.canonicalPaths.size() == 2);
    REQUIRE(identity.canonicalPaths.at(0) != identity.canonicalPaths.at(1));
    REQUIRE(identity == reversed);
}

TEST_CASE("MapDocument_Should_ProduceGenericSummary")
{
    uo_mule::gui::MapDocument document{
        .id = 7,
        .identity = {.id = 7,
                     .kind = uo_mule::gui::DocumentKind::map,
                     .source = uo_mule::gui::makeDocumentSourceIdentity({testRoot / "map0.mul"})},
        .sourcePath = testRoot / "map0.mul",
        .workingPath = {},
        .dimensions = {},
        .dirty = true,
    };

    const auto summary = document.summary();
    REQUIRE(summary.identity.id == 7);
    REQUIRE(summary.identity.kind == uo_mule::gui::DocumentKind::map);
    REQUIRE(summary.displayName == "map0.mul");
    REQUIRE(summary.sourceKind == uo_mule::gui::DocumentSourceKind::standalone);
    REQUIRE(summary.access == uo_mule::gui::DocumentAccess::editable);
    REQUIRE(summary.validation == uo_mule::gui::DocumentValidationState::valid);
    REQUIRE(summary.dirty);
}

TEST_CASE("DocumentManager_Should_ReuseMapIdentityForEquivalentCanonicalPaths")
{
    const TestDirectory directory;
    const auto sourcePath = testRoot / "map0.mul";
    const auto equivalentPath = testRoot / "maps" / ".." / "map0.mul";
    const uomul::Dimensions dimensions{.widthTiles = 16, .heightTiles = 16};
    std::error_code ignoredError;
    std::filesystem::create_directories(equivalentPath.parent_path(), ignoredError);
    REQUIRE_FALSE(ignoredError);
    REQUIRE(uomul::MapEditor::createEmpty(sourcePath, dimensions));

    uo_mule::gui::MapWorkspace workspace;
    uo_mule::gui::DocumentManager documents;
    REQUIRE(workspace.openMap(sourcePath, dimensions));
    documents.sync(workspace);
    const auto firstId = documents.active()->id;

    REQUIRE(workspace.openMap(equivalentPath, dimensions));
    documents.sync(workspace);

    REQUIRE(documents.active()->id == firstId);
}

TEST_CASE("DocumentManager_Should_CoexistWithAndReuseMetadataPlaceholders")
{
    const TestDirectory directory;
    const auto mapPath = testRoot / "map0.mul";
    const uomul::Dimensions dimensions{.widthTiles = 16, .heightTiles = 16};
    REQUIRE(uomul::MapEditor::createEmpty(mapPath, dimensions));

    uo_mule::gui::DocumentManager documents;
    const auto radarcolIdentity = uo_mule::gui::makeDocumentSourceIdentity({testRoot / "radarcol.mul"});
    const auto pairIdentity = uo_mule::gui::makeDocumentSourceIdentity(
        {testRoot / "texidx.mul", testRoot / "texmaps.mul"});

    const auto radarcolId = documents.open({
        .identity = {.kind = uo_mule::gui::DocumentKind::radarcol, .source = radarcolIdentity},
        .displayName = "radarcol.mul",
        .sourceKind = uo_mule::gui::DocumentSourceKind::standalone,
        .access = uo_mule::gui::DocumentAccess::editable,
        .validation = uo_mule::gui::DocumentValidationState::valid,
    });
    const auto pairId = documents.open({
        .identity = {.kind = uo_mule::gui::DocumentKind::texmapPair, .source = pairIdentity},
        .displayName = "Texmap Pair",
        .sourceKind = uo_mule::gui::DocumentSourceKind::standalone,
        .access = uo_mule::gui::DocumentAccess::readOnly,
        .validation = uo_mule::gui::DocumentValidationState::valid,
    });
    uo_mule::gui::MapWorkspace workspace;
    REQUIRE(workspace.openMap(mapPath, dimensions));
    documents.sync(workspace);

    REQUIRE(radarcolId != 0);
    REQUIRE(pairId != 0);
    REQUIRE(documents.documents().size() == 3);
    REQUIRE(documents.active().has_value());
    const auto mapId = documents.active()->id;
    REQUIRE(mapId != radarcolId);
    REQUIRE(mapId != pairId);
    REQUIRE(documents.activate(radarcolId));
    REQUIRE(documents.activeIdentity()->id == radarcolId);
    REQUIRE(documents.open({
                .identity = {.kind = uo_mule::gui::DocumentKind::radarcol,
                             .source = uo_mule::gui::makeDocumentSourceIdentity({testRoot / "./radarcol.mul"})},
                .displayName = "Duplicate",
            }) == radarcolId);
    REQUIRE(documents.documents().size() == 3);
    REQUIRE(documents.close(pairId));
    REQUIRE_FALSE(documents.find(pairId));
    REQUIRE(documents.activeIdentity()->id == radarcolId);
    REQUIRE(documents.activate(mapId));
    REQUIRE(documents.active().has_value());
    documents.close();
    REQUIRE_FALSE(documents.activeIdentity());
    REQUIRE(documents.documents().size() == 1);
}
