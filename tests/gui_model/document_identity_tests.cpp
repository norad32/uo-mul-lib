#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/documents/map_document.h"
#include "gui_model/documents/map_document_state.h"
#include "gui_model/documents/radarcol_document.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/session/workbench_session.h"
#include "gui_model/source/source_identity.h"
#include "uomul/map/map.h"
#include "uomul/radarcol/radarcol.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <system_error>

namespace
{

std::filesystem::path testRoot()
{
    return std::filesystem::temp_directory_path() / "uo-mule-document-identity";
}

void createRadarcol(const std::filesystem::path & path)
{
    std::ofstream radarcol(path, std::ios::binary);
    radarcol.seekp(static_cast<std::streamoff>(uomul::radarColorCount * sizeof(uomul::RadarColor)) - 1);
    radarcol.put('\0');
}

std::optional<std::uint32_t> radarColorCount(const std::optional<uo_mule::gui::RadarcolDocument> & document)
{
    if (!document || !document->reader())
    {
        return std::nullopt;
    }
    return document->reader()->colorCount();
}

bool isInvalidConfiguredDocument(const uo_mule::gui::DocumentRecord * document)
{
    if (document == nullptr || !document->radarcol)
    {
        return false;
    }
    return document->summary.sourceKind == uo_mule::gui::DocumentSourceKind::configuredMetadata &&
           document->summary.validation == uo_mule::gui::DocumentValidationState::invalid && !document->radarcol->reader() &&
           !document->radarcol->error().empty();
}

uo_mule::gui::RadarcolDocument * mutableRadarcolDocument(uo_mule::gui::DocumentRecord * record)
{
    if (record == nullptr || !record->radarcol)
    {
        return nullptr;
    }
    return &record->radarcol.value();
}

const uo_mule::gui::RadarcolDocument * radarcolDocument(const uo_mule::gui::DocumentRecord * record)
{
    if (record == nullptr || !record->radarcol)
    {
        return nullptr;
    }
    return &record->radarcol.value();
}

class TestDirectory
{
public:
    TestDirectory()
    {
        std::error_code ignoredError;
        std::filesystem::remove_all(testRoot(), ignoredError);
        std::filesystem::create_directories(testRoot(), ignoredError);
    }

    TestDirectory(const TestDirectory &) = delete;
    TestDirectory & operator=(const TestDirectory &) = delete;
    TestDirectory(TestDirectory &&) = delete;
    TestDirectory & operator=(TestDirectory &&) = delete;

    ~TestDirectory()
    {
        std::error_code ignoredError;
        std::filesystem::remove_all(testRoot(), ignoredError);
    }
};

} // namespace

TEST_CASE("DocumentIdentity_Should_CanonicalizeEquivalentSinglePathSources")
{
    const auto first = uo_mule::gui::makeDocumentSourceIdentity({testRoot() / "maps" / ".." / "map0.mul"});
    const auto second = uo_mule::gui::makeDocumentSourceIdentity({testRoot() / "map0.mul"});

    REQUIRE(first.paths().size() == 1);
    REQUIRE(first == second);
    REQUIRE(uo_mule::gui::sameDocumentIdentity({.id = 1, .kind = uo_mule::gui::DocumentKind::map, .source = first},
                                               {.id = 2, .kind = uo_mule::gui::DocumentKind::map, .source = second}));
}

TEST_CASE("DocumentIdentity_Should_RepresentTexmapPairAsOneUnorderedSource")
{
    const auto identity = uo_mule::gui::makeRoleAwareDocumentSourceIdentity({
        {.role = uo_mule::gui::SourceRole::index, .path = testRoot() / "texidx.mul"},
        {.role = uo_mule::gui::SourceRole::data, .path = testRoot() / "texmaps.mul"},
    });
    const auto reversed = uo_mule::gui::makeRoleAwareDocumentSourceIdentity({
        {.role = uo_mule::gui::SourceRole::data, .path = testRoot() / "texmaps.mul"},
        {.role = uo_mule::gui::SourceRole::index, .path = testRoot() / "texidx.mul"},
    });
    const auto swapped = uo_mule::gui::makeRoleAwareDocumentSourceIdentity({
        {.role = uo_mule::gui::SourceRole::index, .path = testRoot() / "texmaps.mul"},
        {.role = uo_mule::gui::SourceRole::data, .path = testRoot() / "texidx.mul"},
    });

    REQUIRE(identity.paths().size() == 2);
    REQUIRE(identity.path(uo_mule::gui::SourceRole::index) != identity.path(uo_mule::gui::SourceRole::data));
    REQUIRE(identity == reversed);
    REQUIRE(identity != swapped);
}

