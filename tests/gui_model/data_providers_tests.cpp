#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/documents/map_document.h"
#include "gui_model/documents/radarcol_document.h"
#include "gui_model/documents/texmaps_document.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/providers/data_providers.h"
#include "gui_model/providers/embedded_radarcol.h"
#include "gui_model/source/source_identity.h"
#include "uomul/radarcol/radarcol.h"
#include "uomul/texmaps/texmaps.h"
#include "uomul/tiledata/tiledata.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace
{

constexpr std::uint16_t replacementRadarColor =
    0x7FFFU; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
constexpr std::uint16_t replacementTexturePixel =
    0x1234U; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

class TemporaryDirectory
{
public:
    TemporaryDirectory() : path_(std::filesystem::temp_directory_path() / "uo-mule-data-providers-tests")
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        std::filesystem::create_directories(path_, error);
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory &) = delete;
    TemporaryDirectory & operator=(const TemporaryDirectory &) = delete;
    TemporaryDirectory(TemporaryDirectory &&) = delete;
    TemporaryDirectory & operator=(TemporaryDirectory &&) = delete;

    [[nodiscard]] const std::filesystem::path & path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

void createRadarcol(const std::filesystem::path & path, std::uint16_t firstColor)
{
    const std::vector<std::uint16_t> colors(uomul::radarColorCount, 0U);
    auto values = colors;
    values.front() = firstColor;
    std::ofstream file(path, std::ios::binary);
    for (const std::uint16_t value : values)
    {
        file.put(static_cast<char>(value & 0xFFU)); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
        file.put(static_cast<char>(value >> 8U));   // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    }
}

void createTiledata(const std::filesystem::path & path)
{
    auto editor = uomul::TiledataEditor::createEmpty(path, uomul::TiledataLayout::extended64);
    REQUIRE(editor);
    REQUIRE(editor->editMapTile(3, {.name = "grass"}));
    REQUIRE(editor->commit());
}

uo_mule::gui::DataProfile profileWithRadarcol(const std::filesystem::path & path)
{
    uo_mule::gui::DataProfile profile;
    profile.sourceRoot = path.parent_path();
    profile.files[uo_mule::gui::DataFileRole::radarcol] = {.path = path.filename()};
    return profile;
}

uo_mule::gui::DataProfile profileWithTexmaps(const std::filesystem::path & indexPath, const std::filesystem::path & dataPath)
{
    uo_mule::gui::DataProfile profile;
    profile.sourceRoot = indexPath.parent_path();
    profile.files[uo_mule::gui::DataFileRole::texidx] = {.path = indexPath.filename()};
    profile.files[uo_mule::gui::DataFileRole::texmaps] = {.path = dataPath.filename()};
    return profile;
}

void createTexmaps(const std::filesystem::path & indexPath, const std::filesystem::path & dataPath)
{
    auto editor = uomul::TexMapsEditor::createEmpty(indexPath, dataPath);
    REQUIRE(editor);
    REQUIRE(editor->commit());
}

} // namespace

TEST_CASE("DataProviders_Should_UseEmbeddedRadarcolWithoutAProfile")
{
    uo_mule::gui::DataProviders providers;
    providers.sync({});

    const auto status = providers.radarColors().status();
    REQUIRE(status.state == uo_mule::gui::ProviderAssetState::available);
    REQUIRE(status.source == uo_mule::gui::ProviderSourceKind::embeddedDefault);
    REQUIRE(status.revision == 1);
    REQUIRE(providers.radarColors().mapColor(0U) == uo_mule::gui::makeEmbeddedRadarColorProvider()->mapColor(0U));
}

TEST_CASE("providerSourceName_Should_DescribeEffectiveRadarcolSources")
{
    REQUIRE(uo_mule::gui::providerSourceName(uo_mule::gui::ProviderSourceKind::embeddedDefault) == "Embedded Radarcol");
    REQUIRE(uo_mule::gui::providerSourceName(uo_mule::gui::ProviderSourceKind::externalConfigured) == "Configured Radarcol");
    REQUIRE(uo_mule::gui::providerSourceName(uo_mule::gui::ProviderSourceKind::embeddedDefaultWithWarning) ==
            "Embedded Radarcol (fallback)");
}

TEST_CASE("DataProviders_Should_UseValidExternalRadarcol")
{
    const TemporaryDirectory directory;
    const auto path = directory.path() / "radarcol.mul";
    createRadarcol(path, 0x7FFFU); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    uo_mule::gui::DataProviders providers;
    providers.sync(profileWithRadarcol(path));

    const auto status = providers.radarColors().status();
    REQUIRE(status.state == uo_mule::gui::ProviderAssetState::available);
    REQUIRE(status.source == uo_mule::gui::ProviderSourceKind::externalConfigured);
    REQUIRE(status.revision == 1);
    REQUIRE(providers.radarColors().mapColor(0U).value_or(uo_mule::gui::MapColor{}) ==
            uo_mule::gui::MapColor{.red = 255, .green = 255, .blue = 255});
}

TEST_CASE("DataProviders_Should_FallbackWithWarning_If_ConfiguredRadarcolIsBroken")
{
    const TemporaryDirectory directory;
    const auto path = directory.path() / "radarcol.mul";
    {
        std::ofstream file(path, std::ios::binary);
        file.put('\0');
    }

    uo_mule::gui::DataProviders providers;
    providers.sync(profileWithRadarcol(path));

    const auto status = providers.radarColors().status();
    REQUIRE(status.state == uo_mule::gui::ProviderAssetState::available);
    REQUIRE(status.source == uo_mule::gui::ProviderSourceKind::embeddedDefaultWithWarning);
    REQUIRE(status.message.find("Configured Radarcol unavailable") != std::string::npos);
    REQUIRE(providers.radarColors().mapColor(0U) == uo_mule::gui::makeEmbeddedRadarColorProvider()->mapColor(0U));
}

TEST_CASE("DataProviders_Should_IncrementRadarcolRevision_If_EffectiveSourceChanges")
{
    const TemporaryDirectory directory;
    const auto path = directory.path() / "radarcol.mul";
    createRadarcol(path, 0x7FFFU); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    const auto external = profileWithRadarcol(path);

    uo_mule::gui::DataProviders providers;
    providers.sync({});
    const auto embeddedRevision = providers.radarColors().status().revision;
    providers.sync(external);
    const auto externalRevision = providers.radarColors().status().revision;
    providers.sync({});

    REQUIRE(externalRevision > embeddedRevision);
    REQUIRE(providers.radarColors().status().revision > externalRevision);
    REQUIRE(providers.radarColors().status().source == uo_mule::gui::ProviderSourceKind::embeddedDefault);
}

TEST_CASE("DataProviders_Should_NotRefreshUnchangedPathsDuringSync")
{
    const TemporaryDirectory directory;
    const auto path = directory.path() / "radarcol.mul";
    createRadarcol(path, 0x7FFFU); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    const auto profile = profileWithRadarcol(path);

    uo_mule::gui::DataProviders providers;
    providers.sync(profile);
    const auto revision = providers.radarColors().status().revision;

    providers.sync(profile);
    REQUIRE(providers.radarColors().status().revision == revision);
}

TEST_CASE("DataProviders_Should_RefreshSamePathAndAdvanceRevision")
{
    const TemporaryDirectory directory;
    const auto path = directory.path() / "radarcol.mul";
    createRadarcol(path, 0x7FFFU); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    const auto profile = profileWithRadarcol(path);

    uo_mule::gui::DataProviders providers;
    providers.sync(profile);
    const auto before = providers.radarColors().status();
    REQUIRE(providers.radarColors().mapColor(0U) == uo_mule::gui::MapColor{.red = 255, .green = 255, .blue = 255});

    createRadarcol(path, 0x001FU); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE(providers.refresh(profile));
    const auto after = providers.radarColors().status();

    REQUIRE(after.revision > before.revision);
    REQUIRE(after.sourceIdentity == before.sourceIdentity);
    REQUIRE(after.source == before.source);
    REQUIRE(providers.radarColors().mapColor(0U) == uo_mule::gui::MapColor{.red = 0, .green = 0, .blue = 255});
}

TEST_CASE("DataProviders_Should_ExposeEffectiveRadarcolSourceIdentity")
{
    const TemporaryDirectory directory;
    const auto path = directory.path() / "radarcol.mul";
    createRadarcol(path, 0x7FFFU); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    uo_mule::gui::DataProviders providers;
    providers.sync(profileWithRadarcol(path));

    REQUIRE(providers.radarColors().status().sourceIdentity == uo_mule::gui::makeDocumentSourceIdentity({path}));
    REQUIRE(providers.radarColors().rawColor(0U).has_value());
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("DataProviders_Should_SynchronizeConfiguredRadarcolDocumentChanges")
{
    const TemporaryDirectory directory;
    const auto path = directory.path() / "radarcol.mul";
    createRadarcol(path, 0U);
    const auto profile = profileWithRadarcol(path);

    uo_mule::gui::DocumentManager documents;
    documents.updateMetadataSources(profile);
    const auto documentId = documents.open({
        .identity = {.kind = uo_mule::gui::DocumentKind::radarcol, .source = uo_mule::gui::makeDocumentSourceIdentity({path})},
        .displayName = path.filename().string(),
        .sourceKind = uo_mule::gui::DocumentSourceKind::configuredMetadata,
        .validation = uo_mule::gui::DocumentValidationState::valid,
        .facetKey = std::nullopt,
        .dimensions = std::nullopt,
        .dirty = false,
    });
    REQUIRE(documentId != 0U);

    uo_mule::gui::DataProviders providers;
    providers.sync(profile);
    providers.syncDocuments(documents.documents());
    const auto before = providers.radarColors().status().revision;
    REQUIRE(providers.radarColors().rawColor(0U) == std::optional<uomul::RadarColor>{0U});

    auto * record = documents.findMutable(documentId);
    REQUIRE(record != nullptr);
    REQUIRE(record->radarcol.has_value());
    if (record == nullptr || !record->radarcol)
    {
        return;
    }
    auto & radarcol = *record->radarcol;
    REQUIRE(radarcol.handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::editColors,
        .colorEdits = {{.colorIndex = 0U, .color = replacementRadarColor}},
        .outputPath = {},
        .overwriteOutput = false,
    }));

    providers.syncDocuments(documents.documents());
    REQUIRE(providers.radarColors().status().revision > before);
    REQUIRE(providers.radarColors().rawColor(0U) == std::optional<uomul::RadarColor>{replacementRadarColor});

    REQUIRE(radarcol.handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::discard,
        .colorEdits = {},
        .outputPath = {},
        .overwriteOutput = false,
    }));
    providers.syncDocuments(documents.documents());
    REQUIRE(providers.radarColors().rawColor(0U) == std::optional<uomul::RadarColor>{0U});
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("DataProviders_Should_SynchronizeConfiguredTexmapsDocumentChanges")
{
    const TemporaryDirectory directory;
    const auto indexPath = directory.path() / "texidx.mul";
    const auto dataPath = directory.path() / "texmaps.mul";
    createTexmaps(indexPath, dataPath);
    const auto profile = profileWithTexmaps(indexPath, dataPath);

    uo_mule::gui::DocumentManager documents;
    documents.updateMetadataSources(profile);
    const auto documentId = documents.open({
        .identity =
            {
                .kind = uo_mule::gui::DocumentKind::texmapPair,
                .source = uo_mule::gui::makeRoleAwareDocumentSourceIdentity({
                    {.role = uo_mule::gui::SourceRole::index, .path = indexPath},
                    {.role = uo_mule::gui::SourceRole::data, .path = dataPath},
                }),
            },
        .displayName = indexPath.filename().string(),
        .sourceKind = uo_mule::gui::DocumentSourceKind::configuredMetadata,
        .validation = uo_mule::gui::DocumentValidationState::valid,
        .facetKey = std::nullopt,
        .dimensions = std::nullopt,
        .dirty = false,
    });
    REQUIRE(documentId != 0U);

    uo_mule::gui::DataProviders providers;
    providers.sync(profile);
    providers.syncDocuments(documents.documents());
    const auto before = providers.textures().status().revision;
    REQUIRE_FALSE(providers.textures().hasTexture(0U).value_or(true));

    auto * record = documents.findMutable(documentId);
    REQUIRE(record != nullptr);
    REQUIRE(record->texmaps.has_value());
    if (record == nullptr || !record->texmaps)
    {
        return;
    }
    auto & texmaps = *record->texmaps;
    const uomul::TextureImage replacement{
        .dimension = uomul::textureSmallDimension,
        .pixels = std::vector<std::uint16_t>(uomul::textureSmallPixelCount, replacementTexturePixel),
    };
    REQUIRE(texmaps.handleIntent({
        .kind = uo_mule::gui::TexmapsDocumentIntentKind::replace,
        .outputIndexPath = {},
        .outputDataPath = {},
        .overwriteOutput = false,
        .textureId = 0U,
        .texture = replacement,
        .textureEdits = {},
        .textureIds = {},
    }));

    providers.syncDocuments(documents.documents());
    REQUIRE(providers.textures().status().revision > before);
    REQUIRE(providers.textures().hasTexture(0U).value_or(false));

    REQUIRE(texmaps.handleIntent({
        .kind = uo_mule::gui::TexmapsDocumentIntentKind::discard,
        .outputIndexPath = {},
        .outputDataPath = {},
        .overwriteOutput = false,
        .textureId = 0U,
        .texture = {},
        .textureEdits = {},
        .textureIds = {},
    }));
    providers.syncDocuments(documents.documents());
    REQUIRE_FALSE(providers.textures().hasTexture(0U).value_or(true));
}

