#include "gui/data_profile.h"
#include "gui/command_registry.h"
#include "gui/recent_maps.h"
#include "gui/workbench_session.h"
#include "gui/workbench_state.h"
#include "uomul/map.h"
#include "uomul/radarcol/radarcol.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>

namespace
{

class TemporaryDirectory
{
public:
    TemporaryDirectory() : path_(std::filesystem::temp_directory_path() / "uo-mule-workbench-session-tests")
    {
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() { std::filesystem::remove_all(path_); }

    TemporaryDirectory(const TemporaryDirectory &) = delete;
    TemporaryDirectory & operator=(const TemporaryDirectory &) = delete;
    TemporaryDirectory(TemporaryDirectory &&) = delete;
    TemporaryDirectory & operator=(TemporaryDirectory &&) = delete;

    [[nodiscard]] const std::filesystem::path & path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

void createValidRadarcol(const std::filesystem::path & path)
{
    std::ofstream file(path, std::ios::binary);
    file.seekp(static_cast<std::streamoff>((uomul::radarColorCount * sizeof(uomul::RadarColor)) - 1));
    file.put('\0');
}

uo_mule::gui::DataProfile validProfile(const std::filesystem::path & root)
{
    uo_mule::gui::DataProfile profile;
    profile.name = "Classic";
    profile.sourceRoot = root;
    profile.files[uo_mule::gui::DataFileRole::radarcol] = {.path = "radarcol.mul"};
    return profile;
}

} // namespace

constexpr std::uint32_t testMapDimension = 64;

TEST_CASE("profileReadiness_Should_RequireValidatedRadarcol")
{
    const TemporaryDirectory directory;
    createValidRadarcol(directory.path() / "radarcol.mul");

    const uo_mule::gui::DataProfile empty;
    REQUIRE(uo_mule::gui::profileReadiness(empty) == uo_mule::gui::ProfileReadinessState::needsProfile);

    const auto incomplete = validProfile(directory.path());
    std::filesystem::remove(directory.path() / "radarcol.mul");
    REQUIRE(uo_mule::gui::profileReadiness(incomplete) == uo_mule::gui::ProfileReadinessState::profileIncomplete);

    createValidRadarcol(directory.path() / "radarcol.mul");
    REQUIRE(uo_mule::gui::profileReadiness(incomplete) == uo_mule::gui::ProfileReadinessState::ready);
}

TEST_CASE("workbenchSession_Should_TransitionFromProfileSetupToWelcome")
{
    const TemporaryDirectory directory;
    createValidRadarcol(directory.path() / "radarcol.mul");
    uo_mule::gui::WorkbenchSession session(directory.path() / "recent.conf");

    session.updateProfile(validProfile(directory.path()));

    REQUIRE(session.mapReady());
    REQUIRE(session.startupState() == uo_mule::gui::StartupState::mapWelcome);
}

TEST_CASE("workbenchSession_Should_BlockMapCommandsAfterStartupWhenProfileIsNotReady")
{
    uo_mule::gui::WorkbenchState state;
    state.startupLoaded = true;

    const auto availability = state.commands.availability(uo_mule::gui::CommandId::fileOpen, state);

    REQUIRE_FALSE(availability.enabled);
    REQUIRE(availability.disabledReason == "Configure a valid radarcol.mul profile first");
}

TEST_CASE("recentMapStore_Should_RoundTripAndMarkMissingEntries")
{
    const TemporaryDirectory directory;
    const auto recentPath = directory.path() / "recent.conf";
    const auto mapPath = directory.path() / "missing.map0.mul";

    uo_mule::gui::RecentMapStore store(recentPath);
    store.record(mapPath,
                 uomul::Dimensions{.widthTiles = testMapDimension, .heightTiles = testMapDimension},
                 "Felucca");

    const uo_mule::gui::RecentMapStore loaded(recentPath);
    REQUIRE(loaded.maps().size() == 1);
    REQUIRE(loaded.maps().front().path == mapPath);
    REQUIRE(loaded.maps().front().displayName == "missing.map0");
    const auto & dimensions = loaded.maps().front().dimensions;
    REQUIRE(dimensions.has_value());
    if (dimensions.has_value())
    {
        REQUIRE(dimensions->widthTiles == testMapDimension);
    }
    REQUIRE(loaded.maps().front().facet == "Felucca");
    REQUIRE_FALSE(loaded.maps().front().failure.empty());
}

TEST_CASE("workbenchSession_Should_RecordKnownFacetForRecentMapDimensions")
{
    const TemporaryDirectory directory;
    const auto recentPath = directory.path() / "recent.conf";
    const auto dimensions = uomul::facetDimensions("felucca");
    REQUIRE(dimensions.has_value());

    uo_mule::gui::WorkbenchSession session(recentPath);
    session.recordRecentMap(directory.path() / "map0.mul", *dimensions);

    REQUIRE(session.recentMaps().size() == 1);
    REQUIRE(session.recentMaps().front().facet == "felucca");
}
