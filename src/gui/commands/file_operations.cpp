#include "gui/commands/file_operations.h"
#include "gui/application/workbench_state.h"
#include "gui/context/notification_state.h"
#include "gui/dialogs/dialog_contracts.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/dialogs/file_selector_dialog.h"
#include "gui/dialogs/new_map_dialog.h"
#include "gui/dialogs/preferences_dialog.h"
#include "gui/documents/document_identity.h"
#include "gui/metadata/document_contracts.h"
#include "gui/profile/workbench_session.h"
#include "gui/views/radarcol_panel.h"
#include "gui/views/texmaps_panel.h"
#include <string>
#include <utility>

namespace uo_mule::gui
{
namespace
{

void syncDialogHost(WorkbenchState & state)
{
    const auto mapWorkflow = [](MapFileDialogMode mode) noexcept {
        switch (mode)
        {
            case MapFileDialogMode::openMap:
                return MapDialogWorkflow::open;
            case MapFileDialogMode::newMap:
                return MapDialogWorkflow::create;
            case MapFileDialogMode::saveAs:
                return MapDialogWorkflow::saveAs;
            case MapFileDialogMode::dirtyChanges:
                return MapDialogWorkflow::dirtyChanges;
            case MapFileDialogMode::overwrite:
                return MapDialogWorkflow::overwrite;
            case MapFileDialogMode::error:
                return MapDialogWorkflow::error;
            case MapFileDialogMode::selectMapFacet:
                return MapDialogWorkflow::create;
            case MapFileDialogMode::none:
                break;
        }
        return MapDialogWorkflow::error;
    };
    const bool browserVisible = state.fileBrowser.visible;
    const bool selectorVisible = state.fileSelector.visible;
    const bool mapDialogVisible = state.menuBarState.fileDialog.mode != MapFileDialogMode::none;
    const bool preferencesVisible = state.menuBarState.preferencesDialog.visible;
    if (browserVisible)
    {
        auto request = makeDialogRequest(DialogId::fileBrowser,
                                         FileBrowserDialogRequest{.request = state.fileBrowser.request});
        request.replaceActive = true;
        state.dialogHost.activate(std::move(request));
    }
    else if (selectorVisible)
    {
        state.dialogHost.activate(makeDialogRequest(DialogId::fileSelector,
                                                     FileSelectorDialogRequest{.sourcePath = state.fileSelector.browserResult.path}));
    }
    else if (mapDialogVisible)
    {
        state.dialogHost.activate(makeDialogRequest(DialogId::mapWorkflow,
                                                     MapDialogRequest{.workflow = mapWorkflow(state.menuBarState.fileDialog.mode)}));
    }
    else if (preferencesVisible)
    {
        state.dialogHost.activate(makeDialogRequest(DialogId::preferences));
    }
    else if (state.dialogHost.active)
    {
        state.dialogHost.cancel();
    }
}

void cancelFileBrowserAndMapDialog(WorkbenchState & state)
{
    cancelFileBrowser(state.fileBrowser);
    if (state.menuBarState.fileDialog.mode != MapFileDialogMode::none)
    {
        cancelMapDialog(state.menuBarState.fileDialog);
    }
}

bool openGenericAsset(WorkbenchState & state,
                      const FileBrowserResult & result,
                      const FileBrowserAssetCandidate & candidate)
{
    if (candidate.kind == MulAssetKind::map)
    {
        auto mapResult = result;
        mapResult.target = FileBrowserTarget::mapOpen;
        applyFileBrowserResult(state.menuBarState.fileDialog, mapResult);
        return false;
    }

    // Generic Open starts with the map workflow for map candidates. Once an
    // asset is selected, that workflow must not remain active behind the new
    // standalone document.
    cancelMapDialog(state.menuBarState.fileDialog);

    DocumentSummary summary;
    summary.identity.kind = candidate.kind == MulAssetKind::radarcol ? DocumentKind::radarcol : DocumentKind::texmapPair;
    summary.identity.source = candidate.kind == MulAssetKind::radarcol
                                  ? makeDocumentSourceIdentity({candidate.path})
                                  : makeDocumentSourceIdentity({candidate.path, candidate.companionPath});
    summary.displayName = candidate.path.filename().string();
    summary.access = candidate.kind == MulAssetKind::texmapsPair ? DocumentAccess::readOnly : DocumentAccess::editable;
    summary.validation = candidate.validated ? DocumentValidationState::valid : DocumentValidationState::invalid;
    const DocumentId id = state.session.openDocument(std::move(summary));
    if (id == 0)
    {
        return false;
    }

    const auto activated = state.session.activateDocument(id, state.workspace);
    if (!activated)
    {
        state.notifications.add(NotificationSeverity::error, activated.error().message);
    }
    state.session.recordRecentFile(candidate.path,
                                   candidate.kind == MulAssetKind::radarcol ? DocumentKind::radarcol : DocumentKind::texmapPair,
                                   candidate.companionPath,
                                   candidate.itemCount);
    return false;
}

[[nodiscard]] bool routeFileBrowserResult(WorkbenchState & state, const FileBrowserResult & result)
{
    if (result.target == FileBrowserTarget::texmapsCompanion)
    {
        static_cast<void>(applyFileSelectorCompanion(state.fileSelector, result.path));
        return false;
    }

    if (result.target == FileBrowserTarget::genericOpen && !result.assetCandidate && result.assetCandidates.empty())
    {
        state.notifications.add(NotificationSeverity::error,
                                result.path.filename().string() + ": no supported MUL format was detected");
        return false;
    }

    if (!result.assetCandidate && result.target == FileBrowserTarget::genericOpen && result.assetCandidates.size() == 1U &&
        result.assetCandidates.front().openable)
    {
        return openGenericAsset(state, result, result.assetCandidates.front());
    }

    if (!result.assetCandidate && (result.target == FileBrowserTarget::genericOpen ||
                                   result.target == FileBrowserTarget::radarcolSource ||
                                   result.target == FileBrowserTarget::texmapsIndex ||
                                   result.target == FileBrowserTarget::texmapsData))
    {
        openFileSelector(state.fileSelector, result);
        return false;
    }

    if (result.target == FileBrowserTarget::genericOpen && result.assetCandidate)
    {
        return openGenericAsset(state, result, *result.assetCandidate);
    }

    if (result.assetCandidate && result.assetCandidate->kind == MulAssetKind::texmapsPair)
    {
        const auto & pair = *result.assetCandidate;

        auto indexResult = result;
        indexResult.target = FileBrowserTarget::texmapsIndex;
        indexResult.path = pair.path;
        auto dataResult = result;
        dataResult.target = FileBrowserTarget::texmapsData;
        dataResult.path = pair.companionPath;
        applyFileBrowserResult(state.texmapsState, indexResult);
        applyFileBrowserResult(state.texmapsState, dataResult);
        applyFileBrowserResult(state.menuBarState.preferencesDialog, indexResult);
        applyFileBrowserResult(state.menuBarState.preferencesDialog, dataResult);
        return false;
    }

    applyFileBrowserResult(state.menuBarState.fileDialog, result);
    applyFileBrowserResult(state.menuBarState.preferencesDialog, result);
    if (result.target == FileBrowserTarget::radarcolOutput)
    {
        if (auto * active = state.session.activeDocumentRecord(); active != nullptr && active->radarcol)
        {
            active->radarcol->setOutputPath(result.path);
        }
    }
    else
    {
        applyFileBrowserResult(state.radarcolState, result);
    }
    applyFileBrowserResult(state.texmapsState, result);
    return false;
}

bool handleFileSelector(WorkbenchState & state)
{
    if (!state.dialogHost.owns(DialogId::fileSelector) || state.fileBrowser.visible || !state.fileSelector.visible)
    {
        return false;
    }

    const auto action = drawFileSelector(state.fileSelector);
    if (action == FileSelectorAction::accept)
    {
        const auto selected = selectedFileSelectorResult(state.fileSelector);
        cancelFileSelector(state.fileSelector);
        return routeFileBrowserResult(state, selected);
    }
    if (action == FileSelectorAction::cancel)
    {
        cancelFileSelector(state.fileSelector);
        return false;
    }
    if (action == FileSelectorAction::chooseDifferent)
    {
        const auto original = state.fileSelector.browserResult;
        cancelFileSelector(state.fileSelector);
        requestFileBrowser(state.fileBrowser,
                           {.mode = FileBrowserMode::openFile,
                            .target = original.target,
                            .initialPath = original.path,
                            .extension = ".mul",
                            .title = "Open Texture Pair Component",});
        return false;
    }
    if (action == FileSelectorAction::chooseCompanion)
    {
        const auto original = state.fileSelector.browserResult;
        requestFileBrowser(state.fileBrowser,
                           {.mode = FileBrowserMode::openFile,
                            .target = FileBrowserTarget::texmapsCompanion,
                            .initialPath = original.path.parent_path(),
                            .extension = ".mul",
                            .title = "Choose Texture Pair Companion",});
    }
    return false;
}

bool drawFileDialogs(WorkbenchState & state)
{
    bool profileChanged = false;
    syncDialogHost(state);
    if (state.menuBarState.fileDialog.shouldClose)
    {
        state.shouldClose = true;
        state.menuBarState.fileDialog.shouldClose = false;
    }

    const bool browserWasVisible = state.fileBrowser.visible;
    if (!browserWasVisible && !state.fileSelector.visible && state.dialogHost.owns(DialogId::mapWorkflow))
    {
        drawNewMapDialog(state.workspace,
                         state.menuBarState.fileDialog,
                         state.operationController,
                         state.activeProfile.outputCollisionPolicy,
                         state.notifications,
                         state.fileBrowser,
                         state.shouldClose);
    }
    if (!browserWasVisible && !state.fileSelector.visible && state.dialogHost.owns(DialogId::preferences))
    {
        profileChanged = drawPreferencesDialog(
            state.menuBarState.preferencesDialog, state.activeProfile, state.preferences, state.fileBrowser);
    }
    if (state.dialogHost.owns(DialogId::fileBrowser))
    {
        drawFileBrowser(state.fileBrowser);
    }
    const auto result = consumeFileBrowserResult(state.fileBrowser);
    if (!state.fileBrowser.visible && browserWasVisible && !result &&
        state.menuBarState.fileDialog.mode != MapFileDialogMode::none)
    {
        cancelMapDialog(state.menuBarState.fileDialog);
    }
    if (result)
    {
        state.dialogHost.complete({.id = DialogId::fileBrowser,
                                   .action = DialogAction::accept,
                                   .payload = *result,});
        profileChanged = routeFileBrowserResult(state, *result) || profileChanged;
    }
    return handleFileSelector(state) || profileChanged;
}

} // namespace

bool drawFileOperations(WorkbenchState & state)
{
    return drawFileDialogs(state);
}

bool handleFileOperationsEscape(WorkbenchState & state)
{
    if (state.fileBrowser.visible)
    {
        cancelFileBrowserAndMapDialog(state);
    }
    else if (state.fileSelector.visible)
    {
        cancelFileSelector(state.fileSelector);
    }
    else if (state.menuBarState.fileDialog.mode != MapFileDialogMode::none)
    {
        cancelMapDialog(state.menuBarState.fileDialog);
    }
    else if (state.menuBarState.preferencesDialog.visible)
    {
        state.menuBarState.preferencesDialog.visible = false;
        state.menuBarState.preferencesDialog.closeRequested = true;
    }
    else
    {
        return false;
    }
    return true;
}

} // namespace uo_mule::gui
