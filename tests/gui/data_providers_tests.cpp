#include "gui/providers/data_providers.h"
#include "gui/providers/embedded_radarcol.h"
#include "uomul/radarcol/radarcol.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
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
    file.write(reinterpret_cast<const char *>(values.data()), static_cast<std::streamsize>(values.size() * sizeof(std::uint16_t)));
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

TEST_CASE("DataProviders_Should_UseValidExternalRadarcol")
{
    const TemporaryDirectory directory;
    const auto path = directory.path() / "radarcol.mul";
    createRadarcol(path, 0x7FFFU);

    uo_mule::gui::DataProviders providers;
    providers.sync(profileWithRadarcol(path));

    const auto status = providers.radarColors().status();
    REQUIRE(status.state == uo_mule::gui::ProviderAssetState::available);
    REQUIRE(status.source == uo_mule::gui::ProviderSourceKind::externalConfigured);
    REQUIRE(status.revision == 1);
    REQUIRE(providers.radarColors().mapColor(0U).value() == uo_mule::gui::MapColor{.red = 255, .green = 255, .blue = 255});
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
    createRadarcol(path, 0x7FFFU);
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