TEST_CASE("MapDocument_Should_ProduceGenericSummary")
{
    const uo_mule::gui::MapDocument document{
        .id = 7, // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
        .identity =
            {
                .id = 7, // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
                .kind = uo_mule::gui::DocumentKind::map,
                .source = uo_mule::gui::makeDocumentSourceIdentity({testRoot() / "map0.mul"}),
            },
        .sourcePath = testRoot() / "map0.mul",
        .workingPath = {},
        .facetKey = {},
        .dimensions = uomul::Dimensions(),
        .dirty = true,
    };

    const auto summary = document.summary();
    REQUIRE(summary.identity.id == 7);
    REQUIRE(summary.identity.kind == uo_mule::gui::DocumentKind::map);
    REQUIRE(summary.displayName == "map0.mul");
    REQUIRE(summary.sourceKind == uo_mule::gui::DocumentSourceKind::standalone);
    REQUIRE(summary.validation == uo_mule::gui::DocumentValidationState::valid);
    REQUIRE(summary.dirty);
}

TEST_CASE("DocumentManager_Should_ReuseMapIdentityForEquivalentCanonicalPaths")
{
    const TestDirectory directory;
    const auto sourcePath = testRoot() / "map0.mul";
    const auto equivalentPath = testRoot() / "maps" / ".." / "map0.mul";
    const uomul::Dimensions dimensions{.widthTiles = 16, .heightTiles = 16};
    std::error_code ignoredError;
    std::filesystem::create_directories(equivalentPath.parent_path(), ignoredError);
    REQUIRE_FALSE(ignoredError);
    REQUIRE(uomul::MapEditor::createEmpty(sourcePath, dimensions));

    uo_mule::gui::MapDocumentState mapDocumentState;
    uo_mule::gui::DocumentManager documents;
    REQUIRE(mapDocumentState.openMap(sourcePath, dimensions));
    documents.sync(mapDocumentState);
    const auto firstId = documents.active().value_or(uo_mule::gui::MapDocument{}).id;

    REQUIRE(mapDocumentState.openMap(equivalentPath, dimensions));
    documents.sync(mapDocumentState);

    REQUIRE(documents.active().value_or(uo_mule::gui::MapDocument{}).id == firstId);
}

