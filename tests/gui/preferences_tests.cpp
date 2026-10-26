#include "core/app_info.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/persistence/preferences_store.h"
#include "gui_model/tools/tool_contracts.h"
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace
{

constexpr float heightTolerance = 0.001F;
constexpr float expectedLogPanelHeight = 268.5F;

} // namespace

TEST_CASE("loadPreferences_Should_UseDefaults_If_FileIsMissing")
{
    const auto preferencesFile = std::filesystem::temp_directory_path() / "pref-defaults" / "preferences.conf";
    std::filesystem::remove_all(preferencesFile.parent_path());

    const auto preferences = uo_mule::gui::loadPreferences(preferencesFile);

    REQUIRE_FALSE(preferences.showLogPanel);
    REQUIRE(preferences.autoScroll);
    REQUIRE_FALSE(preferences.logPanelCollapsed);
    REQUIRE(std::abs(preferences.logPanelHeight) < heightTolerance);
    REQUIRE(preferences.showWelcomeOnStartup);
    REQUIRE(preferences.idFormat == uo_mule::gui::IdFormat::hexadecimal);
    REQUIRE(preferences.texmapsHideUnused);
    REQUIRE_FALSE(preferences.texmapsTablePresentation);
    std::filesystem::remove_all(preferencesFile.parent_path());
}

TEST_CASE("savePreferences_Should_PersistOnlyActiveProfileSelector")
{
    const auto configDirectory = std::filesystem::temp_directory_path() / "pref-roundtrip";
    const auto preferencesFile = configDirectory / std::string(uo_mule::core::applicationName()) / "preferences.conf";
    std::filesystem::remove_all(configDirectory);
    const uo_mule::gui::Preferences expected{
        .showLogPanel = true,
        .autoScroll = false,
        .logPanelCollapsed = true,
        .logPanelHeight = expectedLogPanelHeight,
        .activeDataProfile = "Classic UO Felucca",
        .showWelcomeOnStartup = false,
        .idFormat = uo_mule::gui::IdFormat::decimal,
        .texmapsHideUnused = true,
        .texmapsTablePresentation = true,
        .fullscreen = true,
    };

    uo_mule::gui::savePreferences(expected, preferencesFile);
    const auto actual = uo_mule::gui::loadPreferences(preferencesFile);

    REQUIRE(
        std::filesystem::is_regular_file(configDirectory / std::string(uo_mule::core::applicationName()) / "preferences.conf"));
    REQUIRE_FALSE(actual.showLogPanel);
    REQUIRE(actual.autoScroll);
    REQUIRE_FALSE(actual.logPanelCollapsed);
    REQUIRE(std::abs(actual.logPanelHeight) < heightTolerance);
    REQUIRE(actual.showWelcomeOnStartup);
    REQUIRE(actual.idFormat == uo_mule::gui::IdFormat::hexadecimal);
    REQUIRE(actual.texmapsHideUnused);
    REQUIRE_FALSE(actual.texmapsTablePresentation);
    REQUIRE_FALSE(actual.fullscreen);
    REQUIRE(actual.activeDataProfile == "Classic UO Felucca");
    std::filesystem::remove_all(configDirectory);
}

TEST_CASE("savePreferences_Should_ReplaceSymlinkWithoutWritingToItsTarget")
{
    const auto configDirectory = std::filesystem::temp_directory_path() / "pref-symlink";
    const auto externalFile = std::filesystem::temp_directory_path() / "pref-symlink-target";
    std::filesystem::remove_all(configDirectory);
    std::filesystem::remove(externalFile);
    const auto preferencesDirectory = configDirectory / std::string(uo_mule::core::applicationName());
    const auto preferencesFile = preferencesDirectory / "preferences.conf";
    std::filesystem::create_directories(preferencesDirectory);
    {
        std::ofstream target(externalFile);
        target << "preserve this file\n";
    }
    std::filesystem::create_symlink(externalFile, preferencesFile);

    uo_mule::gui::savePreferences(uo_mule::gui::Preferences{.showLogPanel = true, .activeDataProfile = {}}, preferencesFile);

    std::ifstream target(externalFile);
    const std::string targetContents((std::istreambuf_iterator<char>(target)), std::istreambuf_iterator<char>());
    REQUIRE(targetContents == "preserve this file\n");
    REQUIRE_FALSE(std::filesystem::is_symlink(preferencesFile));
    std::filesystem::remove_all(configDirectory);
    std::filesystem::remove(externalFile);
}

