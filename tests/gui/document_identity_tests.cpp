#include "gui/documents/document_identity.h"
#include "gui/documents/map_document.h"
#include "gui/documents/map_workspace.h"
#include "gui/profile/workbench_session.h"
#include "uomul/map.h"
#include "uomul/radarcol/radarcol.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace
{

const std::filesystem::path testRoot = std::filesystem::temp_directory_path() / "uo-mule-document-identity";

void createRadarcol(const std::filesystem::path & path)
{
    std::ofstream radarcol(path, std::ios::binary);
    radarcol.seekp(static_cast<std::streamoff>(uomul::radarColorCount * sizeof(uomul::RadarColor)) - 1);
    radarcol.put('\0');
}

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

TEST_CASE("DocumentManager_Should_OpenStandaloneRadarcolAsAnEditableReaderBackedDocument")
{
    const TestDirectory directory;
    const auto radarcolPath = testRoot / "radarcol.mul";
    createRadarcol(radarcolPath);

    uo_mule::gui::DocumentManager documents;
    const auto summary = uo_mule::gui::DocumentSummary{
        .identity = {.kind = uo_mule::gui::DocumentKind::radarcol,
                     .source = uo_mule::gui::makeDocumentSourceIdentity({radarcolPath})},
        .displayName = radarcolPath.filename().string(),
    };
    const auto firstId = documents.open(summary);
    const auto secondId = documents.open({
        .identity = {.kind = uo_mule::gui::DocumentKind::radarcol,
                     .source = uo_mule::gui::makeDocumentSourceIdentity({testRoot / "./radarcol.mul"})},
        .displayName = "Duplicate",
    });

    REQUIRE(firstId != 0);
    REQUIRE(secondId == firstId);
    REQUIRE(documents.documents().size() == 1);
    REQUIRE(documents.documents().front().summary.sourceKind == uo_mule::gui::DocumentSourceKind::standalone);
    REQUIRE(documents.documents().front().summary.access == uo_mule::gui::DocumentAccess::editable);
    REQUIRE(documents.documents().front().summary.validation == uo_mule::gui::DocumentValidationState::valid);
    REQUIRE(documents.documents().front().radarcol.has_value());
    REQUIRE(documents.documents().front().radarcol->reader().has_value());
    REQUIRE(documents.documents().front().radarcol->reader()->colorCount() == uomul::radarColorCount);
}

TEST_CASE("DocumentManager_Should_KeepInvalidConfiguredRadarcolReadOnly")
{
    const TestDirectory directory;
    const auto radarcolPath = testRoot / "broken-radarcol.mul";
    std::ofstream(radarcolPath, std::ios::binary).put('\0');

    uo_mule::gui::DataProfile profile;
    profile.sourceRoot = testRoot;
    profile.files[uo_mule::gui::DataFileRole::radarcol] = {.path = radarcolPath.filename()};

    uo_mule::gui::DocumentManager documents;
    documents.updateMetadataSources(profile);
    const auto documentId = documents.open({
        .identity = {.kind = uo_mule::gui::DocumentKind::radarcol,
                     .source = uo_mule::gui::makeDocumentSourceIdentity({radarcolPath})},
        .displayName = radarcolPath.filename().string(),
    });
    const auto * document = documents.find(documentId);

    REQUIRE(document != nullptr);
    REQUIRE(document->summary.sourceKind == uo_mule::gui::DocumentSourceKind::configuredMetadata);
    REQUIRE(document->summary.access == uo_mule::gui::DocumentAccess::readOnly);
    REQUIRE(document->summary.validation == uo_mule::gui::DocumentValidationState::invalid);
    REQUIRE(document->radarcol.has_value());
    REQUIRE_FALSE(document->radarcol->reader().has_value());
    REQUIRE_FALSE(document->radarcol->error().empty());
}

TEST_CASE("ConfiguredDocumentSource_Should_MatchCanonicalRadarcolPaths")
{
    const TestDirectory directory;
    const auto radarcolPath = testRoot / "radarcol.mul";
    const auto aliasPath = testRoot / "radarcol-alias.mul";
    std::ofstream radarcol(radarcolPath);
    radarcol.close();
    std::error_code symlinkError;
    std::filesystem::create_symlink(radarcolPath, aliasPath, symlinkError);
    REQUIRE_FALSE(symlinkError);

    uo_mule::gui::DataProfile profile;
    profile.sourceRoot = testRoot;
    profile.files[uo_mule::gui::DataFileRole::radarcol] = {.path = "radarcol.mul"};
    const auto configured = uo_mule::gui::configuredDocumentSourceIdentity(
        profile, uo_mule::gui::DocumentKind::radarcol);
    REQUIRE(configured.has_value());

    const auto opened = uo_mule::gui::makeDocumentSourceIdentity({aliasPath});
    REQUIRE(configured.value() == opened);
}

TEST_CASE("ConfiguredDocumentSource_Should_RequireBothTexmapPairPaths")
{
    const TestDirectory directory;
    uo_mule::gui::DataProfile profile;
    profile.sourceRoot = testRoot;
    profile.files[uo_mule::gui::DataFileRole::texidx] = {.path = "texidx.mul"};

    REQUIRE_FALSE(uo_mule::gui::configuredDocumentSourceIdentity(profile, uo_mule::gui::DocumentKind::texmapPair));

    profile.files[uo_mule::gui::DataFileRole::texmaps] = {.path = "texmaps.mul"};
    const auto configured = uo_mule::gui::configuredDocumentSourceIdentity(
        profile, uo_mule::gui::DocumentKind::texmapPair);
    REQUIRE(configured.has_value());
    REQUIRE(configured->canonicalPaths.size() == 2);
}

TEST_CASE("WorkbenchSession_Should_RebindOpenMetadataSourceStatusWhenProfileChanges")
{
    const TestDirectory directory;
    const auto firstRadarcol = testRoot / "first-radarcol.mul";
    const auto secondRadarcol = testRoot / "second-radarcol.mul";
    std::ofstream(firstRadarcol).close();
    std::ofstream(secondRadarcol).close();

    const auto makeSummary = [](const std::filesystem::path & path) {
        return uo_mule::gui::DocumentSummary{
            .identity = {.kind = uo_mule::gui::DocumentKind::radarcol,
                         .source = uo_mule::gui::makeDocumentSourceIdentity({path})},
            .displayName = path.filename().string(),
        };
    };

    uo_mule::gui::WorkbenchSession session;
    const auto documentId = session.openDocument(makeSummary(firstRadarcol));
    REQUIRE(documentId != 0);
    REQUIRE(session.documents().front().summary.sourceKind == uo_mule::gui::DocumentSourceKind::standalone);

    uo_mule::gui::DataProfile firstProfile;
    firstProfile.sourceRoot = testRoot;
    firstProfile.files[uo_mule::gui::DataFileRole::radarcol] = {.path = firstRadarcol.filename()};
    session.updateProfile(firstProfile);
    REQUIRE(session.documents().front().summary.sourceKind == uo_mule::gui::DocumentSourceKind::configuredMetadata);
    REQUIRE(session.documents().front().summary.access == uo_mule::gui::DocumentAccess::readOnly);

    uo_mule::gui::DataProfile secondProfile;
    secondProfile.sourceRoot = testRoot;
    secondProfile.files[uo_mule::gui::DataFileRole::radarcol] = {.path = secondRadarcol.filename()};
    session.updateProfile(secondProfile);
    REQUIRE(session.documents().front().summary.sourceKind == uo_mule::gui::DocumentSourceKind::standalone);
    REQUIRE(session.documents().front().summary.access == uo_mule::gui::DocumentAccess::editable);
}
