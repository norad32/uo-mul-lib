#include "core/app_info.h"
#include "gui/preferences.h"
#include "gui/preferences_store.h"
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

    const auto preferences = im_cpp::gui::loadPreferences(preferencesFile);

    REQUIRE_FALSE(preferences.showLogPanel);
    REQUIRE(preferences.autoScroll);
    REQUIRE_FALSE(preferences.logPanelCollapsed);
    REQUIRE(std::abs(preferences.logPanelHeight) < heightTolerance);
    std::filesystem::remove_all(preferencesFile.parent_path());
}

TEST_CASE("savePreferences_Should_RoundTripSettings")
{
    const auto configDirectory = std::filesystem::temp_directory_path() / "pref-roundtrip";
    const auto preferencesFile = configDirectory / std::string(im_cpp::core::applicationName()) / "preferences.conf";
    std::filesystem::remove_all(configDirectory);
    const im_cpp::gui::Preferences expected{
        .showLogPanel = true,
        .autoScroll = false,
        .logPanelCollapsed = true,
        .logPanelHeight = expectedLogPanelHeight,
    };

    im_cpp::gui::savePreferences(expected, preferencesFile);
    const auto actual = im_cpp::gui::loadPreferences(preferencesFile);

    REQUIRE(std::filesystem::is_regular_file(configDirectory / std::string(im_cpp::core::applicationName()) / "preferences.conf"));
    REQUIRE(actual.showLogPanel);
    REQUIRE_FALSE(actual.autoScroll);
    REQUIRE(actual.logPanelCollapsed);
    REQUIRE(std::abs(actual.logPanelHeight - expectedLogPanelHeight) < heightTolerance);
    std::filesystem::remove_all(configDirectory);
}

TEST_CASE("savePreferences_Should_ReplaceSymlinkWithoutWritingToItsTarget")
{
    const auto configDirectory = std::filesystem::temp_directory_path() / "pref-symlink";
    const auto externalFile = std::filesystem::temp_directory_path() / "pref-symlink-target";
    std::filesystem::remove_all(configDirectory);
    std::filesystem::remove(externalFile);
    const auto preferencesDirectory = configDirectory / std::string(im_cpp::core::applicationName());
    const auto preferencesFile = preferencesDirectory / "preferences.conf";
    std::filesystem::create_directories(preferencesDirectory);
    {
        std::ofstream target(externalFile);
        target << "preserve this file\n";
    }
    std::filesystem::create_symlink(externalFile, preferencesFile);

    im_cpp::gui::savePreferences(im_cpp::gui::Preferences{.showLogPanel = true}, preferencesFile);

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

    im_cpp::gui::savePreferences(im_cpp::gui::Preferences{.showLogPanel = true}, preferencesFile);
    const auto loaded = im_cpp::gui::loadPreferences(preferencesFile);

    REQUIRE(std::filesystem::is_regular_file(preferencesFile));
    REQUIRE(loaded.showLogPanel);
    std::filesystem::remove_all(preferencesDirectory);
}