TEST_CASE("DataProviders_Should_ExposeStableUnavailableMetadataSourceStatus")
{
    uo_mule::gui::DataProviders providers;
    providers.sync({});

    const auto textureStatus = providers.textures().status();
    const auto tileDataStatus = providers.tiledata().status();

    REQUIRE(textureStatus.state == uo_mule::gui::ProviderAssetState::unavailable);
    REQUIRE(textureStatus.sourceIdentity.roles.size() == 2U);
    REQUIRE(textureStatus.sourceIdentity.path(uo_mule::gui::SourceRole::index)->empty());
    REQUIRE(textureStatus.sourceIdentity.path(uo_mule::gui::SourceRole::data)->empty());
    REQUIRE(textureStatus.revision > 0U);
    REQUIRE(tileDataStatus.state == uo_mule::gui::ProviderAssetState::unavailable);
    REQUIRE(tileDataStatus.sourceIdentity.roles.empty());
    REQUIRE(tileDataStatus.revision > 0U);
}

TEST_CASE("DataProviders_Should_ReadConfiguredTiledataMapAndStaticRecords")
{
    const TemporaryDirectory directory;
    const auto path = directory.path() / "tiledata.mul";
    createTiledata(path);

    uo_mule::gui::DataProfile profile;
    profile.sourceRoot = path.parent_path();
    profile.files[uo_mule::gui::DataFileRole::tiledata] = uo_mule::gui::DataFileEntry{.path = path.filename()};

    uo_mule::gui::DataProviders providers;
    providers.sync(profile);

    const auto & tiledata = providers.tiledata();
    REQUIRE(tiledata.status().state == uo_mule::gui::ProviderAssetState::available);
    REQUIRE(tiledata.layout() == uomul::TiledataLayout::extended64);
    const auto mapTile = tiledata.mapTile(3U);
    REQUIRE(mapTile.has_value());
    const auto mapTileValue = mapTile.value_or(uo_mule::gui::ProviderMapTile{});
    REQUIRE(mapTileValue.record.name == "grass");
    REQUIRE(tiledata.staticTile(0U).has_value());
    REQUIRE(tiledata.status(3U).state == uo_mule::gui::ProviderAssetState::available);
    REQUIRE(tiledata.staticStatus(0U).state == uo_mule::gui::ProviderAssetState::available);
}