TEST_CASE("DocumentManager_Should_CoexistWithAndReuseMetadataPlaceholders")
{
    const TestDirectory directory;
    const auto mapPath = testRoot() / "map0.mul";
    const uomul::Dimensions dimensions{.widthTiles = 16, .heightTiles = 16};
    REQUIRE(uomul::MapEditor::createEmpty(mapPath, dimensions));

    uo_mule::gui::DocumentManager documents;
    const auto radarcolIdentity = uo_mule::gui::makeDocumentSourceIdentity({testRoot() / "radarcol.mul"});
    const auto pairIdentity = uo_mule::gui::makeRoleAwareDocumentSourceIdentity({
        {.role = uo_mule::gui::SourceRole::index, .path = testRoot() / "texidx.mul"},
        {.role = uo_mule::gui::SourceRole::data, .path = testRoot() / "texmaps.mul"},
    });

    const auto radarcolId = documents.open({
        .identity = {.kind = uo_mule::gui::DocumentKind::radarcol, .source = radarcolIdentity},
        .displayName = "radarcol.mul",
        .sourceKind = uo_mule::gui::DocumentSourceKind::standalone,
        .validation = uo_mule::gui::DocumentValidationState::valid,
        .facetKey = std::nullopt,
        .dimensions = std::nullopt,
    });
    const auto pairId = documents.open({
        .identity = {.kind = uo_mule::gui::DocumentKind::texmapPair, .source = pairIdentity},
        .displayName = "Texmap Pair",
        .sourceKind = uo_mule::gui::DocumentSourceKind::standalone,
        .validation = uo_mule::gui::DocumentValidationState::valid,
        .facetKey = std::nullopt,
        .dimensions = std::nullopt,
    });
    uo_mule::gui::MapDocumentState mapDocumentState;
    REQUIRE(mapDocumentState.openMap(mapPath, dimensions));
    documents.sync(mapDocumentState);

    REQUIRE(radarcolId != 0);
    REQUIRE(pairId != 0);
    REQUIRE(documents.documents().size() == 3);
    REQUIRE(documents.active().has_value());
    const auto mapId = documents.active().value_or(uo_mule::gui::MapDocument{}).id;
    REQUIRE(mapId != radarcolId);
    REQUIRE(mapId != pairId);
    REQUIRE(documents.activate(radarcolId));
    REQUIRE(documents.activeIdentity().value_or(uo_mule::gui::DocumentIdentity{}).id == radarcolId);
    REQUIRE(documents.open({
                .identity =
                    {
                        .kind = uo_mule::gui::DocumentKind::radarcol,
                        .source = uo_mule::gui::makeDocumentSourceIdentity({testRoot() / "./radarcol.mul"}),
                    },
                .displayName = "Duplicate",
                .facetKey = std::nullopt,
                .dimensions = std::nullopt,
            }) == radarcolId);
    REQUIRE(documents.documents().size() == 3);
    REQUIRE(documents.close(pairId));
    REQUIRE_FALSE(documents.find(pairId));
    REQUIRE(documents.activeIdentity().value_or(uo_mule::gui::DocumentIdentity{}).id == radarcolId);
    REQUIRE(documents.activate(mapId));
    REQUIRE(documents.active().has_value());
    documents.close();
    REQUIRE_FALSE(documents.activeIdentity());
    REQUIRE(documents.documents().size() == 1);
}

TEST_CASE("DocumentManager_Should_OpenStandaloneRadarcolAsAnEditableReaderBackedDocument")
{
    const TestDirectory directory;
    const auto radarcolPath = testRoot() / "radarcol.mul";
    createRadarcol(radarcolPath);

    uo_mule::gui::DocumentManager documents;
    const auto summary = uo_mule::gui::DocumentSummary{
        .identity =
            {
                .kind = uo_mule::gui::DocumentKind::radarcol,
                .source = uo_mule::gui::makeDocumentSourceIdentity({radarcolPath}),
            },
        .displayName = radarcolPath.filename().string(),
        .facetKey = std::nullopt,
        .dimensions = std::nullopt,
    };
    const auto firstId = documents.open(summary);
    const auto secondId = documents.open({
        .identity =
            {
                .kind = uo_mule::gui::DocumentKind::radarcol,
                .source = uo_mule::gui::makeDocumentSourceIdentity({testRoot() / "./radarcol.mul"}),
            },
        .displayName = "Duplicate",
        .facetKey = std::nullopt,
        .dimensions = std::nullopt,
    });

    const bool reused = firstId != 0 && secondId == firstId && documents.documents().size() == 1 &&
                        documents.documents().front().summary.sourceKind == uo_mule::gui::DocumentSourceKind::standalone &&
                        documents.documents().front().summary.validation == uo_mule::gui::DocumentValidationState::valid;
    REQUIRE(reused);
    const auto & radarcol = documents.documents().front().radarcol;
    REQUIRE(radarcol.has_value());
    REQUIRE(radarColorCount(radarcol) == uomul::radarColorCount);
}

