#include "core/app_info.h"
#include "gui/persistence/preferences.h"
#include "gui/persistence/preferences_store.h"
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

TEST_CASE("loadPreferences_Should_UseDefaultsWhenFileIsMissing")
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

TEST_CASE("savePreferences_Should_RoundTripSettings")
{
    const auto configDirectory = std::filesystem::temp_directory_path() / "pref-roundtrip";
    const auto preferencesFile = configDirectory / std::string(uo_mule::core::applicationName()) / "preferences.conf";
    std::filesystem::remove_all(configDirectory);
    const uo_mule::gui::Preferences expected{
        .showLogPanel = true,
        .autoScroll = false,
        .logPanelCollapsed = true,
        .logPanelHeight = expectedLogPanelHeight,
        .activeDataProfile = {},
        .showWelcomeOnStartup = false,
        .idFormat = uo_mule::gui::IdFormat::decimal,
        .texmapsHideUnused = true,
        .texmapsTablePresentation = true,
    };

    uo_mule::gui::savePreferences(expected, preferencesFile);
    const auto actual = uo_mule::gui::loadPreferences(preferencesFile);

    REQUIRE(
        std::filesystem::is_regular_file(configDirectory / std::string(uo_mule::core::applicationName()) / "preferences.conf"));
    REQUIRE(actual.showLogPanel);
    REQUIRE_FALSE(actual.autoScroll);
    REQUIRE(actual.logPanelCollapsed);
    REQUIRE(std::abs(actual.logPanelHeight - expectedLogPanelHeight) < heightTolerance);
    REQUIRE_FALSE(actual.showWelcomeOnStartup);
    REQUIRE(actual.idFormat == uo_mule::gui::IdFormat::decimal);
    REQUIRE(actual.texmapsHideUnused);
    REQUIRE(actual.texmapsTablePresentation);
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
    REQUIRE(loaded.showLogPanel);
    std::filesystem::remove_all(preferencesDirectory);
}