TEST_CASE("savePreferences_Should_ReplaceExistingPreferences")
{
    const auto preferencesDirectory = std::filesystem::temp_directory_path() / "pref-existing";
    const auto preferencesFile = preferencesDirectory / "preferences.conf";
    std::filesystem::remove_all(preferencesDirectory);

    uo_mule::gui::savePreferences(uo_mule::gui::Preferences{.showLogPanel = true, .activeDataProfile = {}}, preferencesFile);
    const auto loaded = uo_mule::gui::loadPreferences(preferencesFile);

    REQUIRE(std::filesystem::is_regular_file(preferencesFile));
    REQUIRE_FALSE(loaded.showLogPanel);
    std::filesystem::remove_all(preferencesDirectory);
}

TEST_CASE("preferences_Should_PersistLastActiveProfile")
{
    const auto preferencesFile = std::filesystem::temp_directory_path() / "pref-active-profile" / "preferences.conf";
    std::filesystem::remove_all(preferencesFile.parent_path());

    uo_mule::gui::Preferences preferences;
    preferences.activeDataProfile = "Classic UO Felucca";
    uo_mule::gui::savePreferences(preferences, preferencesFile);

    REQUIRE(uo_mule::gui::loadPreferences(preferencesFile).activeDataProfile == "Classic UO Felucca");
    std::filesystem::remove_all(preferencesFile.parent_path());
}

TEST_CASE("savePreferences_Should_NotPersistProfileBoundToolOptionsGlobally")
{
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    const auto preferencesFile = std::filesystem::temp_directory_path() / "pref-tools" / "preferences.conf";
    std::filesystem::remove_all(preferencesFile.parent_path());
    uo_mule::gui::Preferences expected;
    expected.tools.activeTool = uo_mule::gui::ToolId::replace;
    expected.tools.selectionGranularity = uo_mule::gui::ToolSelectionGranularity::blocks;
    expected.tools.activeTileSet = true;
    expected.tools.activeTile = {.id = 1234, .z = -12};
    expected.tools.fillIdEnabled = true;
    expected.tools.fillZEnabled = false;
    expected.tools.replaceIdEnabled = false;
    expected.tools.replaceZEnabled = true;
    expected.tools.replaceSource = {.id = 22, .z = 3};
    expected.tools.replaceTarget = {.id = 44, .z = 5};
    expected.tools.fuzzy.scope = uo_mule::gui::FuzzyScope::wholeMap;
    expected.tools.fuzzy.idEnabled = true;
    expected.tools.fuzzy.id = 77;
    expected.tools.fuzzy.zEnabled = true;
    expected.tools.fuzzy.z = -4;
    expected.tools.fuzzy.nameEnabled = true;
    expected.tools.fuzzy.name = "granite floor";

    uo_mule::gui::savePreferences(expected, preferencesFile);
    const auto actual = uo_mule::gui::loadPreferences(preferencesFile);

    REQUIRE(actual.tools.activeTool == uo_mule::gui::ToolId::move);
    REQUIRE(actual.tools.selectionGranularity == uo_mule::gui::ToolSelectionGranularity::tiles);
    REQUIRE_FALSE(actual.tools.activeTileSet);
    REQUIRE(actual.tools.activeTile == uomul::MapTile{});
    REQUIRE(actual.tools.fuzzy.scope == uo_mule::gui::FuzzyScope::contiguous);
    REQUIRE(actual.tools.fuzzy.name.empty());
    std::filesystem::remove_all(preferencesFile.parent_path());
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
}
