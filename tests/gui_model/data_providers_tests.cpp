#include "gui_model/documents/document_identity.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/providers/data_providers.h"
#include "gui_model/providers/embedded_radarcol.h"
#include "gui_model/source/source_identity.h"
#include "uomul/radarcol/radarcol.h"
#include "uomul/tiledata/tiledata.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <system_error>
#include <vector>

namespace
{

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

uo_mule::gui::DataProfile profileWithRadarcol(const std::filesystem::path & path)
{
    uo_mule::gui::DataProfile profile;
    profile.sourceRoot = path.parent_path();
    profile.files[uo_mule::gui::DataFileRole::radarcol] = {.path = path.filename()};
    return profile;
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

TEST_CASE("DataProviders_Should_FallbackWithWarningWhenConfiguredRadarcolIsBroken")
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

TEST_CASE("DataProviders_Should_IncrementRadarcolRevisionWhenEffectiveSourceChanges")
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
    const auto path = std::filesystem::path(UOMUL_SOURCE_DIR) / "testfiles" / "tiledata.mul";
    REQUIRE(std::filesystem::is_regular_file(path));

    uo_mule::gui::DataProfile profile;
    profile.sourceRoot = path.parent_path();
    profile.files[uo_mule::gui::DataFileRole::tiledata] = uo_mule::gui::DataFileEntry{.path = path.filename()};

    uo_mule::gui::DataProviders providers;
    providers.sync(profile);

    const auto & tiledata = providers.tiledata();
    REQUIRE(tiledata.status().state == uo_mule::gui::ProviderAssetState::available);
    REQUIRE(tiledata.layout() == uomul::TiledataLayout::extended64);
    REQUIRE(tiledata.mapTile(3U)->record.name == "grass");
    REQUIRE(tiledata.staticTile(0U).has_value());
    REQUIRE(tiledata.status(3U).state == uo_mule::gui::ProviderAssetState::available);
    REQUIRE(tiledata.staticStatus(0U).state == uo_mule::gui::ProviderAssetState::available);
}
