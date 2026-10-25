#include "gui/presentation/strings.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/providers/data_providers.h"
#include "gui_model/session/workbench_session.h"
#include <catch2/catch_test_macros.hpp>
#include <string_view>

TEST_CASE("uiStrings_Should_CentralizeSharedDisplayVocabulary")
{
    REQUIRE(std::string_view(uo_mule::gui::strings::open()) == "Open");
    REQUIRE(std::string_view(uo_mule::gui::strings::cancel()) == "Cancel");
    REQUIRE(std::string_view(uo_mule::gui::strings::hideEmpty()) == "Hide empty");
    REQUIRE(std::string_view(uo_mule::gui::strings::preferences()) == "Preferences");
    REQUIRE(std::string_view(uo_mule::gui::strings::createDefaultRadarcol()) == "Create Default Radarcol");
    REQUIRE(std::string_view(uo_mule::gui::strings::defaultRadarcolCreated()) == "Default Radarcol created and configured");
    REQUIRE(std::string_view(uo_mule::gui::strings::closeWithoutSaving()) == "Close without saving");
    REQUIRE(std::string_view(uo_mule::gui::strings::noMapOpen()) == "No map open");
    REQUIRE(std::string_view(uo_mule::gui::strings::clear()) == "Clear");
    REQUIRE(std::string_view(uo_mule::gui::strings::sourceTexmapUnavailable()) == "The source Texmap texture is unavailable.");
    REQUIRE(std::string_view(uo_mule::gui::strings::exportCancelled()) == "Export cancelled.");
    REQUIRE(std::string_view(uo_mule::gui::strings::metadataSourceNotConfigured()) == "The metadata source is not configured");
    REQUIRE(uo_mule::gui::strings::dataFileLabel(uo_mule::gui::DataFileRole::radarcol) == "Radar colors");
    REQUIRE(uo_mule::gui::strings::dataFileName(uo_mule::gui::DataFileRole::radarcol) == "radarcol.mul");
}

TEST_CASE("uiStrings_Should_FormatSharedStatusValues")
{
    REQUIRE(uo_mule::gui::strings::formatProfileStatus("") == "Profile: None");
    REQUIRE(uo_mule::gui::strings::formatMapStatus({}) == "Map: None");
    REQUIRE(uo_mule::gui::strings::formatStateStatus(true) == "State: Staged changes");
    REQUIRE(uo_mule::gui::strings::formatDataStatus(uo_mule::gui::ProfileReadinessState::ready) == "Data: Configured");
    REQUIRE(uo_mule::gui::strings::formatRadarcolStatus(uo_mule::gui::ProviderSourceKind::embeddedDefault) ==
            "Radarcol: Embedded Radarcol");
    REQUIRE(uo_mule::gui::strings::formatSaveChangesPrompt("map.mul") == "Save changes to \"map.mul\" before closing?");
    REQUIRE(uo_mule::gui::strings::formatRadarcolFallbackWarning("read failed") ==
            "Configured Radarcol is unavailable; using Embedded Radarcol fallback. read failed");
    REQUIRE(uo_mule::gui::strings::formatEditingRadarcols(2U) == "Editing 2 Radarcols. Press Esc to cancel.");
    REQUIRE(uo_mule::gui::strings::formatSelectedColors(3U) == "Selected colors: 3");
}

TEST_CASE("uiStrings_Should_FormatIdsAccordingToPreference")
{
    REQUIRE(uo_mule::gui::strings::formatId(0x3AU, uo_mule::gui::IdFormat::hexadecimal) == "0x003A");
    REQUIRE(uo_mule::gui::strings::formatId(0x3AU, uo_mule::gui::IdFormat::decimal) == "58");
}
