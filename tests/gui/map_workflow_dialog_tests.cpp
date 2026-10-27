#include "gui/dialogs/file_browser_dialog.h"
#include "gui/dialogs/map_workflow_dialog.h"
#include "gui/presentation/bounded_buffer.h"
#include "gui/workbench/workbench_state.h"
#include "gui/workflows/file_workflow.h"
#include "gui_model/dialogs/dialog_contracts.h"
#include "gui_model/dialogs/dialog_host.h"
#include "gui_model/documents/map_document_state.h"
#include "gui_model/operations/operation_controller.h"
#include "gui_model/profile/data_profile.h"
#include "imgui_test_support.h"
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <uomul/facet_sizes.h>
#include <uomul/map/map.h>

namespace
{

constexpr int resetTileId = 123;
constexpr int resetTileZ = -4;
constexpr int retainedTileId = 42;
constexpr float radarRed = 1.0F;
constexpr float radarGreen = 0.5F;
constexpr float radarBlue = 0.25F;
constexpr std::uint64_t invalidBlockCount = 0U;
constexpr std::uint64_t validBlockCount = 1U;
constexpr std::size_t expectedCandidateCount = 2U;
constexpr int firstFacetIndex = 1;

} // namespace

using uo_mule::gui::applyFileBrowserResult;
using uo_mule::gui::cancelFileDialog;
using uo_mule::gui::copyToBuffer;
using uo_mule::gui::DialogId;
using uo_mule::gui::DialogRequest;
using uo_mule::gui::drawNewFileDialog;
using uo_mule::gui::FileBrowserMapCandidate;
using uo_mule::gui::FileBrowserState;
using uo_mule::gui::FileBrowserTarget;
using uo_mule::gui::handleFileWorkflowEscape;
using uo_mule::gui::MapFileDialogMode;
using uo_mule::gui::MapOverwriteKind;
using uo_mule::gui::NewFileDialogState;
using uo_mule::gui::NewFileKind;
using uo_mule::gui::OperationKind;
using uo_mule::gui::OperationRequest;
using uo_mule::gui::OutputCollisionPolicy;
using uo_mule::gui::requestMapDialog;
using uo_mule::gui::returnToFileBrowser;
using uo_mule::gui::WorkbenchState;
using uo_mule::gui::test::ImGuiFrame;

TEST_CASE("mapWorkflowDialog_Should_ResetNewFileFieldsWhenEnteringNewType")
{
    NewFileDialogState state;
    state.newFileKind = NewFileKind::tiledata;
    state.initialTileId = resetTileId;
    state.initialTileZ = resetTileZ;
    state.radarDefaultColor = {radarRed, radarGreen, radarBlue};
    state.useEmbeddedRadarColors = false;

    requestMapDialog(state, MapFileDialogMode::newType);

    REQUIRE(state.mode == MapFileDialogMode::newType);
    REQUIRE(state.newFileKind == NewFileKind::map);
    REQUIRE(state.initialTileId == 0);
    REQUIRE(state.initialTileZ == 0);
    REQUIRE(state.useEmbeddedRadarColors);
}

TEST_CASE("mapWorkflowDialog_Should_KeepNewFileFieldsWhenReenteringSameMode")
{
    NewFileDialogState state;
    requestMapDialog(state, MapFileDialogMode::newType);
    state.newFileKind = NewFileKind::radarcol;
    state.initialTileId = retainedTileId;

    requestMapDialog(state, MapFileDialogMode::newType);

    REQUIRE(state.newFileKind == NewFileKind::radarcol);
    REQUIRE(state.initialTileId == 42);
}