TEST_CASE("DocumentManager_Should_RejectClosingDirtyRadarcol")
{
    const TestDirectory directory;
    const auto radarcolPath = testRoot() / "dirty-radarcol.mul";
    createRadarcol(radarcolPath);

    uo_mule::gui::DocumentManager documents;
    const auto documentId = documents.open({
        .identity =
            {
                .kind = uo_mule::gui::DocumentKind::radarcol,
                .source = uo_mule::gui::makeDocumentSourceIdentity({radarcolPath}),
            },
        .displayName = radarcolPath.filename().string(),
        .facetKey = std::nullopt,
        .dimensions = std::nullopt,
    });
    REQUIRE(documentId != 0U);

    auto * record = documents.findMutable(documentId);
    REQUIRE(record != nullptr);
    auto * radarcol = mutableRadarcolDocument(record);
    REQUIRE(radarcol != nullptr);
    if (radarcol == nullptr)
    {
        return;
    }
    REQUIRE(radarcol->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::editColors,
        .colorEdits = {{.colorIndex = 0U, .color = 1U}},
        .outputPath = {},
        .overwriteOutput = false,
    }));
    REQUIRE_FALSE(documents.close(documentId));
    REQUIRE(documents.find(documentId) != nullptr);

    REQUIRE(radarcol->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::discard,
        .colorEdits = {},
        .outputPath = {},
        .overwriteOutput = false,
    }));
    REQUIRE(documents.close(documentId));
}

TEST_CASE("DocumentManager_Should_KeepInvalidConfiguredRadarcolEditable")
{
    const TestDirectory directory;
    const auto radarcolPath = testRoot() / "broken-radarcol.mul";
    std::ofstream(radarcolPath, std::ios::binary).put('\0');

    uo_mule::gui::DataProfile profile;
    profile.sourceRoot = testRoot();
    profile.files[uo_mule::gui::DataFileRole::radarcol] = {.path = radarcolPath.filename()};

    uo_mule::gui::DocumentManager documents;
    documents.updateMetadataSources(profile);
    const auto documentId = documents.open({
        .identity =
            {
                .kind = uo_mule::gui::DocumentKind::radarcol,
                .source = uo_mule::gui::makeDocumentSourceIdentity({radarcolPath}),
            },
        .displayName = radarcolPath.filename().string(),
        .facetKey = std::nullopt,
        .dimensions = std::nullopt,
    });
    const auto * document = documents.find(documentId);

    REQUIRE(isInvalidConfiguredDocument(document));
}

TEST_CASE("DocumentManager_Should_KeepInvalidStandaloneRadarcolAsDiagnosticPlaceholder")
{
    const TestDirectory directory;
    const auto radarcolPath = testRoot() / "broken-standalone-radarcol.mul";
    std::ofstream(radarcolPath, std::ios::binary).put('\0');

    uo_mule::gui::DocumentManager documents;
    const auto documentId = documents.open({
        .identity =
            {
                .kind = uo_mule::gui::DocumentKind::radarcol,
                .source = uo_mule::gui::makeDocumentSourceIdentity({radarcolPath}),
            },
        .displayName = radarcolPath.filename().string(),
        .facetKey = std::nullopt,
        .dimensions = std::nullopt,
    });
    const auto * document = documents.find(documentId);

    REQUIRE(documentId != 0U);
    REQUIRE(document != nullptr);
    REQUIRE(document->summary.sourceKind == uo_mule::gui::DocumentSourceKind::standalone);
    REQUIRE(document->summary.validation == uo_mule::gui::DocumentValidationState::invalid);
    REQUIRE(document->radarcol.has_value());
    const auto * radarcol = radarcolDocument(document);
    REQUIRE(radarcol != nullptr);
    if (radarcol == nullptr)
    {
        return;
    }
    REQUIRE_FALSE(radarcol->reader());
    REQUIRE_FALSE(radarcol->error().empty());
}

