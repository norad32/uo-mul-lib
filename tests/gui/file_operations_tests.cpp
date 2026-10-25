#include "gui/dialogs/map_workflow_dialog.h"
#include "gui/dialogs/preferences_dialog.h"
#include "gui/workbench/notification_state.h"
#include "gui/workbench/workbench_state.h"
#include "gui/workflows/file_workflow.h"
#include "gui_model/dialogs/dialog_contracts.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/providers/data_providers.h"
#include "uomul/radarcol/radarcol.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <optional>
#include <system_error>

namespace
{

class TemporaryDirectory
{
public:
    TemporaryDirectory() : path_(std::filesystem::temp_directory_path() / "uo-mule-file-workflow-tests")
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        std::filesystem::create_directories(path_, error);
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory &) = delete;
    TemporaryDirectory & operator=(const TemporaryDirectory &) = delete;
    TemporaryDirectory(TemporaryDirectory &&) = delete;
    TemporaryDirectory & operator=(TemporaryDirectory &&) = delete;

    [[nodiscard]] const std::filesystem::path & path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("fileBrowserResult_Should_ApplyMetadataSearchFolder")
{
    uo_mule::gui::PreferencesDialogState state;
    const auto root = std::filesystem::temp_directory_path() / "metadata-root";
    const uo_mule::gui::FileBrowserResult result{
        .target = uo_mule::gui::FileBrowserTarget::dataSearchRoot,
        .path = root,
        .mapCandidates = {},
        .assetCandidates = {},
        .assetCandidate = std::nullopt,
    };

    uo_mule::gui::applyFileBrowserResult(state, result);
    REQUIRE(state.workingProfile.sourceRoot == root);
}

TEST_CASE("fileBrowserResult_Should_TransitionMapOpenToFacetSelection")
{
    uo_mule::gui::NewFileDialogState state;
    const auto mapPath = std::filesystem::temp_directory_path() / "map0.mul";
    const uo_mule::gui::FileBrowserResult result{
        .target = uo_mule::gui::FileBrowserTarget::mapOpen,
        .path = mapPath,
        .mapCandidates = {},
        .assetCandidates = {},
        .assetCandidate = std::nullopt,
    };

    uo_mule::gui::applyFileBrowserResult(state, result);
    REQUIRE(state.mode == uo_mule::gui::MapFileDialogMode::selectMapFacet);
    REQUIRE(std::filesystem::path(state.openPath.data()) == mapPath);
}

TEST_CASE("defaultRadarcolWorkflow_Should_CreateAndBindProfile")
{
    const TemporaryDirectory directory;
    uo_mule::gui::WorkbenchState state;
    state.activeProfile.name = "workflow-test";
    state.activeProfile.sourceRoot = directory.path();
    const auto path = directory.path() / "radarcol.mul";

    REQUIRE(uo_mule::gui::createDefaultRadarcol(state, path));
    REQUIRE(std::filesystem::exists(path));
    REQUIRE(state.activeProfile.files.at(uo_mule::gui::DataFileRole::radarcol).path == "radarcol.mul");
    REQUIRE(state.activeProfile.files.at(uo_mule::gui::DataFileRole::radarcol).userOverride);
    REQUIRE(state.menuBarState.preferencesDialog.profileSaveRequested);
    REQUIRE_FALSE(state.notifications.entries.empty());
    REQUIRE(state.notifications.entries.back().severity == uo_mule::gui::NotificationSeverity::success);
    REQUIRE(state.providers.radarColors().status().source == uo_mule::gui::ProviderSourceKind::externalConfigured);
    REQUIRE(uomul::RadarColReader::open(path));
}

TEST_CASE("defaultRadarcolWorkflow_Should_PreserveProfileIfCreationCollides")
{
    const TemporaryDirectory directory;
    const auto path = directory.path() / "radarcol.mul";
    auto editor = uomul::RadarColEditor::createEmpty(path);
    REQUIRE(editor);
    REQUIRE(editor->commit());

    uo_mule::gui::WorkbenchState state;
    state.activeProfile.sourceRoot = directory.path();
    REQUIRE_FALSE(uo_mule::gui::createDefaultRadarcol(state, path));
    REQUIRE(state.activeProfile.files.find(uo_mule::gui::DataFileRole::radarcol) == state.activeProfile.files.end());
    REQUIRE_FALSE(state.menuBarState.preferencesDialog.profileSaveRequested);
    REQUIRE_FALSE(state.notifications.entries.empty());
    REQUIRE(state.notifications.entries.back().severity == uo_mule::gui::NotificationSeverity::error);
}

TEST_CASE("defaultRadarcolWorkflow_Should_CreateNothingIfFileBrowserIsCancelled")
{
    const TemporaryDirectory directory;
    uo_mule::gui::WorkbenchState state;
    const auto path = directory.path() / "radarcol.mul";
    state.fileBrowser.visible = true;
    state.fileBrowser.request.target = uo_mule::gui::FileBrowserTarget::defaultRadarcolOutput;
    state.fileBrowser.request.initialPath = path;

    REQUIRE(uo_mule::gui::handleFileWorkflowEscape(state));
    REQUIRE_FALSE(std::filesystem::exists(path));
    REQUIRE(state.activeProfile.files.find(uo_mule::gui::DataFileRole::radarcol) == state.activeProfile.files.end());
}