TEST_CASE("mapWorkflowDialog_Should_ApplyMapBrowserCandidatesAndFacet")
{
    NewFileDialogState state;
    const auto facet = uomul::knownFacetSizes().front();
    const uomul::Dimensions dimensions{.widthTiles = facet.widthTiles, .heightTiles = facet.heightTiles};
    const FileBrowserMapCandidate invalid{
        .presetName = std::string(facet.name),
        .facet = {},
        .dimensions = dimensions,
        .expectedBlocks = invalidBlockCount,
        .completeBlocks = invalidBlockCount,
        .missingBlocks = invalidBlockCount,
        .trailingBytes = invalidBlockCount,
        .valid = false,
    };
    const FileBrowserMapCandidate valid{
        .presetName = std::string(facet.name),
        .facet = {},
        .dimensions = dimensions,
        .expectedBlocks = validBlockCount,
        .completeBlocks = validBlockCount,
        .missingBlocks = invalidBlockCount,
        .trailingBytes = invalidBlockCount,
        .valid = true,
    };
    applyFileBrowserResult(state,
                           {
                               .target = FileBrowserTarget::openMap,
                               .path = "map0.mul",
                               .mapCandidates = {invalid, valid},
                               .assetCandidates = {},
                               .assetCandidate = std::nullopt,
                           });

    REQUIRE(state.mode == MapFileDialogMode::selectMapFacet);
    REQUIRE(std::string(state.openPath.data()) == "map0.mul");
    REQUIRE(state.openCandidates.size() == expectedCandidateCount);
    REQUIRE(state.openFacet == firstFacetIndex);
    REQUIRE(state.openWidth == dimensions.widthTiles);
}

TEST_CASE("mapWorkflowDialog_Should_ApplyOutputResultsOnlyToMatchingModes")
{
    NewFileDialogState state;
    state.mode = MapFileDialogMode::newType;
    applyFileBrowserResult(state,
                           {
                               .target = FileBrowserTarget::createMap,
                               .path = "new.map",
                               .mapCandidates = {},
                               .assetCandidates = {},
                               .assetCandidate = std::nullopt,
                           });
    REQUIRE(std::string(state.newMapPath.data()) == "new.map");

    state.mode = MapFileDialogMode::saveAs;
    applyFileBrowserResult(state,
                           {
                               .target = FileBrowserTarget::saveMapAs,
                               .path = "saved.map",
                               .mapCandidates = {},
                               .assetCandidates = {},
                               .assetCandidate = std::nullopt,
                           });
    REQUIRE(std::string(state.savePath.data()) == "saved.map");

    const auto previousPath = std::string(state.savePath.data());
    applyFileBrowserResult(state,
                           {
                               .target = FileBrowserTarget::openGeneric,
                               .path = "ignored.mul",
                               .mapCandidates = {},
                               .assetCandidates = {},
                               .assetCandidate = std::nullopt,
                           });
    REQUIRE(std::string(state.savePath.data()) == previousPath);
}

TEST_CASE("mapWorkflowDialog_Should_ReturnToGenericFileBrowser")
{
    const auto root = std::filesystem::temp_directory_path() / "uo-mule-map-workflow-return";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root / "maps", error);
    NewFileDialogState state;
    FileBrowserState browser;
    const auto selectedPath = root / "maps" / "map0.mul";
    copyToBuffer(state.openPath, selectedPath);
    state.mode = MapFileDialogMode::selectMapFacet;

    returnToFileBrowser(state, browser);

    REQUIRE(state.mode == MapFileDialogMode::none);
    REQUIRE(browser.visible);
    REQUIRE(browser.request.target == FileBrowserTarget::openGeneric);
    REQUIRE(browser.directory == (root / "maps").lexically_normal());
    std::filesystem::remove_all(root, error);
}

TEST_CASE("mapWorkflowDialog_Should_CancelAndReleaseDialogHost")
{
    WorkbenchState state;
    requestMapDialog(state.menuBarState.fileDialog, MapFileDialogMode::newType);
    REQUIRE(state.dialogHost.owns(DialogId::mapWorkflow));

    cancelFileDialog(state.menuBarState.fileDialog);

    REQUIRE(state.menuBarState.fileDialog.mode == MapFileDialogMode::none);
    REQUIRE_FALSE(state.dialogHost.active.has_value());
}

