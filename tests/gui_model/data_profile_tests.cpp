#include "gui_model/profile/data_discovery.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/profile/data_profile_store.h"
#include "gui_model/profile/data_validation.h"
#include "gui_model/tools/tool_contracts.h"
#include "uomul/radarcol/radarcol.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>

namespace
{

constexpr int profileWindowX = 37;
constexpr int profileWindowY = 42;
constexpr int profileWindowWidth = 1440;
constexpr int profileWindowHeight = 900;

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
    expected.preferences.showLogPanel = true;
    expected.preferences.fullscreen = true;
    expected.preferences.maximized = true;
    expected.preferences.tools.activeTool = uo_mule::gui::ToolId::replace;
    expected.preferences.tools.fuzzy.nameEnabled = true;
    expected.preferences.tools.fuzzy.name = "granite floor";
    expected.layout.window = {
        .geometryValid = true,
        .x = profileWindowX,
        .y = profileWindowY,
        .width = profileWindowWidth,
        .height = profileWindowHeight,
    };
    expected.layout.dockingIni = "[Window][Map]\nDockId=0x1234,1\n";
    expected.layout.panels["panel.tool-palette"] = false;
    expected.layout.windows["window.texmaps"] = false;

    uo_mule::gui::saveDataProfile(expected, profilePath);
    const auto loaded = uo_mule::gui::loadDataProfile(profilePath);

    REQUIRE(loaded.diagnostics.empty());
    REQUIRE(loaded.profile.schemaVersion == uo_mule::gui::DataProfile::currentSchemaVersion);
    REQUIRE(loaded.profile.name == expected.name);
    REQUIRE(loaded.profile.sourceRoot == expected.sourceRoot);
    REQUIRE(loaded.profile.discoveryMode == expected.discoveryMode);
    REQUIRE(loaded.profile.outputCollisionPolicy == expected.outputCollisionPolicy);
    REQUIRE(loaded.profile.preferences.showLogPanel);
    REQUIRE(loaded.profile.preferences.fullscreen);
    REQUIRE(loaded.profile.preferences.maximized);
    REQUIRE(loaded.profile.preferences.tools.activeTool == uo_mule::gui::ToolId::replace);
    REQUIRE(loaded.profile.preferences.tools.fuzzy.name == "granite floor");
    REQUIRE(loaded.profile.layout.window.geometryValid);
    REQUIRE(loaded.profile.layout.window.width == 1440);
    REQUIRE(loaded.profile.layout.window.height == 900);
    REQUIRE(loaded.profile.layout.dockingIni == expected.layout.dockingIni);
    REQUIRE_FALSE(loaded.profile.layout.panels.at("panel.tool-palette"));
    REQUIRE_FALSE(loaded.profile.layout.windows.at("window.texmaps"));
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

TEST_CASE("DataProfileStore_Should_ReturnDefaults_If_ProfileIsMissing")
{
    const TemporaryProfileDirectory directory;

    const auto loaded = uo_mule::gui::loadDataProfile(directory.path() / "missing.profile");

    REQUIRE(loaded.diagnostics.empty());
    REQUIRE(loaded.profile.schemaVersion == uo_mule::gui::DataProfile::currentSchemaVersion);
    REQUIRE(loaded.profile.name.empty());
}

TEST_CASE("DataProfileStore_Should_RejectMissingSchemaAndPreserveFile")
{
    const TemporaryProfileDirectory directory;
    const auto profilePath = directory.path() / "invalid.profile";
    {
        std::ofstream file(profilePath);
        file << "name \"legacy\"\n";
    }

    const auto loaded = uo_mule::gui::loadDataProfile(profilePath);
    std::ifstream file(profilePath);
    const std::string preserved((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    REQUIRE(loaded.profile.schemaVersion == uo_mule::gui::DataProfile::currentSchemaVersion);
    REQUIRE(loaded.profile.name.empty());
    REQUIRE(loaded.profile.sourceRoot.empty());
    REQUIRE(loaded.profile.files.empty());
    REQUIRE(loaded.profile.preferences == uo_mule::gui::Preferences{});
    REQUIRE(loaded.profile.layout.dockingIni.empty());
    REQUIRE_FALSE(loaded.diagnostics.empty());
    REQUIRE(preserved == "name \"legacy\"\n");
}

TEST_CASE("DataProfileStore_Should_RejectNonzeroSchemaAndPreserveFile")
{
    const TemporaryProfileDirectory directory;
    const auto profilePath = directory.path() / "invalid.profile";
    {
        std::ofstream file(profilePath);
        file << "schema_version 1\nname \"old\"\n";
    }

    const auto loaded = uo_mule::gui::loadDataProfile(profilePath);
    std::ifstream file(profilePath);
    const std::string preserved((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    REQUIRE(loaded.profile.name.empty());
    REQUIRE_FALSE(loaded.diagnostics.empty());
    REQUIRE(preserved == "schema_version 1\nname \"old\"\n");
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

TEST_CASE("DataProfileStore_Should_ValidateProfileNamesAndSanitizedCollisions")
{
    const TemporaryProfileDirectory directory;
    const uo_mule::gui::DataProfileStore store(directory.path());

    REQUIRE_FALSE(store.validateProfileName("").valid());
    REQUIRE_FALSE(store.validateProfileName("   \t").valid());
    REQUIRE_FALSE(store.validateProfileName("path/to-profile").valid());
    REQUIRE_FALSE(store.validateProfileName("path\\to-profile").valid());
    REQUIRE_FALSE(store.validateProfileName(".").valid());
    REQUIRE_FALSE(store.validateProfileName("..").valid());
    REQUIRE(store.validateProfileName("distinct-profile").valid());

    uo_mule::gui::DataProfile existing;
    existing.name = "Friendly Profile";
    store.save(existing);

    const auto sameName = store.validateProfileName(existing.name);
    REQUIRE(sameName.valid());

    const auto collision = store.validateProfileName("Friendly_Profile");
    REQUIRE_FALSE(collision.valid());
    REQUIRE(collision.message.find("Friendly Profile") != std::string::npos);
}

TEST_CASE("DataProfileValidation_Should_ReportIncompleteTexmapsSet")
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

TEST_CASE("DataProfileValidation_Should_RejectEmptyProfile")
{
    const uo_mule::gui::DataProfile profile;

    const auto validation = uo_mule::gui::validateDataProfile(profile);

    REQUIRE(validation.hasErrors());
    REQUIRE(validation.diagnostics.size() == 1);
    REQUIRE(validation.diagnostics.front().severity == uo_mule::gui::Diagnostic::Severity::error);
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
