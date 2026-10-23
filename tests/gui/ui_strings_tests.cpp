#include "gui/documents/document_contracts.h"
#include "gui/persistence/preferences.h"
#include "gui/ui/strings/ui_strings.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("uiStrings_Should_CentralizeSharedDisplayVocabulary")
{
    REQUIRE(std::string_view(uo_mule::gui::strings::open()) == "Open");
    REQUIRE(std::string_view(uo_mule::gui::strings::cancel()) == "Cancel");
    REQUIRE(std::string_view(uo_mule::gui::strings::preferences()) == "Preferences");
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
}

TEST_CASE("uiStrings_Should_FormatIdsAccordingToPreference")
{
    REQUIRE(uo_mule::gui::strings::formatId(0x3AU, uo_mule::gui::IdFormat::hexadecimal) == "0x003A");
    REQUIRE(uo_mule::gui::strings::formatId(0x3AU, uo_mule::gui::IdFormat::decimal) == "58");
}
