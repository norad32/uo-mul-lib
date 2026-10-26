#include "gui/workflows/file_workflow.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/dialogs/file_selector_dialog.h"
#include "gui/dialogs/map_workflow_dialog.h"
#include "gui/dialogs/preferences_dialog.h"
#include "gui/presentation/bounded_buffer.h"
#include "gui/presentation/strings.h"
#include "gui/workbench/notification_state.h"
#include "gui/workbench/workbench_state.h"
#include "gui_model/dialogs/dialog_contracts.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/documents/radarcol_document.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/providers/embedded_radarcol.h"
#include "gui_model/session/workbench_session.h"
#include "gui_model/source/source_identity.h"
#include "uomul/radarcol/radarcol.h"
#include "uomul/texmaps/texmaps.h"
#include "uomul/tiledata/tiledata.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace uo_mule::gui
{
namespace
{

void returnToFileBrowser(WorkbenchState & state, const FileBrowserResult & result)
{
    const std::filesystem::path initialPath = result.path.empty() ? std::filesystem::path{} : result.path.parent_path();
    cancelFileSelector(state.fileSelector);
    cancelFileDialog(state.menuBarState.fileDialog);
    requestFileBrowser(state.fileBrowser,
                       {
                           .mode = FileBrowserMode::openFile,
                           .target = result.target,
                           .role = result.role,
                           .initialPath = initialPath,
                           .extension = ".mul",
                           .title = strings::openMulFile(),
                       });
}

void queueFileBrowserPersistence(WorkbenchState & state)
{
    if (!state.fileBrowser.recentFoldersDirty)
    {
        return;
    }
    state.deferWorkflowAction([&state] { persistFileBrowserRecentFolders(state.fileBrowser); });
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
    if (auto activated = state.session.activateDocument(id, state.mapDocuments); !activated)
    {
        state.notifications.add(NotificationSeverity::error, activated.error().message);
    }
}

bool createDefaultRadarcolImpl(WorkbenchState & state, const std::filesystem::path & path)
{
    auto created = uomul::RadarColEditor::createEmpty(path);
    if (!created)
    {
        state.notifications.add(NotificationSeverity::error, strings::formatError(created.error()));
        return false;
    }
    if (auto committed = created->commit(); !committed)
    {
        state.notifications.add(NotificationSeverity::error, strings::formatError(committed.error()));
        return false;
    }

    const auto sourceRoot = state.activeProfile.sourceRoot;
    const auto relativePath = sourceRoot.empty() ? std::filesystem::path{} : path.lexically_relative(sourceRoot);
    state.activeProfile.files[DataFileRole::radarcol] = {
        .path = relativePath.empty() || relativePath.native().starts_with("..") ? path : relativePath,
        .userOverride = true,
    };
    state.session.updateProfile(state.activeProfile);
    state.providers.sync(state.activeProfile);
    state.assetStateRestored = false;
    if (!state.activeProfile.name.empty())
    {
        state.menuBarState.preferencesDialog.workingProfile = state.activeProfile;
        state.menuBarState.preferencesDialog.profileSaveRequested = true;
    }
    state.notifications.add(NotificationSeverity::success, strings::defaultRadarcolCreated());
    return true;
}

bool createNewRadarcol(WorkbenchState & state,
                       const std::filesystem::path & path,
                       bool useEmbeddedColors,
                       const std::array<float, 3> & defaultColor)
{
    auto created = uomul::RadarColEditor::createEmpty(path);
    if (!created)
    {
        state.notifications.add(NotificationSeverity::error, strings::formatError(created.error()));
        return false;
    }

    constexpr float colorScale = 255.0F;
    const auto channel = [](float value) { return static_cast<std::uint8_t>(std::clamp(value, 0.0F, 1.0F) * colorScale); };
    const uomul::RadarColor fallback = uomul::RadarColorConverter::fromRgb888(
        channel(defaultColor.at(0)), channel(defaultColor.at(1)), channel(defaultColor.at(2)));
    for (std::uint32_t index = 0; index < uomul::radarColorCount; ++index)
    {
        const uomul::RadarColor value = useEmbeddedColors ? embeddedRadarcolColor(index).value_or(fallback) : fallback;
        if (auto edited = created->editRawColor(index, value); !edited)
        {
            state.notifications.add(NotificationSeverity::error, strings::formatError(edited.error()));
            return false;
        }
    }
    if (auto committed = created->commit(); !committed)
    {
        state.notifications.add(NotificationSeverity::error, strings::formatError(committed.error()));
        return false;
    }
    openCreatedAsset(state, DocumentKind::radarcol, path);
    return true;
}

void createNewAsset(WorkbenchState & state,
                    FileBrowserTarget target,
                    const std::filesystem::path & path,
                    bool useEmbeddedColors,
                    const std::array<float, 3> & defaultColor)
{
    if (target == FileBrowserTarget::defaultRadarcolOutput)
    {
        static_cast<void>(createDefaultRadarcolImpl(state, path));
        return;
    }
    if (target == FileBrowserTarget::newRadarcolOutput)
    {
        static_cast<void>(createNewRadarcol(state, path, useEmbeddedColors, defaultColor));
        return;
    }

    if (target == FileBrowserTarget::newTiledataOutput)
    {
        auto created = uomul::TiledataEditor::createEmpty(path, uomul::TiledataLayout::extended64);
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
        openCreatedAsset(state, DocumentKind::tiledata, path);
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
    openCreatedAsset(state, DocumentKind::texmaps, indexPath, path);
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

    DocumentKind kind = DocumentKind::texmaps;
    if (candidate.kind == MulAssetKind::radarcol)
    {
        kind = DocumentKind::radarcol;
    }
    else if (candidate.kind == MulAssetKind::tiledata)
    {
        kind = DocumentKind::tiledata;
    }
    const DocumentSourceIdentity source = candidate.kind == MulAssetKind::radarcol || candidate.kind == MulAssetKind::tiledata
                                              ? makeDocumentSourceIdentity({candidate.path})
                                              : makeRoleAwareDocumentSourceIdentity({
                                                    {.role = SourceRole::index, .path = candidate.path},
                                                    {.role = SourceRole::data, .path = candidate.companionPath},
                                                });
    const auto summary = state.documentDescriptors.createSummary({
        .kind = kind,
        .source = source,
        .displayName = candidate.path.filename().string(),
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

    const auto activated = state.session.activateDocument(id, state.mapDocuments);
    if (!activated)
    {
        state.notifications.add(NotificationSeverity::error, activated.error().message);
    }
    state.session.recordRecentFile(candidate.path, kind, candidate.companionPath, candidate.itemCount);
    return false;
}

bool routeSetResult(WorkbenchState & state, const FileBrowserResult & result, const FileBrowserAssetCandidate & set)
{
    auto indexResult = result;
    indexResult.target = FileBrowserTarget::texmapsIndex;
    indexResult.path = set.path;
    auto dataResult = result;
    dataResult.target = FileBrowserTarget::texmapsData;
    dataResult.path = set.companionPath;
    applyFileBrowserResult(state.menuBarState.preferencesDialog, indexResult);
    applyFileBrowserResult(state.menuBarState.preferencesDialog, dataResult);
    if (state.menuBarState.preferencesDialog.visible)
    {
        activatePreferencesDialog(state.menuBarState.preferencesDialog);
    }
    return false;
}

void applyAssetResult(WorkbenchState & state, const FileBrowserResult & result)
{
    applyFileBrowserResult(state.menuBarState.fileDialog, result);
    applyFileBrowserResult(state.menuBarState.preferencesDialog, result);
    if (state.menuBarState.fileDialog.mode != MapFileDialogMode::none)
    {
        requestMapDialog(state.menuBarState.fileDialog, state.menuBarState.fileDialog.mode);
    }
    else if (state.menuBarState.preferencesDialog.visible)
    {
        activatePreferencesDialog(state.menuBarState.preferencesDialog);
    }
    if (result.target == FileBrowserTarget::radarcolOutput)
    {
        if (auto * active = state.session.activeDocumentRecord(); active != nullptr && active->radarcol)
        {
            active->radarcol->requestIntent({
                .kind = RadarcolDocumentIntentKind::saveAs,
                .colorEdits = {},
                .outputPath = result.path,
                .overwriteOutput = false,
            });
        }
    }
}

[[nodiscard]] bool routeImageOrExportResult(WorkbenchState & state, const FileBrowserResult & result)
{
    switch (result.target)
    {
        case FileBrowserTarget::imageOpen :
            if (state.texmapsState.replacementFilePicked)
            {
                const auto path = result.path;
                state.deferWorkflowAction(
                    [&state, path]
                    {
                        if (state.texmapsState.replacementFilePicked)
                        {
                            state.texmapsState.replacementFilePicked(path);
                        }
                    });
            }
            return true;
        case FileBrowserTarget::imageFolderOpen :
            if (state.texmapsState.folderReplacementFolderPicked)
            {
                const auto path = result.path;
                state.deferWorkflowAction(
                    [&state, path]
                    {
                        if (state.texmapsState.folderReplacementFolderPicked)
                        {
                            state.texmapsState.folderReplacementFolderPicked(path);
                        }
                    });
            }
            return true;
        case FileBrowserTarget::texmapExportFile :
            if (state.texmapsState.exportFilePicked)
            {
                const auto path = result.path;
                state.deferWorkflowAction(
                    [&state, path]
                    {
                        if (state.texmapsState.exportFilePicked)
                        {
                            state.texmapsState.exportFilePicked(path);
                        }
                    });
            }
            return true;
        case FileBrowserTarget::texmapExportFolder :
            if (state.texmapsState.exportFolderPicked)
            {
                const auto path = result.path;
                state.deferWorkflowAction(
                    [&state, path]
                    {
                        if (state.texmapsState.exportFolderPicked)
                        {
                            state.texmapsState.exportFolderPicked(path);
                        }
                    });
            }
            return true;
        default :
            return false;
    }
}

[[nodiscard]] bool routeCompanionResult(WorkbenchState & state, const FileBrowserResult & result)
{
    if (result.target != FileBrowserTarget::texmapsCompanion)
    {
        return false;
    }
    if (applyFileSelectorCompanion(state.fileSelector, result.path))
    {
        activateFileSelector(state.fileSelector);
    }
    return true;
}

[[nodiscard]] bool routeNewAssetResult(WorkbenchState & state, const FileBrowserResult & result)
{
    if (result.target != FileBrowserTarget::newRadarcolOutput && result.target != FileBrowserTarget::defaultRadarcolOutput &&
        result.target != FileBrowserTarget::newTexmapsData && result.target != FileBrowserTarget::newTiledataOutput)
    {
        return false;
    }
    const auto target = result.target;
    const auto path = result.path;
    const bool useEmbeddedColors = state.menuBarState.fileDialog.useEmbeddedRadarColors;
    const auto defaultColor = state.menuBarState.fileDialog.radarDefaultColor;
    cancelFileDialog(state.menuBarState.fileDialog);
    state.deferWorkflowAction([&state, target, path, useEmbeddedColors, defaultColor]
                              { createNewAsset(state, target, path, useEmbeddedColors, defaultColor); });
    return true;
}

[[nodiscard]] bool routeNewMapResult(WorkbenchState & state, const FileBrowserResult & result)
{
    if (result.target != FileBrowserTarget::newMapOutput)
    {
        return false;
    }

    copyToBuffer(state.menuBarState.fileDialog.newMapPath, result.path);
    state.deferWorkflowAction([&state] { startNewMapCreation(state.mapDocuments, state.menuBarState.fileDialog); });
    return true;
}

[[nodiscard]] bool routeUnsupportedGenericResult(WorkbenchState & state, const FileBrowserResult & result)
{
    if (result.target != FileBrowserTarget::genericOpen || result.assetCandidate || !result.assetCandidates.empty())
    {
        return false;
    }
    state.notifications.add(NotificationSeverity::error,
                            result.path.filename().string() + ": no supported MUL format was detected");
    if (state.menuBarState.fileDialog.mode != MapFileDialogMode::none)
    {
        requestMapDialog(state.menuBarState.fileDialog, state.menuBarState.fileDialog.mode);
    }
    return true;
}

[[nodiscard]] bool routeFileSelectorResult(WorkbenchState & state, const FileBrowserResult & result)
{
    if (result.assetCandidate ||
        (result.target != FileBrowserTarget::genericOpen && result.target != FileBrowserTarget::radarcolSource &&
         result.target != FileBrowserTarget::texmapsIndex && result.target != FileBrowserTarget::texmapsData))
    {
        return false;
    }
    openFileSelector(state.fileSelector, result);
    return true;
}

[[nodiscard]] bool routeFileBrowserResult(WorkbenchState & state, const FileBrowserResult & result)
{
    if (routeImageOrExportResult(state, result) || routeCompanionResult(state, result) || routeNewAssetResult(state, result) ||
        routeNewMapResult(state, result) || routeUnsupportedGenericResult(state, result))
    {
        return false;
    }

    if (!result.assetCandidate && result.target == FileBrowserTarget::genericOpen && result.assetCandidates.size() == 1U &&
        result.assetCandidates.front().openable)
    {
        const auto candidate = result.assetCandidates.front();
        state.deferWorkflowAction([&state, result, candidate]
                                  { static_cast<void>(openGenericAsset(state, result, candidate)); });
        return false;
    }

    if (routeFileSelectorResult(state, result))
    {
        return false;
    }

    if (result.target == FileBrowserTarget::genericOpen && result.assetCandidate)
    {
        const auto candidate = *result.assetCandidate;
        state.deferWorkflowAction([&state, result, candidate]
                                  { static_cast<void>(openGenericAsset(state, result, candidate)); });
        return false;
    }

    if (result.assetCandidate && result.assetCandidate->kind == MulAssetKind::texmapsSet)
    {
        return routeSetResult(state, result, *result.assetCandidate);
    }

    state.deferWorkflowAction([&state, result] { applyAssetResult(state, result); });
    return false;
}

bool handleDialogResult(WorkbenchState & state, DialogResult result)
{
    if (result.action == DialogAction::cancel)
    {
        if (result.id == DialogId::fileSelector)
        {
            const FileBrowserResult origin = state.fileSelector.browserResult;
            returnToFileBrowser(state, origin);
        }
        else if (result.id == DialogId::fileBrowser)
        {
            const bool preferencesVisible = state.menuBarState.preferencesDialog.visible;
            cancelFileBrowser(state.fileBrowser);
            if (state.menuBarState.fileDialog.mode != MapFileDialogMode::none)
            {
                cancelFileDialog(state.menuBarState.fileDialog);
            }
            else if (preferencesVisible)
            {
                activatePreferencesDialog(state.menuBarState.preferencesDialog);
            }
        }
        else if (result.id == DialogId::mapWorkflow)
        {
            cancelFileDialog(state.menuBarState.fileDialog);
        }
        else if (result.id == DialogId::preferences)
        {
            state.menuBarState.preferencesDialog.visible = false;
            state.menuBarState.preferencesDialog.closeRequested = true;
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
                               .title = "Choose Texmaps Companion",
                           });
        return false;
    }

    if (result.id == DialogId::fileSelector && result.action == DialogAction::accept &&
        std::holds_alternative<FileBrowserResult>(result.payload))
    {
        cancelFileSelector(state.fileSelector);
        return routeFileBrowserResult(state, std::get<FileBrowserResult>(result.payload));
    }

    if (result.id == DialogId::fileBrowser && result.action == DialogAction::accept &&
        std::holds_alternative<FileBrowserResult>(result.payload))
    {
        return routeFileBrowserResult(state, std::get<FileBrowserResult>(result.payload));
    }
    return false;
}

bool completeDialogResultImpl(WorkbenchState & state, DialogResult result)
{
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
    if (const auto pendingResult = state.dialogHost.consumeResult())
    {
        profileChanged = handleDialogResult(state, *pendingResult) || profileChanged;
    }

    std::optional<DialogResult> dialogResult;
    if (state.dialogHost.owns(DialogId::mapWorkflow))
    {
        state.menuBarState.fileDialog.deferWorkflow = [&state](std::function<void()> action)
        { state.deferWorkflowAction(std::move(action)); };
        drawNewFileDialog(state.mapDocuments,
                          state.menuBarState.fileDialog,
                          state.operationController,
                          state.activeProfile.outputCollisionPolicy,
                          state.notifications,
                          state.fileBrowser,
                          state.preferences.idFormat);
    }
    if (state.dialogHost.owns(DialogId::preferences))
    {
        if (const auto result = drawPreferencesDialog(state.menuBarState.preferencesDialog, state.fileBrowser))
        {
            profileChanged = result->applyProfile;
            state.pendingPreferencesResult = *result;
        }
    }
    if (state.dialogHost.owns(DialogId::fileBrowser) && state.dialogHost.active)
    {
        dialogResult = drawFileBrowser(state.fileBrowser, state.dialogHost.active->generation);
    }
    if (dialogResult)
    {
        profileChanged = completeDialogResultImpl(state, *dialogResult) || profileChanged;
    }
    if (state.dialogHost.owns(DialogId::fileSelector) && state.dialogHost.active)
    {
        if (const auto selectorResult = drawFileSelector(state.fileSelector, state.dialogHost.active->generation))
        {
            profileChanged = completeDialogResultImpl(state, *selectorResult) || profileChanged;
        }
    }
    queueFileBrowserPersistence(state);
    return profileChanged;
}

} // namespace

bool completeDialogResult(WorkbenchState & state, DialogResult result)
{
    return completeDialogResultImpl(state, std::move(result));
}

bool createDefaultRadarcol(WorkbenchState & state, const std::filesystem::path & path)
{
    return createDefaultRadarcolImpl(state, path);
}

bool drawFileWorkflow(WorkbenchState & state)
{
    return drawFileDialogs(state);
}

bool handleFileWorkflowEscape(WorkbenchState & state)
{
    if (state.fileBrowser.createFolderVisible)
    {
        state.fileBrowser.createFolderVisible = false;
        state.fileBrowser.error.clear();
        return true;
    }

    if (!state.dialogHost.active)
    {
        if (state.fileBrowser.visible)
        {
            cancelFileBrowser(state.fileBrowser);
            if (state.menuBarState.fileDialog.mode != MapFileDialogMode::none)
            {
                cancelFileDialog(state.menuBarState.fileDialog);
            }
            return true;
        }
        if (state.fileSelector.visible)
        {
            const FileBrowserResult origin = state.fileSelector.browserResult;
            returnToFileBrowser(state, origin);
            return true;
        }
        if (state.menuBarState.fileDialog.mode != MapFileDialogMode::none)
        {
            if (state.menuBarState.fileDialog.mode == MapFileDialogMode::selectMapFacet)
            {
                returnToFileBrowser(state.menuBarState.fileDialog, state.fileBrowser);
            }
            else
            {
                cancelFileDialog(state.menuBarState.fileDialog);
            }
            return true;
        }
        if (state.menuBarState.preferencesDialog.visible)
        {
            state.menuBarState.preferencesDialog.visible = false;
            state.menuBarState.preferencesDialog.closeRequested = true;
            return true;
        }
        return false;
    }

    switch (state.dialogHost.active->id)
    {
        case DialogId::mapWorkflow :
            if (state.menuBarState.fileDialog.mode == MapFileDialogMode::selectMapFacet)
            {
                returnToFileBrowser(state.menuBarState.fileDialog, state.fileBrowser);
            }
            else
            {
                state.dialogHost.cancel();
            }
            break;
        case DialogId::fileSelector :
        case DialogId::fileBrowser :
            state.dialogHost.cancel();
            break;
        case DialogId::preferences :
            state.menuBarState.preferencesDialog.visible = false;
            state.menuBarState.preferencesDialog.closeRequested = true;
            state.dialogHost.cancel();
            break;
        case DialogId::none :
            return false;
    }
    return true;
}

} // namespace uo_mule::gui