TEST_CASE("DataProviders_Should_SynchronizeConfiguredTiledataDocument")
{
    const TemporaryDirectory directory;
    const auto path = directory.path() / "tiledata.mul";
    createTiledata(path);

    uo_mule::gui::DataProfile profile;
    profile.sourceRoot = path.parent_path();
    profile.files[uo_mule::gui::DataFileRole::tiledata] = {.path = path.filename()};

    uo_mule::gui::DocumentManager documents;
    documents.updateMetadataSources(profile);
    const auto documentId = documents.open({
        .identity =
            {
                .kind = uo_mule::gui::DocumentKind::tiledata,
                .source = uo_mule::gui::makeDocumentSourceIdentity({path}),
            },
        .displayName = path.filename().string(),
        .sourceKind = uo_mule::gui::DocumentSourceKind::configuredMetadata,
        .validation = uo_mule::gui::DocumentValidationState::valid,
        .facetKey = std::nullopt,
        .dimensions = std::nullopt,
        .dirty = false,
    });
    REQUIRE(documentId != 0U);

    uo_mule::gui::DataProviders providers;
    providers.sync(profile);
    const auto profileRevision = providers.tiledata().status().revision;
    providers.syncDocuments(documents.documents());

    REQUIRE(providers.tiledata().status().revision > profileRevision);
    REQUIRE(providers.tiledata().status().sourceIdentity == uo_mule::gui::makeDocumentSourceIdentity({path}));
    const auto tile = providers.tiledata().mapTile(3U);
    REQUIRE(tile.has_value());
    const auto tileName = tile.has_value() ? tile->record.name : std::string{};
    REQUIRE(tileName == "grass");
}