TEST_CASE("ConfiguredDocumentSource_Should_MatchCanonicalRadarcolPaths")
{
    const TestDirectory directory;
    const auto radarcolPath = testRoot() / "radarcol.mul";
    const auto aliasPath = testRoot() / "radarcol-alias.mul";
    std::ofstream radarcol(radarcolPath);
    radarcol.close();
    std::error_code symlinkError;
    std::filesystem::create_symlink(radarcolPath, aliasPath, symlinkError);
    REQUIRE_FALSE(symlinkError);

    uo_mule::gui::DataProfile profile;
    profile.sourceRoot = testRoot();
    profile.files[uo_mule::gui::DataFileRole::radarcol] = {.path = "radarcol.mul"};
    const auto configured = uo_mule::gui::configuredDocumentSourceIdentity(profile, uo_mule::gui::DocumentKind::radarcol);
    REQUIRE(configured.has_value());

    const auto opened = uo_mule::gui::makeDocumentSourceIdentity({aliasPath});
    const auto configuredIdentity = configured.value_or(uo_mule::gui::DocumentSourceIdentity{});
    REQUIRE(configuredIdentity == opened);
}

TEST_CASE("ConfiguredDocumentSource_Should_RequireBothTexmapPairPaths")
{
    const TestDirectory directory;
    uo_mule::gui::DataProfile profile;
    profile.sourceRoot = testRoot();
    profile.files[uo_mule::gui::DataFileRole::texidx] = {.path = "texidx.mul"};

    REQUIRE_FALSE(uo_mule::gui::configuredDocumentSourceIdentity(profile, uo_mule::gui::DocumentKind::texmapPair));

    profile.files[uo_mule::gui::DataFileRole::texmaps] = {.path = "texmaps.mul"};
    const auto configured = uo_mule::gui::configuredDocumentSourceIdentity(profile, uo_mule::gui::DocumentKind::texmapPair);
    REQUIRE(configured.has_value());
    const auto configuredIdentity = configured.value_or(uo_mule::gui::DocumentSourceIdentity{});
    REQUIRE(configuredIdentity.paths().size() == 2);
    REQUIRE(configuredIdentity.roles.contains(uo_mule::gui::SourceRole::index));
    REQUIRE(configuredIdentity.roles.contains(uo_mule::gui::SourceRole::data));
}

TEST_CASE("WorkbenchSession_Should_RebindOpenMetadataSourceStatus_If_ProfileChanges")
{
    const TestDirectory directory;
    const auto firstRadarcol = testRoot() / "first-radarcol.mul";
    const auto secondRadarcol = testRoot() / "second-radarcol.mul";
    std::ofstream(firstRadarcol).close();
    std::ofstream(secondRadarcol).close();

    const auto makeSummary = [](const std::filesystem::path & path)
    {
        return uo_mule::gui::DocumentSummary{
            .identity =
                {
                    .kind = uo_mule::gui::DocumentKind::radarcol,
                    .source = uo_mule::gui::makeDocumentSourceIdentity({path}),
                },
            .displayName = path.filename().string(),
            .facetKey = std::nullopt,
            .dimensions = std::nullopt,
        };
    };

    uo_mule::gui::WorkbenchSession session;
    const auto documentId = session.openDocument(makeSummary(firstRadarcol));
    REQUIRE(documentId != 0);
    REQUIRE(session.documents().front().summary.sourceKind == uo_mule::gui::DocumentSourceKind::standalone);

    uo_mule::gui::DataProfile firstProfile;
    firstProfile.sourceRoot = testRoot();
    firstProfile.files[uo_mule::gui::DataFileRole::radarcol] = {.path = firstRadarcol.filename()};
    session.updateProfile(firstProfile);
    REQUIRE(session.documents().front().summary.sourceKind == uo_mule::gui::DocumentSourceKind::configuredMetadata);

    uo_mule::gui::DataProfile secondProfile;
    secondProfile.sourceRoot = testRoot();
    secondProfile.files[uo_mule::gui::DataFileRole::radarcol] = {.path = secondRadarcol.filename()};
    session.updateProfile(secondProfile);
    REQUIRE(session.documents().front().summary.sourceKind == uo_mule::gui::DocumentSourceKind::standalone);
}
