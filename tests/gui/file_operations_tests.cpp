#include "gui/dialogs/file_browser_dialog.h"
#include "gui/dialogs/file_selector_dialog.h"
#include "gui/dialogs/map_workflow_dialog.h"
#include "gui/dialogs/preferences_dialog.h"
#include "gui/presentation/bounded_buffer.h"
#include "gui/workbench/notification_state.h"
#include "gui/workbench/workbench_state.h"
#include "gui/workflows/file_workflow.h"
#include "gui_model/dialogs/dialog_contracts.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/providers/data_providers.h"
#include "uomul/radarcol/radarcol.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <optional>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>

using uo_mule::gui::completeDialogResult;
using uo_mule::gui::DataFileRole;
using uo_mule::gui::DialogAction;
using uo_mule::gui::DialogId;
using uo_mule::gui::DialogRequest;
using uo_mule::gui::DocumentKind;
using uo_mule::gui::FileBrowserAssetCandidate;
using uo_mule::gui::FileBrowserMode;
using uo_mule::gui::FileBrowserResult;
using uo_mule::gui::FileBrowserTarget;
using uo_mule::gui::MulAssetKind;
using uo_mule::gui::requestFileBrowser;
using uo_mule::gui::requestPreferencesDialog;
using uo_mule::gui::WorkbenchState;

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

void applyDeferredActions(uo_mule::gui::WorkbenchState & state)
{
    const auto actions = std::move(state.deferredWorkflowActions);
    for (const auto & action : actions)
    {
        action();
    }
}

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

TEST_CASE("dialogRequests_Should_ActivateAndReplaceTheAuthoritativeHost")
{
    uo_mule::gui::WorkbenchState state;
    uo_mule::gui::requestFileBrowser(state.fileBrowser,
                                     {
                                         .mode = uo_mule::gui::FileBrowserMode::openFile,
                                         .target = uo_mule::gui::FileBrowserTarget::genericOpen,
                                         .initialPath = std::filesystem::current_path(),
                                         .extension = ".mul",
                                         .title = "Open MUL File",
                                     });

    REQUIRE(state.dialogHost.owns(uo_mule::gui::DialogId::fileBrowser));
    if (!state.dialogHost.active)
    {
        FAIL("The file-browser request must activate the dialog host");
    }
    REQUIRE(state.fileBrowser.request.target == uo_mule::gui::FileBrowserTarget::genericOpen);

    uo_mule::gui::requestMapDialog(state.menuBarState.fileDialog, uo_mule::gui::MapFileDialogMode::newType);
    REQUIRE(state.dialogHost.owns(uo_mule::gui::DialogId::mapWorkflow));
    REQUIRE_FALSE(state.dialogHost.owns(uo_mule::gui::DialogId::fileBrowser));
}

TEST_CASE("dialogWorkflow_Should_RouteMapBrowserResultThroughHost")
{
    uo_mule::gui::WorkbenchState state;
    uo_mule::gui::requestFileBrowser(state.fileBrowser,
                                     {
                                         .mode = uo_mule::gui::FileBrowserMode::openFile,
                                         .target = uo_mule::gui::FileBrowserTarget::mapOpen,
                                         .initialPath = std::filesystem::current_path(),
                                         .extension = ".mul",
                                         .title = "Open Map",
                                     });
    const auto generation = state.dialogHost.active.value_or(uo_mule::gui::DialogRequest{}).generation;
    const uo_mule::gui::FileBrowserResult result{
        .target = uo_mule::gui::FileBrowserTarget::mapOpen,
        .path = "map0.mul",
        .mapCandidates = {},
        .assetCandidates = {},
        .assetCandidate = std::nullopt,
    };

    REQUIRE_FALSE(uo_mule::gui::completeDialogResult(state,
                                                     {.id = uo_mule::gui::DialogId::fileBrowser,
                                                      .action = uo_mule::gui::DialogAction::accept,
                                                      .payload = result,
                                                      .generation = generation}));
    applyDeferredActions(state);

    REQUIRE(state.menuBarState.fileDialog.mode == uo_mule::gui::MapFileDialogMode::selectMapFacet);
    REQUIRE(state.dialogHost.owns(uo_mule::gui::DialogId::mapWorkflow));
}

