#include "gui/commands/command_registry.h"
#include "gui/shell/welcome_greetings.h"
#include "gui/state/workbench_state.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/input/coordinate_parser.h"
#include "gui_model/persistence/recent_files.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/session/workbench_session.h"
#include "gui_model/workspaces/map_workspace.h"
#include "uomul/map.h"
#include "uomul/radarcol/radarcol.h"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <optional>
#include <random>
#include <string>

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

TEST_CASE("profileReadiness_Should_ReportRadarcolConfigurationWithoutBlockingFallback")
{
    const TemporaryDirectory directory;
    createValidRadarcol(directory.path() / "radarcol.mul");

    const uo_mule::gui::DataProfile empty;
    REQUIRE(uo_mule::gui::profileReadiness(empty) == uo_mule::gui::ProfileReadinessState::needsProfile);
    REQUIRE(uo_mule::gui::mapReady(empty));

    const auto incomplete = validProfile(directory.path());
    std::filesystem::remove(directory.path() / "radarcol.mul");
    REQUIRE(uo_mule::gui::profileReadiness(incomplete) == uo_mule::gui::ProfileReadinessState::profileIncomplete);
    REQUIRE(uo_mule::gui::mapReady(incomplete));

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

TEST_CASE("workbenchSession_Should_AllowMapCommandsWithEmbeddedRadarcolFallback")
{
    uo_mule::gui::WorkbenchState state;
    state.startupLoaded = true;

    const auto availability = state.commands.availability(uo_mule::gui::CommandId::fileOpen, state);

    REQUIRE(availability.enabled);
}

TEST_CASE("workbenchSession_Should_ShowWelcomeAfterInvalidProfileWithoutBlockingMaps")
{
    uo_mule::gui::WorkbenchSession session;

    session.markProfileInvalid();

    REQUIRE(session.profileState() == uo_mule::gui::ProfileReadinessState::profileInvalid);
    REQUIRE(session.mapReady());
    REQUIRE(session.startupState() == uo_mule::gui::StartupState::mapWelcome);
}

TEST_CASE("recentFileStore_Should_RoundTripAndMarkMissingEntries")
{
    const TemporaryDirectory directory;
    const auto recentPath = directory.path() / "recent.conf";
    const auto mapPath = directory.path() / "missing.map0.mul";

    uo_mule::gui::RecentFileStore store(recentPath);
    store.record(mapPath, uomul::Dimensions{.widthTiles = testMapDimension, .heightTiles = testMapDimension}, "Felucca");

    const uo_mule::gui::RecentFileStore loaded(recentPath);
    REQUIRE(loaded.files().size() == 1);
    REQUIRE(loaded.files().front().path == mapPath);
    REQUIRE(loaded.files().front().displayName == "missing.map0");
    const auto & dimensions = loaded.files().front().dimensions;
    REQUIRE(dimensions.has_value());
    if (dimensions.has_value())
    {
        REQUIRE(dimensions->widthTiles == testMapDimension);
    }
    REQUIRE(loaded.files().front().facet == "Felucca");
    REQUIRE_FALSE(loaded.files().front().failure.empty());
}

TEST_CASE("recentFileStore_Should_PersistLastOpenedTimestamp")
{
    const TemporaryDirectory directory;
    const auto recentPath = directory.path() / "recent.conf";
    const auto mapPath = directory.path() / "map0.mul";
    std::ofstream mapFile(mapPath);
    mapFile << "fixture";
    mapFile.close();

    uo_mule::gui::RecentFileStore store(recentPath);
    store.record(mapPath);

    const uo_mule::gui::RecentFileStore loaded(recentPath);
    REQUIRE(loaded.files().size() == 1);
    REQUIRE(loaded.files().front().lastOpened > 0);
    REQUIRE(loaded.files().front().lastModified > 0);
}

TEST_CASE("workbenchSession_Should_RecordKnownFacetForRecentFileDimensions")
{
    const TemporaryDirectory directory;
    const auto recentPath = directory.path() / "recent.conf";
    const auto dimensions = uomul::facetDimensions("felucca");
    REQUIRE(dimensions.has_value());

    uo_mule::gui::WorkbenchSession session(recentPath);
    session.recordRecentFile(directory.path() / "map0.mul", *dimensions);

    REQUIRE(session.recentFiles().size() == 1);
    REQUIRE(session.recentFiles().front().facet == "legacy");
}

TEST_CASE("recentFileStore_Should_KeepFiveMostRecentFilesInOrder")
{
    const TemporaryDirectory directory;
    const auto recentPath = directory.path() / "recent.conf";
    uo_mule::gui::RecentFileStore store(recentPath);

    for (std::size_t index = 0; index < uo_mule::gui::recentFileLimit + 1U; index++)
    {
        store.record(directory.path() / ("map" + std::to_string(index) + ".mul"));
    }

    REQUIRE(store.files().size() == uo_mule::gui::recentFileLimit);
    REQUIRE(store.files().front().path.filename() == "map5.mul");
    REQUIRE(store.files().back().path.filename() == "map1.mul");

    store.record(directory.path() / "map2.mul");
    REQUIRE(store.files().front().path.filename() == "map2.mul");
    REQUIRE(store.files().size() == uo_mule::gui::recentFileLimit);
}

TEST_CASE("recentFileStore_Should_RoundTripAllRecentDocumentKinds")
{
    const TemporaryDirectory directory;
    const auto recentPath = directory.path() / "recent.conf";
    const auto radarcolPath = directory.path() / "radarcol.mul";
    const auto texidxPath = directory.path() / "texidx.mul";
    const auto texmapsPath = directory.path() / "texmaps.mul";

    uo_mule::gui::RecentFileStore store(recentPath);
    store.record(radarcolPath, std::nullopt, {}, uo_mule::gui::DocumentKind::radarcol);
    store.record(texidxPath, std::nullopt, {}, uo_mule::gui::DocumentKind::texmapPair, texmapsPath);

    const uo_mule::gui::RecentFileStore loaded(recentPath);
    REQUIRE(loaded.files().size() == 2);
    REQUIRE(loaded.files().at(0).kind == uo_mule::gui::DocumentKind::texmapPair);
    REQUIRE(loaded.files().at(0).companionPath == texmapsPath);
    REQUIRE(loaded.files().at(1).kind == uo_mule::gui::DocumentKind::radarcol);
    REQUIRE(loaded.files().at(1).companionPath.empty());
}

TEST_CASE("recentFiles_Should_RetainUnknownKindsAsUnavailable")
{
    const TemporaryDirectory directory;
    const auto recentPath = directory.path() / "recent.conf";
    const auto sourcePath = directory.path() / "future.mul";
    std::ofstream(sourcePath) << "future";
    std::ofstream file(recentPath);
    file << "schema_version 0\n"
         << "map " << std::quoted(sourcePath.string()) << " " << std::quoted("future") << " 0 " << std::quoted("") << " "
         << std::quoted("") << " " << std::quoted("future_kind") << " 0 " << std::quoted("") << " 1 1\n";
    file.close();

    const auto loaded = uo_mule::gui::loadRecentFiles(recentPath);

    REQUIRE(loaded.size() == 1);
    REQUIRE(loaded.front().kind == uo_mule::gui::DocumentKind::unavailable);
    REQUIRE(loaded.front().failure.find("future_kind") != std::string::npos);
}

TEST_CASE("welcomeGreeting_Should_SelectAConfiguredGreeting")
{
    std::mt19937 generator(std::random_device{}());
    const std::string greeting = uo_mule::gui::chooseWelcomeGreeting(generator);

    REQUIRE_FALSE(greeting.empty());
    REQUIRE(std::find(uo_mule::gui::welcomeGreetings().begin(), uo_mule::gui::welcomeGreetings().end(), greeting) !=
            uo_mule::gui::welcomeGreetings().end());
}

TEST_CASE("parseCoordinate_Should_ParseTilesAndBlocks")
{
    const auto tile = uo_mule::gui::parseCoordinate("12 34", uo_mule::gui::CoordinateKind::tile);
    REQUIRE(tile.has_value());
    REQUIRE(tile->kind == uo_mule::gui::CoordinateKind::tile);
    REQUIRE(tile->value.x == 12);
    REQUIRE(tile->value.y == 34);

    const auto block = uo_mule::gui::parseCoordinate("2 3", uo_mule::gui::CoordinateKind::block);
    REQUIRE(block.has_value());
    REQUIRE(block->kind == uo_mule::gui::CoordinateKind::block);
    REQUIRE(block->value.x == 2);
    REQUIRE(block->value.y == 3);

    REQUIRE_FALSE(uo_mule::gui::parseCoordinate("invalid", uo_mule::gui::CoordinateKind::tile).has_value());
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("documentManager_Should_AssignStableIdsAndExposeContext")
{
    const TemporaryDirectory directory;
    const auto mapPath = directory.path() / "map0.mul";
    const uomul::Dimensions dimensions{.widthTiles = 16, .heightTiles = 16};
    REQUIRE(uomul::MapEditor::createEmpty(mapPath, dimensions));

    uo_mule::gui::MapWorkspace workspace;
    REQUIRE(workspace.openMap(mapPath, dimensions));
    uo_mule::gui::WorkbenchSession session(directory.path() / "recent.conf");
    session.syncDocument(workspace);

    REQUIRE(session.activeDocument().has_value());
    if (session.activeDocument())
    {
        REQUIRE(session.activeDocument()->id != 0);
        REQUIRE(session.activeDocument()->sourcePath == mapPath);
    }
    REQUIRE(session.contextSnapshot(workspace).hasDocument);

    REQUIRE(workspace.closeMap());
    session.syncDocument(workspace);
    REQUIRE_FALSE(session.activeDocument().has_value());
}

TEST_CASE("workbenchSession_Should_ActivateMapDocumentsThroughTheCollection")
{
    const TemporaryDirectory directory;
    const auto firstPath = directory.path() / "first.mul";
    const auto secondPath = directory.path() / "second.mul";
    const uomul::Dimensions dimensions{.widthTiles = 16, .heightTiles = 16};
    REQUIRE(uomul::MapEditor::createEmpty(firstPath, dimensions));
    REQUIRE(uomul::MapEditor::createEmpty(secondPath, dimensions));

    uo_mule::gui::MapWorkspace workspace;
    uo_mule::gui::WorkbenchSession session(directory.path() / "recent.conf");
    REQUIRE(workspace.openMap(firstPath, dimensions));
    session.syncDocument(workspace);
    const auto firstId = session.activeDocumentId();
    REQUIRE(firstId.has_value());
    REQUIRE(workspace.openMap(secondPath, dimensions));
    session.syncDocument(workspace);
    const auto secondId = session.activeDocumentId();
    REQUIRE(secondId.has_value());
    REQUIRE(firstId != secondId);

    const auto firstDocumentId = firstId.value_or(0);
    REQUIRE(firstId.has_value());
    REQUIRE(session.activateDocument(firstDocumentId, workspace));
    REQUIRE(session.activeDocumentId() == firstId);
    REQUIRE(workspace.originalPath() == firstPath);
    REQUIRE(session.documents().size() == 2);
}

TEST_CASE("workbenchSession_Should_KeepExistingMapTabsWhenOpeningMapsSequentially")
{
    const TemporaryDirectory directory;
    const auto firstPath = directory.path() / "first.mul";
    const auto secondPath = directory.path() / "second.mul";
    const uomul::Dimensions dimensions{.widthTiles = 16, .heightTiles = 16};
    REQUIRE(uomul::MapEditor::createEmpty(firstPath, dimensions));
    REQUIRE(uomul::MapEditor::createEmpty(secondPath, dimensions));

    uo_mule::gui::MapWorkspace workspace;
    uo_mule::gui::WorkbenchSession session(directory.path() / "recent.conf");
    REQUIRE(workspace.openMap(firstPath, dimensions));
    session.syncDocument(workspace);
    const auto firstId = session.activeDocumentId();
    REQUIRE(firstId.has_value());

    REQUIRE(workspace.openMap(secondPath, dimensions));
    session.syncDocument(workspace);
    const auto secondId = session.activeDocumentId();
    REQUIRE(secondId.has_value());
    REQUIRE(secondId != firstId);
    REQUIRE(session.documents().size() == 2);

    const auto firstDocumentId = firstId.value_or(0);
    REQUIRE(firstId.has_value());
    REQUIRE(session.activateDocument(firstDocumentId, workspace));
    REQUIRE(session.activeDocumentId() == firstId);
    REQUIRE(workspace.originalPath() == firstPath);
}
