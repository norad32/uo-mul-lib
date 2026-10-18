#include "gui/data_profile.h"
#include "gui/data_profile_store.h"
#include "gui/data_validation.h"
#include "uomul/map.h"
#include "uomul/radarcol/radarcol.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>

namespace
{

constexpr auto feluccaWidth = 6144U;
constexpr auto feluccaHeight = 4096U;

class TemporaryProfileDirectory
{
public:
    TemporaryProfileDirectory() : path_(std::filesystem::temp_directory_path() / "uo-mule-data-profile-tests")
    {
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }

    ~TemporaryProfileDirectory() { std::filesystem::remove_all(path_); }

    TemporaryProfileDirectory(const TemporaryProfileDirectory &) = delete;
    TemporaryProfileDirectory & operator=(const TemporaryProfileDirectory &) = delete;
    TemporaryProfileDirectory(TemporaryProfileDirectory &&) = delete;
    TemporaryProfileDirectory & operator=(TemporaryProfileDirectory &&) = delete;

    [[nodiscard]] const std::filesystem::path & path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("DataProfile_Should_RoundTripRolePathsAndPolicies")
{
    const TemporaryProfileDirectory directory;
    const auto profilePath = directory.path() / "classic.profile";
    uo_mule::gui::DataProfile expected;
    expected.name = "Classic UO Felucca";
    expected.sourceRoot = directory.path() / "source";
    expected.outputRoot = directory.path() / "output";
    expected.discoveryMode = uo_mule::gui::DiscoveryMode::installation;
    expected.createOutputIfMissing = false;
    expected.useSourceFolderForOutput = false;
    expected.outputCollisionPolicy = uo_mule::gui::OutputCollisionPolicy::reject;
    expected.files[uo_mule::gui::DataFileRole::map0] = {
        .path = "map0.mul",
        .userOverride = true,
    };
    expected.files[uo_mule::gui::DataFileRole::texidx] = {.path = "texidx.mul"};
    expected.files[uo_mule::gui::DataFileRole::texmaps] = {.path = "texmaps.mul"};
    expected.facets.at(0).facet = "felucca";
    expected.facets.at(0).dimensions = uomul::Dimensions{.widthTiles = feluccaWidth, .heightTiles = feluccaHeight};
    expected.facets.at(0).recoverTruncated = true;

    uo_mule::gui::saveDataProfile(expected, profilePath);
    const auto loaded = uo_mule::gui::loadDataProfile(profilePath);

    REQUIRE(loaded.diagnostics.empty());
    REQUIRE(loaded.profile.schemaVersion == uo_mule::gui::DataProfile::currentSchemaVersion);
    REQUIRE(loaded.profile.name == expected.name);
    REQUIRE(loaded.profile.sourceRoot == expected.sourceRoot);
    REQUIRE(loaded.profile.outputRoot == expected.outputRoot);
    REQUIRE(loaded.profile.discoveryMode == expected.discoveryMode);
    REQUIRE(loaded.profile.outputCollisionPolicy == expected.outputCollisionPolicy);
    REQUIRE(loaded.profile.files.at(uo_mule::gui::DataFileRole::map0).path == "map0.mul");
    REQUIRE(loaded.profile.files.at(uo_mule::gui::DataFileRole::map0).userOverride);
    REQUIRE(loaded.profile.facets.at(0).dimensions == expected.facets.at(0).dimensions);
    REQUIRE(loaded.profile.facets.at(0).recoverTruncated);
}

TEST_CASE("DataProfileStore_Should_IgnoreUnknownKeysAndRejectUnsafeProfileNames")
{
    const TemporaryProfileDirectory directory;
    const auto profilePath = directory.path() / "unknown.profile";
    {
        std::ofstream file(profilePath);
        file << "schema_version 1\n"
                "name \"Fixture\"\n"
                "unknown_setting \"ignored\"\n"
                "map0.path \"map0.mul\"\n";
    }

    const auto loaded = uo_mule::gui::loadDataProfile(profilePath);
    REQUIRE(loaded.profile.files.at(uo_mule::gui::DataFileRole::map0).path == "map0.mul");
    REQUIRE(loaded.diagnostics.size() == 1);
    REQUIRE(loaded.diagnostics.front().severity == uo_mule::gui::Diagnostic::Severity::info);

    const auto safePath = uo_mule::gui::dataProfilePath("../secret/profile", directory.path());
    REQUIRE(safePath.parent_path() == directory.path());
    REQUIRE(safePath.filename() == "___secret_profile.profile");
}

TEST_CASE("DataProfileStore_Should_ReturnDefaultsWhenProfileIsMissing")
{
    const TemporaryProfileDirectory directory;

    const auto loaded = uo_mule::gui::loadDataProfile(directory.path() / "missing.profile");

    REQUIRE(loaded.diagnostics.empty());
    REQUIRE(loaded.profile.schemaVersion == uo_mule::gui::DataProfile::currentSchemaVersion);
    REQUIRE(loaded.profile.name.empty());
}

TEST_CASE("DataProfileValidation_Should_ReportTexturePairAndOutputSafety")
{
    const TemporaryProfileDirectory directory;
    const auto source = directory.path() / "source";
    std::filesystem::create_directories(source);
    {
        std::ofstream(source / "texidx.mul");
    }

    uo_mule::gui::DataProfile profile;
    profile.sourceRoot = source;
    profile.outputRoot = source;
    profile.files[uo_mule::gui::DataFileRole::texidx] = {.path = "texidx.mul"};

    const auto validation = uo_mule::gui::validateDataProfile(profile);

    REQUIRE(validation.hasErrors());
    REQUIRE(validation.find(uo_mule::gui::DataFileRole::texidx)->status == uo_mule::gui::DataFileStatus::found);
    REQUIRE(validation.find(uo_mule::gui::DataFileRole::texmaps)->status == uo_mule::gui::DataFileStatus::missing);
    REQUIRE(validation.diagnostics.size() >= 2);
}

TEST_CASE("DataProfileValidation_Should_PreserveExplicitOverrideStatus")
{
    const TemporaryProfileDirectory directory;
    const auto source = directory.path() / "source";
    std::filesystem::create_directories(source);
    {
        std::ofstream radarcol(source / "radarcol.mul", std::ios::binary);
        radarcol.seekp(static_cast<std::streamoff>((uomul::radarColorCount * sizeof(uomul::RadarColor)) - 1));
        radarcol.put('\0');
    }

    uo_mule::gui::DataProfile profile;
    profile.sourceRoot = source;
    profile.files[uo_mule::gui::DataFileRole::radarcol] = {
        .path = "radarcol.mul",
        .userOverride = true,
    };

    const auto validation = uo_mule::gui::validateDataProfile(profile);
    REQUIRE(validation.find(uo_mule::gui::DataFileRole::radarcol)->status == uo_mule::gui::DataFileStatus::overridden);
}
