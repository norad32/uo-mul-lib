#include "gui/workflows/file_operations.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/dialogs/file_selector_dialog.h"
#include "gui/dialogs/map_workflow_dialog.h"
#include "gui/dialogs/preferences_dialog.h"
#include "gui/presentation/strings/ui_strings.h"
#include "gui/state/notification_state.h"
#include "gui/state/workbench_state.h"
#include "gui_model/dialogs/dialog_contracts.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/documents/radarcol_document.h"
#include "gui_model/session/workbench_session.h"
#include "gui_model/source/source_identity.h"
#include "uomul/radarcol/radarcol.h"
#include "uomul/texmaps/texmaps.h"
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace uo_mule::gui
{
namespace
{

void syncDialogHost(WorkbenchState & state)
{
    const auto mapWorkflow = [](MapFileDialogMode mode) noexcept
    {
        switch (mode)
        {
            case MapFileDialogMode::openMap :
                return MapDialogWorkflow::open;
            case MapFileDialogMode::newMap :
                return MapDialogWorkflow::create;
            case MapFileDialogMode::saveAs :
                return MapDialogWorkflow::saveAs;
            case MapFileDialogMode::dirtyChanges :
                return MapDialogWorkflow::dirtyChanges;
            case MapFileDialogMode::overwrite :
                return MapDialogWorkflow::overwrite;
            case MapFileDialogMode::error :
                return MapDialogWorkflow::error;
            case MapFileDialogMode::newType :
            case MapFileDialogMode::selectMapFacet :
                return MapDialogWorkflow::create;
            case MapFileDialogMode::none :
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
        auto request = makeDialogRequest(DialogId::fileBrowser, FileBrowserDialogRequest{.request = state.fileBrowser.request});
        request.replaceActive = true;
        static_cast<void>(state.dialogHost.request(std::move(request)));
    }
    else if (selectorVisible)
    {
        auto request = makeDialogRequest(DialogId::fileSelector,
                                         FileSelectorDialogRequest{.sourcePath = state.fileSelector.browserResult.path});
        request.replaceActive = true;
        static_cast<void>(state.dialogHost.request(std::move(request)));
    }
    else if (mapDialogVisible)
    {
        auto request = makeDialogRequest(DialogId::mapWorkflow,
                                         MapDialogRequest{.workflow = mapWorkflow(state.menuBarState.fileDialog.mode)});
        request.replaceActive = true;
        static_cast<void>(state.dialogHost.request(std::move(request)));
    }
    else if (preferencesVisible)
    {
        auto request = makeDialogRequest(DialogId::preferences);
        request.replaceActive = true;
        static_cast<void>(state.dialogHost.request(std::move(request)));
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
        cancelFileDialog(state.menuBarState.fileDialog);
    }
}

void openCreatedAsset(WorkbenchState & state,
                      DocumentKind kind,
                      const std::filesystem::path & primaryPath,
                      const std::filesystem::path & companionPath = {})
{
    const DocumentSourceIdentity source = companionPath.empty() ? makeDocumentSourceIdentity({primaryPath})
                                                                : makeRoleAwareDocumentSourceIdentity({
                                                                      {.role = SourceRole::index, .path = primaryPath},
                                                                      {.role = SourceRole::data, .path = companionPath},
                                                                  });
    const auto summary = state.documentDescriptors.createSummary({
        .kind = kind,
        .source = source,
        .displayName = primaryPath.filename().string(),
        .access = kind == DocumentKind::radarcol ? DocumentAccess::editable : DocumentAccess::readOnly,
        .validation = DocumentValidationState::valid,
        .facetKey = std::nullopt,
        .dimensions = std::nullopt,
    });
    if (!summary)
    {
        state.notifications.add(NotificationSeverity::error, "Document kind is not available");
        return;
    }
    const DocumentId id = state.session.openDocument(*summary);
    if (id == 0)
    {
        return;
    }
    if (auto activated = state.session.activateDocument(id, state.workspace); !activated)
    {
        state.notifications.add(NotificationSeverity::error, activated.error().message);
    }
}

void createNewAsset(WorkbenchState & state, FileBrowserTarget target, const std::filesystem::path & path)
{
    if (target == FileBrowserTarget::newRadarcolOutput)
    {
        auto created = uomul::RadarColEditor::createEmpty(path);
        if (!created)
        {
            state.notifications.add(NotificationSeverity::error, strings::formatError(created.error()));
            return;
        }
        if (auto committed = created->commit(); !committed)
        {
            state.notifications.add(NotificationSeverity::error, strings::formatError(committed.error()));
            return;
        }
        openCreatedAsset(state, DocumentKind::radarcol, path);
        return;
    }

    const std::filesystem::path indexPath = path.filename() == "texmaps.mul"
                                                ? path.parent_path() / "texidx.mul"
                                                : path.parent_path() / (path.stem().string() + ".idx");
    auto created = uomul::TexMapsEditor::createEmpty(indexPath, path);
    if (!created)
    {
        state.notifications.add(NotificationSeverity::error, strings::formatError(created.error()));
        return;
    }
    if (auto committed = created->commit(); !committed)
    {
        state.notifications.add(NotificationSeverity::error, strings::formatError(committed.error()));
        return;
    }
    openCreatedAsset(state, DocumentKind::texmapPair, indexPath, path);
}

bool openGenericAsset(WorkbenchState & state, const FileBrowserResult & result, const FileBrowserAssetCandidate & candidate)
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
    cancelFileDialog(state.menuBarState.fileDialog);

    const DocumentKind kind = candidate.kind == MulAssetKind::radarcol ? DocumentKind::radarcol : DocumentKind::texmapPair;
    const DocumentSourceIdentity source = candidate.kind == MulAssetKind::radarcol
                                              ? makeDocumentSourceIdentity({candidate.path})
                                              : makeRoleAwareDocumentSourceIdentity({
                                                    {.role = SourceRole::index, .path = candidate.path},
                                                    {.role = SourceRole::data, .path = candidate.companionPath},
                                                });
    const auto summary = state.documentDescriptors.createSummary({
        .kind = kind,
        .source = source,
        .displayName = candidate.path.filename().string(),
        .access = candidate.kind == MulAssetKind::texmapsPair ? DocumentAccess::readOnly : DocumentAccess::editable,
        .validation = candidate.validated ? DocumentValidationState::valid : DocumentValidationState::invalid,
        .facetKey = std::nullopt,
        .dimensions = std::nullopt,
    });
    if (!summary)
    {
        state.notifications.add(NotificationSeverity::error, "Document kind is not available");
        return false;
    }
    const DocumentId id = state.session.openDocument(*summary);
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

bool routePairResult(WorkbenchState & state, const FileBrowserResult & result, const FileBrowserAssetCandidate & pair)
{
    auto indexResult = result;
    indexResult.target = FileBrowserTarget::texmapsIndex;
    indexResult.path = pair.path;
    auto dataResult = result;
    dataResult.target = FileBrowserTarget::texmapsData;
    dataResult.path = pair.companionPath;
    applyFileBrowserResult(state.menuBarState.preferencesDialog, indexResult);
    applyFileBrowserResult(state.menuBarState.preferencesDialog, dataResult);
    return false;
}

void applyAssetResult(WorkbenchState & state, const FileBrowserResult & result)
{
    applyFileBrowserResult(state.menuBarState.fileDialog, result);
    applyFileBrowserResult(state.menuBarState.preferencesDialog, result);
    if (result.target == FileBrowserTarget::radarcolOutput)
    {
        if (auto * active = state.session.activeDocumentRecord(); active != nullptr && active->radarcol)
        {
            active->radarcol->requestIntent({
                .kind = RadarcolDocumentIntentKind::saveAs,
                .outputPath = result.path,
                .overwriteOutput = false,
            });
        }
    }
}

[[nodiscard]] bool routeFileBrowserResult(WorkbenchState & state, const FileBrowserResult & result)
{
    if (result.target == FileBrowserTarget::texmapsCompanion)
    {
        static_cast<void>(applyFileSelectorCompanion(state.fileSelector, result.path));
        return false;
    }

    if (result.target == FileBrowserTarget::newRadarcolOutput || result.target == FileBrowserTarget::newTexmapsData)
    {
        createNewAsset(state, result.target, result.path);
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

    if (!result.assetCandidate &&
        (result.target == FileBrowserTarget::genericOpen || result.target == FileBrowserTarget::radarcolSource ||
         result.target == FileBrowserTarget::texmapsIndex || result.target == FileBrowserTarget::texmapsData))
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
        return routePairResult(state, result, *result.assetCandidate);
    }

    applyAssetResult(state, result);
    return false;
}

bool handleDialogResult(WorkbenchState & state, DialogResult result)
{
    if (result.action == DialogAction::cancel)
    {
        if (result.id == DialogId::fileSelector)
        {
            cancelFileSelector(state.fileSelector);
        }
        else if (result.id == DialogId::fileBrowser && state.menuBarState.fileDialog.mode != MapFileDialogMode::none)
        {
            cancelFileDialog(state.menuBarState.fileDialog);
        }
        return false;
    }

    if (result.id == DialogId::fileSelector && result.action == DialogAction::chooseCompanion &&
        std::holds_alternative<FileBrowserResult>(result.payload))
    {
        const auto original = std::get<FileBrowserResult>(result.payload);
        cancelFileSelector(state.fileSelector);
        requestFileBrowser(state.fileBrowser,
                           {
                               .mode = FileBrowserMode::openFile,
                               .target = FileBrowserTarget::texmapsCompanion,
                               .initialPath = original.path.parent_path(),
                               .extension = ".mul",
                               .title = "Choose Texture Pair Companion",
                           });
        return false;
    }

    if (result.id == DialogId::fileSelector && result.action == DialogAction::accept &&
        std::holds_alternative<FileBrowserResult>(result.payload))
    {
        cancelFileSelector(state.fileSelector);
        return routeFileBrowserResult(state, std::get<FileBrowserResult>(result.payload));
    }
    return false;
}

bool completeDialogResult(WorkbenchState & state, DialogResult result)
{
    if (!state.dialogHost.active || result.id != state.dialogHost.active->id)
    {
        return false;
    }
    result.generation = state.dialogHost.active->generation;
    if (!state.dialogHost.complete(std::move(result)))
    {
        return false;
    }
    const auto completed = state.dialogHost.consumeResult();
    return completed && handleDialogResult(state, *completed);
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

    if (const auto pendingResult = state.dialogHost.consumeResult())
    {
        profileChanged = handleDialogResult(state, *pendingResult) || profileChanged;
    }

    const bool browserWasVisible = state.fileBrowser.visible;
    std::optional<DialogResult> dialogResult;
    if (!browserWasVisible && !state.fileSelector.visible && state.dialogHost.owns(DialogId::mapWorkflow))
    {
        drawNewFileDialog(state.workspace,
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
        dialogResult = drawFileBrowser(state.fileBrowser);
    }
    if (dialogResult)
    {
        profileChanged = completeDialogResult(state, *dialogResult) || profileChanged;
    }
    if (!state.fileBrowser.visible && browserWasVisible && !dialogResult &&
        state.menuBarState.fileDialog.mode != MapFileDialogMode::none)
    {
        cancelFileDialog(state.menuBarState.fileDialog);
    }
    if (state.dialogHost.owns(DialogId::fileSelector) && !state.fileBrowser.visible && state.fileSelector.visible)
    {
        if (const auto selectorResult = drawFileSelector(state.fileSelector))
        {
            profileChanged = completeDialogResult(state, *selectorResult) || profileChanged;
        }
    }
    return profileChanged;
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
        cancelFileDialog(state.menuBarState.fileDialog);
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