TEST_CASE("dialogWorkflow_Should_RouteCompanionSelectionBackToFileBrowser")
{
    uo_mule::gui::WorkbenchState state;
    const uo_mule::gui::FileBrowserResult origin{
        .target = uo_mule::gui::FileBrowserTarget::genericOpen,
        .path = "texidx.mul",
        .mapCandidates = {},
        .assetCandidates = {},
        .assetCandidate = std::nullopt,
    };
    uo_mule::gui::openFileSelector(state.fileSelector, origin);
    const auto generation = state.dialogHost.active.value_or(uo_mule::gui::DialogRequest{}).generation;

    REQUIRE_FALSE(uo_mule::gui::completeDialogResult(state,
                                                     {.id = uo_mule::gui::DialogId::fileSelector,
                                                      .action = uo_mule::gui::DialogAction::chooseCompanion,
                                                      .payload = origin,
                                                      .generation = generation}));

    REQUIRE(state.dialogHost.owns(uo_mule::gui::DialogId::fileBrowser));
    REQUIRE(state.fileBrowser.request.target == uo_mule::gui::FileBrowserTarget::texmapsCompanion);
}

TEST_CASE("dialogWorkflow_Should_ApplyPreferenceFileBrowserResultAndReactivatePreferences")
{
    uo_mule::gui::WorkbenchState state;
    uo_mule::gui::requestPreferencesDialog(state.menuBarState.preferencesDialog, state.activeProfile, state.preferences);
    uo_mule::gui::requestFileBrowser(state.fileBrowser,
                                     {
                                         .mode = uo_mule::gui::FileBrowserMode::chooseDirectory,
                                         .target = uo_mule::gui::FileBrowserTarget::dataSearchRoot,
                                         .initialPath = {},
                                         .extension = {},
                                         .title = "Search Metadata",
                                     });
    const auto generation = state.dialogHost.active.value_or(uo_mule::gui::DialogRequest{}).generation;
    const auto root = std::filesystem::temp_directory_path() / "metadata-root";
    const uo_mule::gui::FileBrowserResult result{
        .target = uo_mule::gui::FileBrowserTarget::dataSearchRoot,
        .path = root,
        .mapCandidates = {},
        .assetCandidates = {},
        .assetCandidate = std::nullopt,
    };

    REQUIRE_FALSE(uo_mule::gui::completeDialogResult(state,
                                                     {.id = uo_mule::gui::DialogId::fileBrowser,
                                                      .action = uo_mule::gui::DialogAction::accept,
                                                      .payload = result,
                                                      .generation = generation}));
    applyDeferredActions(state);

    REQUIRE(state.menuBarState.preferencesDialog.workingProfile.sourceRoot == root);
    REQUIRE(state.dialogHost.owns(uo_mule::gui::DialogId::preferences));
}

TEST_CASE("dialogWorkflow_Should_CancelFileBrowserThroughHost")
{
    uo_mule::gui::WorkbenchState state;
    uo_mule::gui::requestFileBrowser(state.fileBrowser,
                                     {
                                         .mode = uo_mule::gui::FileBrowserMode::openFile,
                                         .target = uo_mule::gui::FileBrowserTarget::genericOpen,
                                         .initialPath = {},
                                         .extension = ".mul",
                                         .title = "Open MUL File",
                                     });
    const auto generation = state.dialogHost.active.value_or(uo_mule::gui::DialogRequest{}).generation;

    REQUIRE_FALSE(uo_mule::gui::completeDialogResult(state,
                                                     {.id = uo_mule::gui::DialogId::fileBrowser,
                                                      .action = uo_mule::gui::DialogAction::cancel,
                                                      .payload = {},
                                                      .generation = generation}));

    REQUIRE_FALSE(state.dialogHost.active.has_value());
    REQUIRE_FALSE(state.fileBrowser.visible);
}

