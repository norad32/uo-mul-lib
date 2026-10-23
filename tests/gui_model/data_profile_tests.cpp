#include "gui_model/profile/data_discovery.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/profile/data_profile_store.h"
#include "gui_model/profile/data_validation.h"
#include "uomul/radarcol/radarcol.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>

namespace
{

void createFile(const std::filesystem::path & path)
{
    std::filesystem::create_directories(path.parent_path());
    static_cast<void>(std::ofstream(path, std::ios::binary));
}

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
    expected.discoveryMode = uo_mule::gui::DiscoveryMode::installation;
    expected.outputCollisionPolicy = uo_mule::gui::OutputCollisionPolicy::reject;
    expected.files[uo_mule::gui::DataFileRole::texidx] = {.path = "texidx.mul"};
    expected.files[uo_mule::gui::DataFileRole::texmaps] = {.path = "texmaps.mul"};

    uo_mule::gui::saveDataProfile(expected, profilePath);
    const auto loaded = uo_mule::gui::loadDataProfile(profilePath);

    REQUIRE(loaded.diagnostics.empty());
    REQUIRE(loaded.profile.schemaVersion == uo_mule::gui::DataProfile::currentSchemaVersion);
    REQUIRE(loaded.profile.name == expected.name);
    REQUIRE(loaded.profile.sourceRoot == expected.sourceRoot);
    REQUIRE(loaded.profile.discoveryMode == expected.discoveryMode);
    REQUIRE(loaded.profile.outputCollisionPolicy == expected.outputCollisionPolicy);
}

TEST_CASE("DataProfileStore_Should_IgnoreUnknownKeysAndRejectUnsafeProfileNames")
{
    const TemporaryProfileDirectory directory;
    const auto profilePath = directory.path() / "unknown.profile";
    {
        std::ofstream file(profilePath);
        file << "schema_version 0\n"
                "name \"Fixture\"\n"
                "unknown_setting \"ignored\"\n"
                "map0.path \"map0.mul\"\n";
    }

    const auto loaded = uo_mule::gui::loadDataProfile(profilePath);
    REQUIRE(loaded.profile.files.empty());
    REQUIRE(loaded.diagnostics.size() == 2);

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

TEST_CASE("DataProfileStore_Should_DeleteProfile")
{
    const TemporaryProfileDirectory directory;
    const uo_mule::gui::DataProfileStore store(directory.path());
    uo_mule::gui::DataProfile profile;
    profile.name = "To Delete";
    store.save(profile);

    REQUIRE(std::filesystem::is_regular_file(store.path(profile.name)));
    store.remove(profile.name);
    REQUIRE_FALSE(std::filesystem::exists(store.path(profile.name)));
}

TEST_CASE("DataProfileValidation_Should_ReportIncompleteTexturePair")
{
    const TemporaryProfileDirectory directory;
    const auto source = directory.path() / "source";
    std::filesystem::create_directories(source);
    {
        std::ofstream(source / "texidx.mul");
    }

    uo_mule::gui::DataProfile profile;
    profile.sourceRoot = source;
    profile.files[uo_mule::gui::DataFileRole::texidx] = {.path = "texidx.mul"};

    const auto validation = uo_mule::gui::validateDataProfile(profile);

    REQUIRE(validation.hasErrors());
    REQUIRE(validation.find(uo_mule::gui::DataFileRole::texidx)->status == uo_mule::gui::DataFileStatus::found);
    REQUIRE(validation.find(uo_mule::gui::DataFileRole::texmaps)->status == uo_mule::gui::DataFileStatus::missing);
    REQUIRE(validation.diagnostics.size() == 1);
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

TEST_CASE("DataDiscovery_Should_FindRootAndKnownChildFilesCaseInsensitively")
{
    const TemporaryProfileDirectory directory;
    const auto root = directory.path() / "installation";
    createFile(root / "Data" / "RADARCOL.MUL");
    {
        std::ofstream radarcol(root / "Data" / "RADARCOL.MUL", std::ios::binary);
        radarcol.put('\0');
    }
    createFile(root / "Client" / "MUL" / "TeXiDx.MuL");
    createFile(root / "Client" / "MUL" / "TeXmApS.MuL");
    createFile(root / "tiledata.mul");

    const auto result = uo_mule::gui::discoverDataFiles(root, uo_mule::gui::DiscoveryMode::installation);

    REQUIRE(result.diagnostics.size() >= 1);
    REQUIRE(result.find(uo_mule::gui::DataFileRole::radarcol) != nullptr);
    REQUIRE(result.find(uo_mule::gui::DataFileRole::radarcol)->status == uo_mule::gui::DataFileStatus::invalid);
    REQUIRE(result.find(uo_mule::gui::DataFileRole::texidx)->status == uo_mule::gui::DataFileStatus::found);
    REQUIRE(result.find(uo_mule::gui::DataFileRole::texmaps)->status == uo_mule::gui::DataFileStatus::found);
    REQUIRE(result.find(uo_mule::gui::DataFileRole::tiledata)->status == uo_mule::gui::DataFileStatus::invalid);
}

TEST_CASE("DataDiscovery_Should_NotRecognizeMapFilesAsDataPaths")
{
    const TemporaryProfileDirectory directory;
    const auto root = directory.path() / "ambiguous";
    createFile(root / "Data" / "map0.mul");
    createFile(root / "Client" / "map0.mul");
    const auto result = uo_mule::gui::discoverDataFiles(root, uo_mule::gui::DiscoveryMode::folder);
    REQUIRE(result.files.size() == uo_mule::gui::knownDataFileRoles().size());
    REQUIRE(result.find(uo_mule::gui::DataFileRole::radarcol)->status == uo_mule::gui::DataFileStatus::missing);
}

TEST_CASE("DataDiscovery_Should_ClearAmbiguousAutomaticPathButPreserveOverride")
{
    const TemporaryProfileDirectory directory;
    const auto root = directory.path() / "rescan";
    uo_mule::gui::DataProfile profile;
    const auto firstScan = uo_mule::gui::discoverDataFiles(root, uo_mule::gui::DiscoveryMode::folder);
    uo_mule::gui::applyDiscovery(profile, firstScan);
    REQUIRE(profile.files.empty());
}