TEST_CASE("mapWorkflowDialog_Should_CancelFailedSaveAsAndAllowRetry")
{
    WorkbenchState state;
    auto & dialog = state.menuBarState.fileDialog;
    dialog.controller = &state.operationController;
    dialog.saveForPendingOperation = true;
    copyToBuffer(dialog.savePath, "retry-map.mul");
    requestMapDialog(dialog, MapFileDialogMode::saveAs);
    REQUIRE(state.dialogHost.active.has_value());
    const auto firstGeneration = state.dialogHost.active.value_or(DialogRequest{}).generation;

    OperationRequest request{
        .kind = OperationKind::saveAs,
        .documentId = std::nullopt,
        .path = "retry-map.mul",
        .dimensions = uomul::Dimensions(),
        .initialTile = uomul::MapTile(),
    };
    REQUIRE(state.operationController.begin(request, true, false, OutputCollisionPolicy::confirm).kind ==
            uo_mule::gui::OperationTransitionKind::showDirtyDecision);
    REQUIRE(state.operationController.resolveDirty(uo_mule::gui::PendingChangesAction::save).kind ==
            uo_mule::gui::OperationTransitionKind::showSaveDialog);
    REQUIRE(state.operationController.completeSave(false).kind == uo_mule::gui::OperationTransitionKind::saveFailed);

    cancelFileDialog(dialog);

    REQUIRE(state.operationController.phase() == uo_mule::gui::OperationPhase::idle);
    REQUIRE(dialog.mode == MapFileDialogMode::none);
    REQUIRE(dialog.savePath.front() == '\0');
    REQUIRE_FALSE(dialog.saveForPendingOperation);
    REQUIRE_FALSE(state.dialogHost.active.has_value());

    requestMapDialog(dialog, MapFileDialogMode::saveAs);
    REQUIRE(state.dialogHost.active.has_value());
    REQUIRE(state.dialogHost.active.value_or(DialogRequest{}).generation > firstGeneration);
    REQUIRE(state.operationController.begin(request, false, false, OutputCollisionPolicy::confirm).kind ==
            uo_mule::gui::OperationTransitionKind::showSaveDialog);
    REQUIRE(state.operationController.completeSave(true).kind == uo_mule::gui::OperationTransitionKind::execute);
    cancelFileDialog(dialog);
}

TEST_CASE("mapWorkflowDialog_Should_CancelSaveAsThroughEscape")
{
    WorkbenchState state;
    auto & dialog = state.menuBarState.fileDialog;
    dialog.controller = &state.operationController;
    dialog.mode = MapFileDialogMode::saveAs;
    copyToBuffer(dialog.savePath, "escape-map.mul");
    requestMapDialog(dialog, MapFileDialogMode::saveAs);

    OperationRequest request{
        .kind = OperationKind::saveAs,
        .documentId = std::nullopt,
        .path = "escape-map.mul",
        .dimensions = uomul::Dimensions(),
        .initialTile = uomul::MapTile(),
    };
    REQUIRE(state.operationController.begin(request, false, false, OutputCollisionPolicy::confirm).kind ==
            uo_mule::gui::OperationTransitionKind::showSaveDialog);
    REQUIRE(handleFileWorkflowEscape(state));

    REQUIRE(state.operationController.phase() == uo_mule::gui::OperationPhase::idle);
    REQUIRE(dialog.mode == MapFileDialogMode::none);
    REQUIRE(dialog.savePath.front() == '\0');
    REQUIRE_FALSE(state.dialogHost.active.has_value());
}

TEST_CASE("mapWorkflowDialog_Should_RenderEveryDialogMode")
{
    WorkbenchState workbench;
    auto & state = workbench.menuBarState.fileDialog;

    for (const auto mode : {
             MapFileDialogMode::newType,
             MapFileDialogMode::selectMapFacet,
             MapFileDialogMode::openMap,
             MapFileDialogMode::saveAs,
             MapFileDialogMode::dirtyChanges,
             MapFileDialogMode::overwrite,
             MapFileDialogMode::error,
         })
    {
        state.mode = mode;
        state.activePopup = MapFileDialogMode::none;
        state.error = mode == MapFileDialogMode::error ? "test error" : std::string{};
        state.overwriteKind = mode == MapFileDialogMode::overwrite ? MapOverwriteKind::save : MapOverwriteKind::none;
        const ImGuiFrame frame;
        drawNewFileDialog(workbench.mapDocuments,
                          state,
                          workbench.operationController,
                          OutputCollisionPolicy::confirm,
                          workbench.notifications,
                          workbench.fileBrowser,
                          workbench.preferences.idFormat);
        REQUIRE_FALSE(state.rendering);
    }
}