TEST_CASE("DataProviders_Should_PreserveInvalidConfiguredTiledataDocumentStatus")
{
    const TemporaryDirectory directory;
    const auto path = directory.path() / "broken-tiledata.mul";
    std::ofstream(path, std::ios::binary).put('\0');

    uo_mule::gui::DataProfile profile;
    profile.sourceRoot = path.parent_path();
    profile.files[uo_mule::gui::DataFileRole::tiledata] = {.path = path.filename()};

    uo_mule::gui::DocumentManager documents;
    documents.updateMetadataSources(profile);
    const auto documentId = documents.open({
        .identity =
            {
                .kind = uo_mule::gui::DocumentKind::tiledata,
                .source = uo_mule::gui::makeDocumentSourceIdentity({path}),
            },
        .displayName = path.filename().string(),
        .facetKey = std::nullopt,
        .dimensions = std::nullopt,
    });
    REQUIRE(documentId != 0U);

    uo_mule::gui::DataProviders providers;
    providers.sync(profile);
    providers.syncDocuments(documents.documents());
    const auto status = providers.tiledata().status();

    REQUIRE(status.state == uo_mule::gui::ProviderAssetState::invalid);
    REQUIRE(status.source == uo_mule::gui::ProviderSourceKind::externalConfigured);
    REQUIRE(status.sourceIdentity == uo_mule::gui::makeDocumentSourceIdentity({path}));
    REQUIRE_FALSE(status.message.empty());
    REQUIRE_FALSE(providers.tiledata().mapTile(3U));
}