TEST_CASE("dialogWorkflow_Should_RejectStaleResultAfterDialogReplacement")
{
    uo_mule::gui::WorkbenchState state;
    uo_mule::gui::requestFileBrowser(state.fileBrowser,
                                     {
                                         .mode = uo_mule::gui::FileBrowserMode::openFile,
                                         .target = uo_mule::gui::FileBrowserTarget::genericOpen,
                                         .initialPath = {},
                                         .extension = ".mul",
                                         .title = "Open MUL File",
                                     });
    const auto staleGeneration = state.dialogHost.active.value_or(uo_mule::gui::DialogRequest{}).generation;
    uo_mule::gui::requestMapDialog(state.menuBarState.fileDialog, uo_mule::gui::MapFileDialogMode::newType);

    REQUIRE_FALSE(uo_mule::gui::completeDialogResult(state,
                                                     {.id = uo_mule::gui::DialogId::fileBrowser,
                                                      .action = uo_mule::gui::DialogAction::cancel,
                                                      .payload = {},
                                                      .generation = staleGeneration}));
    REQUIRE(state.dialogHost.owns(uo_mule::gui::DialogId::mapWorkflow));
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

TEST_CASE("mapFacetCancel_Should_ReturnToTheGenericFileBrowser")
{
    uo_mule::gui::NewFileDialogState state;
    uo_mule::gui::FileBrowserState browser;
    const auto mapPath = std::filesystem::temp_directory_path() / "map0.mul";
    uo_mule::gui::copyToBuffer(state.openPath, mapPath);
    state.mode = uo_mule::gui::MapFileDialogMode::selectMapFacet;

    uo_mule::gui::returnToFileBrowser(state, browser);

    REQUIRE(browser.visible);
    REQUIRE(browser.request.target == uo_mule::gui::FileBrowserTarget::genericOpen);
    REQUIRE(browser.directory == mapPath.parent_path());
    REQUIRE(state.mode == uo_mule::gui::MapFileDialogMode::none);
}

TEST_CASE("fileSelectorEscape_Should_ReturnToTheOriginatingFileBrowser")
{
    uo_mule::gui::WorkbenchState state;
    const auto selectedPath = std::filesystem::temp_directory_path() / "asset.mul";
    state.fileSelector.visible = true;
    state.fileSelector.browserResult = {
        .target = uo_mule::gui::FileBrowserTarget::genericOpen,
        .role = uo_mule::gui::DataFileRole::radarcol,
        .path = selectedPath,
        .mapCandidates = {},
        .assetCandidates = {},
        .assetCandidate = std::nullopt,
    };
    state.menuBarState.fileDialog.mode = uo_mule::gui::MapFileDialogMode::openMap;

    REQUIRE(uo_mule::gui::handleFileWorkflowEscape(state));

    REQUIRE(state.fileBrowser.visible);
    REQUIRE(state.fileBrowser.request.target == uo_mule::gui::FileBrowserTarget::genericOpen);
    REQUIRE(state.fileBrowser.directory == selectedPath.parent_path());
    REQUIRE_FALSE(state.fileSelector.visible);
    REQUIRE(state.menuBarState.fileDialog.mode == uo_mule::gui::MapFileDialogMode::none);
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

TEST_CASE("newAssetWorkflow_Should_CreateTiledataAndTexmapsFromBrowserResults")
{
    const TemporaryDirectory directory;
    for (const auto & [target, filename, companion] : {
             std::tuple{uo_mule::gui::FileBrowserTarget::newTiledataOutput, "tiledata.mul", ""},
             std::tuple{uo_mule::gui::FileBrowserTarget::newTexmapsData, "texmaps.mul", "texidx.mul"},
         })
    {
        uo_mule::gui::WorkbenchState state;
        const auto path = directory.path() / filename;
        uo_mule::gui::requestFileBrowser(state.fileBrowser,
                                         {
                                             .mode = uo_mule::gui::FileBrowserMode::saveFile,
                                             .target = target,
                                             .initialPath = path,
                                             .extension = ".mul",
                                             .title = "Create",
                                         });
        const auto generation = state.dialogHost.active.value_or(uo_mule::gui::DialogRequest{}).generation;
        const uo_mule::gui::FileBrowserResult result{
            .target = target,
            .path = path,
            .mapCandidates = {},
            .assetCandidates = {},
            .assetCandidate = std::nullopt,
        };

        REQUIRE_FALSE(uo_mule::gui::completeDialogResult(state,
                                                         {.id = uo_mule::gui::DialogId::fileBrowser,
                                                          .action = uo_mule::gui::DialogAction::accept,
                                                          .payload = result,
                                                          .generation = generation}));
        applyDeferredActions(state);

        REQUIRE(std::filesystem::exists(path));
        if (!std::string_view(companion).empty())
        {
            REQUIRE(std::filesystem::exists(directory.path() / companion));
        }
    }
}

TEST_CASE("genericOpenWorkflow_Should_ReportUnsupportedResult")
{
    uo_mule::gui::WorkbenchState state;
    const auto path = std::filesystem::temp_directory_path() / "unsupported-workflow.mul";
    uo_mule::gui::requestFileBrowser(state.fileBrowser,
                                     {
                                         .mode = uo_mule::gui::FileBrowserMode::openFile,
                                         .target = uo_mule::gui::FileBrowserTarget::genericOpen,
                                         .initialPath = path,
                                         .extension = ".mul",
                                         .title = "Open",
                                     });
    const auto generation = state.dialogHost.active.value_or(uo_mule::gui::DialogRequest{}).generation;
    const uo_mule::gui::FileBrowserResult result{
        .target = uo_mule::gui::FileBrowserTarget::genericOpen,
        .path = path,
        .mapCandidates = {},
        .assetCandidates = {},
        .assetCandidate = std::nullopt,
    };

    REQUIRE_FALSE(uo_mule::gui::completeDialogResult(state,
                                                     {.id = uo_mule::gui::DialogId::fileBrowser,
                                                      .action = uo_mule::gui::DialogAction::accept,
                                                      .payload = result,
                                                      .generation = generation}));
    REQUIRE_FALSE(state.notifications.entries.empty());
    REQUIRE(state.notifications.entries.back().severity == uo_mule::gui::NotificationSeverity::error);
}

TEST_CASE("genericOpenWorkflow_Should_DeferOpenableStandaloneAsset")
{
    const TemporaryDirectory directory;
    const auto path = directory.path() / "standalone-radarcol.mul";
    auto editor = uomul::RadarColEditor::createEmpty(path);
    REQUIRE(editor);
    REQUIRE(editor->commit());

    WorkbenchState state;
    requestFileBrowser(state.fileBrowser,
                       {
                           .mode = FileBrowserMode::openFile,
                           .target = FileBrowserTarget::genericOpen,
                           .initialPath = path,
                           .extension = ".mul",
                           .title = "Open",
                       });
    const auto generation = state.dialogHost.active.value_or(DialogRequest{}).generation;
    const FileBrowserAssetCandidate candidate{
        .kind = MulAssetKind::radarcol,
        .path = path,
        .companionPath = {},
        .label = "Radar colors",
        .facet = {},
        .reason = {},
        .warning = {},
        .score = 0,
        .itemCount = 0,
        .dimensions = std::nullopt,
        .validated = true,
        .openable = true,
    };
    const FileBrowserResult result{
        .target = FileBrowserTarget::genericOpen,
        .path = path,
        .mapCandidates = {},
        .assetCandidates = {candidate},
        .assetCandidate = std::nullopt,
    };

    REQUIRE_FALSE(completeDialogResult(
        state, {.id = DialogId::fileBrowser, .action = DialogAction::accept, .payload = result, .generation = generation}));
    applyDeferredActions(state);

    REQUIRE_FALSE(state.session.recentFiles().empty());
    REQUIRE(state.session.recentFiles().front().kind == DocumentKind::radarcol);
}

TEST_CASE("fileWorkflow_Should_RouteTexmapsSetToPreferences")
{
    WorkbenchState state;
    requestPreferencesDialog(state.menuBarState.preferencesDialog, state.activeProfile, state.preferences);
    requestFileBrowser(state.fileBrowser,
                       {
                           .mode = FileBrowserMode::openFile,
                           .target = FileBrowserTarget::texmapsIndex,
                           .initialPath = {},
                           .extension = ".mul",
                           .title = "Choose Texmaps",
                       });
    const auto generation = state.dialogHost.active.value_or(DialogRequest{}).generation;
    const FileBrowserAssetCandidate candidate{
        .kind = MulAssetKind::texmapsSet,
        .path = "texidx.mul",
        .companionPath = "texmaps.mul",
        .label = "Texmaps",
        .facet = {},
        .reason = {},
        .warning = {},
        .score = 0,
        .itemCount = 0,
        .dimensions = std::nullopt,
        .validated = true,
        .openable = true,
    };
    const FileBrowserResult result{
        .target = FileBrowserTarget::texmapsIndex,
        .role = DataFileRole::texmaps,
        .path = candidate.path,
        .mapCandidates = {},
        .assetCandidates = {},
        .assetCandidate = candidate,
    };

    REQUIRE_FALSE(completeDialogResult(
        state, {.id = DialogId::fileBrowser, .action = DialogAction::accept, .payload = result, .generation = generation}));

    REQUIRE(state.menuBarState.preferencesDialog.visible);
    REQUIRE(state.dialogHost.owns(DialogId::preferences));
}

TEST_CASE("imageWorkflow_Should_DeferEachRegisteredCallback")
{
    const auto path = std::filesystem::temp_directory_path() / "replacement.png";
    for (const auto target : {
             FileBrowserTarget::imageOpen,
             FileBrowserTarget::imageFolderOpen,
             FileBrowserTarget::texmapExportFile,
             FileBrowserTarget::texmapExportFolder,
         })
    {
        WorkbenchState state;
        bool called = false;
        state.texmapsState.replacementFilePicked = [&](const auto &) { called = true; };
        state.texmapsState.folderReplacementFolderPicked = [&](const auto &) { called = true; };
        state.texmapsState.exportFilePicked = [&](const auto &) { called = true; };
        state.texmapsState.exportFolderPicked = [&](const auto &) { called = true; };
        requestFileBrowser(
            state.fileBrowser,
            {.mode = FileBrowserMode::openFile, .target = target, .initialPath = path, .extension = {}, .title = "Choose"});
        const auto generation = state.dialogHost.active.value_or(DialogRequest{}).generation;
        const FileBrowserResult result{
            .target = target,
            .path = path,
            .mapCandidates = {},
            .assetCandidates = {},
            .assetCandidate = std::nullopt,
        };

        REQUIRE_FALSE(completeDialogResult(
            state, {.id = DialogId::fileBrowser, .action = DialogAction::accept, .payload = result, .generation = generation}));
        applyDeferredActions(state);
        REQUIRE(called);
    }
}
