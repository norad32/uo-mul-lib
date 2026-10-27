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
#include "gui_model/dialogs/file_deduction.h"
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
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

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
    state.deferWorkflowAction(
        [&state]
        {
            try
            {
                persistFileBrowserRecentFolders(state.fileBrowser);
            }
            catch (const std::exception & error)
            {
                state.notifications.add(NotificationSeverity::error,
                                        "Recent folders save failed: " + std::string(error.what()));
            }
        });
}

[[nodiscard]] std::filesystem::path texmapsIndexPath(const std::filesystem::path & path)
{
    const std::string stem = path.stem().string();
    const std::string extension = path.extension().empty() ? ".mul" : path.extension().string();
    if (stem == "texmaps")
    {
        return path.parent_path() / ("texidx" + extension);
    }

    std::string indexStem = stem;
    if (const std::size_t mapsPosition = indexStem.find("maps"); mapsPosition != std::string::npos)
    {
        indexStem.replace(mapsPosition, 4U, "idx");
    }
    else
    {
        indexStem += "idx";
    }
    return path.parent_path() / (indexStem + extension);
}

[[nodiscard]] std::vector<std::filesystem::path> standaloneCreationOutputs(FileBrowserTarget target,
                                                                           const std::filesystem::path & path)
{
    if (target == FileBrowserTarget::createTexmaps)
    {
        return {path, texmapsIndexPath(path)};
    }
    return {path};
}

[[nodiscard]] std::optional<bool>
standaloneCreationCollides(WorkbenchState & state, FileBrowserTarget target, const std::filesystem::path & path)
{
    for (const auto & output : standaloneCreationOutputs(target, path))
    {
        std::error_code filesystemError;
        const bool exists = std::filesystem::exists(output, filesystemError);
        if (filesystemError)
        {
            state.notifications.add(NotificationSeverity::error, output.string() + ": " + filesystemError.message());
            return std::nullopt;
        }
        if (exists)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool openStandaloneAssetImpl(WorkbenchState & state,
                                           DocumentKind kind,
                                           const std::filesystem::path & primaryPath,
                                           const std::filesystem::path & companionPath = {},
                                           DocumentValidationState validation = DocumentValidationState::valid,
                                           std::uint32_t itemCount = 0)
{
    const DocumentSourceIdentity source = companionPath.empty() ? makeDocumentSourceIdentity({primaryPath})
                                                                : makeRoleAwareDocumentSourceIdentity({
                                                                      {.role = SourceRole::index, .path = primaryPath},
                                                                      {.role = SourceRole::data, .path = companionPath},
                                                                  });
    const std::filesystem::path & displayPath =
        kind == DocumentKind::texmaps && !companionPath.empty() ? companionPath : primaryPath;
    const auto summary = state.documentDescriptors.createSummary({
        .kind = kind,
        .source = source,
        .displayName = displayPath.filename().string(),
        .validation = validation,
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
    state.pendingDocumentFocus = id;
    if (auto activated = state.session.activateDocument(id, state.mapDocuments); !activated)
    {
        state.notifications.add(NotificationSeverity::error, activated.error().message);
    }
    state.session.recordRecentFile(primaryPath, kind, companionPath, itemCount);
    return true;
}

bool createDefaultRadarcolImpl(WorkbenchState & state, const std::filesystem::path & path, bool overwriteOutput)
{
    auto created = uomul::RadarColEditor::createEmpty(path, {.overwriteOutput = overwriteOutput});
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
                       const std::array<float, 3> & defaultColor,
                       bool overwriteOutput)
{
    auto created = uomul::RadarColEditor::createEmpty(path, {.overwriteOutput = overwriteOutput});
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
    static_cast<void>(openStandaloneAssetImpl(state, DocumentKind::radarcol, path));
    return true;
}

void createNewAsset(WorkbenchState & state,
                    FileBrowserTarget target,
                    const std::filesystem::path & path,
                    bool useEmbeddedColors,
                    const std::array<float, 3> & defaultColor,
                    bool overwriteOutput = false)
{
    if (target == FileBrowserTarget::createDefaultRadarcol)
    {
        static_cast<void>(createDefaultRadarcolImpl(state, path, overwriteOutput));
        return;
    }
    if (target == FileBrowserTarget::createRadarcol)
    {
        static_cast<void>(createNewRadarcol(state, path, useEmbeddedColors, defaultColor, overwriteOutput));
        return;
    }

    if (target == FileBrowserTarget::createTiledata)
    {
        auto created =
            uomul::TiledataEditor::createEmpty(path, uomul::TiledataLayout::extended64, {.overwriteOutput = overwriteOutput});
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
        static_cast<void>(openStandaloneAssetImpl(state, DocumentKind::tiledata, path));
        return;
    }

    const std::filesystem::path indexPath = texmapsIndexPath(path);
    auto created = uomul::TexMapsEditor::createEmpty(indexPath, path, {.overwriteOutput = overwriteOutput});
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
    static_cast<void>(openStandaloneAssetImpl(state, DocumentKind::texmaps, indexPath, path));
}

bool openGenericAsset(WorkbenchState & state, const FileBrowserResult & result, const FileBrowserAssetCandidate & candidate)
{
    if (candidate.kind == MulAssetKind::map)
    {
        auto mapResult = result;
        mapResult.target = FileBrowserTarget::openMap;
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
    static_cast<void>(
        openStandaloneAssetImpl(state,
                                kind,
                                candidate.path,
                                candidate.companionPath,
                                candidate.validated ? DocumentValidationState::valid : DocumentValidationState::invalid,
                                candidate.itemCount));
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
    if (result.target == FileBrowserTarget::saveRadarcolAs)
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
        case FileBrowserTarget::openImage :
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
        case FileBrowserTarget::chooseImageFolder :
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
        case FileBrowserTarget::exportTexmapFile :
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
        case FileBrowserTarget::exportTexmapFolder :
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
    if (result.target != FileBrowserTarget::chooseTexmapsCompanion)
    {
        return false;
    }
    static_cast<void>(applyFileSelectorCompanion(state.fileSelector, result.path));
    activateFileSelector(state.fileSelector);
    return true;
}

[[nodiscard]] bool routeNewAssetResult(WorkbenchState & state, const FileBrowserResult & result)
{
    if (result.target != FileBrowserTarget::createRadarcol && result.target != FileBrowserTarget::createDefaultRadarcol &&
        result.target != FileBrowserTarget::createTexmaps && result.target != FileBrowserTarget::createTiledata)
    {
        return false;
    }
    const auto target = result.target;
    const auto path = result.path;
    const bool useEmbeddedColors = state.menuBarState.fileDialog.useEmbeddedRadarColors;
    const auto defaultColor = state.menuBarState.fileDialog.radarDefaultColor;
    cancelFileDialog(state.menuBarState.fileDialog);
    const auto collision = standaloneCreationCollides(state, target, path);
    if (!collision)
    {
        return true;
    }
    if (*collision)
    {
        auto & dialog = state.menuBarState.fileDialog;
        dialog.overwriteKind = MapOverwriteKind::standaloneAsset;
        dialog.standaloneOverwritePath = path;
        dialog.standaloneOverwriteDecision = [&state, target, path, useEmbeddedColors, defaultColor](bool overwrite)
        {
            if (overwrite)
            {
                state.deferWorkflowAction([&state, target, path, useEmbeddedColors, defaultColor]
                                          { createNewAsset(state, target, path, useEmbeddedColors, defaultColor, true); });
            }
        };
        requestMapDialog(dialog, MapFileDialogMode::overwrite);
        return true;
    }
    state.deferWorkflowAction([&state, target, path, useEmbeddedColors, defaultColor]
                              { createNewAsset(state, target, path, useEmbeddedColors, defaultColor); });
    return true;
}

[[nodiscard]] bool routeNewMapResult(WorkbenchState & state, const FileBrowserResult & result)
{
    if (result.target != FileBrowserTarget::createMap)
    {
        return false;
    }

    copyToBuffer(state.menuBarState.fileDialog.newMapPath, result.path);
    state.deferWorkflowAction([&state] { startNewMapCreation(state.mapDocuments, state.menuBarState.fileDialog); });
    return true;
}

[[nodiscard]] bool routeUnsupportedGenericResult(WorkbenchState & state, const FileBrowserResult & result)
{
    if (result.target != FileBrowserTarget::openGeneric || result.assetCandidate || !result.assetCandidates.empty())
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
    if (result.assetCandidate || result.target != FileBrowserTarget::openGeneric)
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

    const auto candidateToOpen = selectConfidentFileBrowserCandidate(result.assetCandidates);
    if (!result.assetCandidate && result.target == FileBrowserTarget::openGeneric && candidateToOpen)
    {
        const auto & candidate = *candidateToOpen;
        state.deferWorkflowAction([&state, result, candidate]
                                  { static_cast<void>(openGenericAsset(state, result, candidate)); });
        return false;
    }

    if (routeFileSelectorResult(state, result))
    {
        return false;
    }

    if (result.target == FileBrowserTarget::openGeneric && result.assetCandidate)
    {
        const auto candidate = *result.assetCandidate;
        state.deferWorkflowAction([&state, result, candidate]
                                  { static_cast<void>(openGenericAsset(state, result, candidate)); });
        return false;
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
        requestFileBrowser(state.fileBrowser,
                           {
                               .mode = FileBrowserMode::openFile,
                               .target = FileBrowserTarget::chooseTexmapsCompanion,
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
    return createDefaultRadarcolImpl(state, path, false);
}

bool openStandaloneAsset(WorkbenchState & state,
                         DocumentKind kind,
                         const std::filesystem::path & primaryPath,
                         const std::filesystem::path & companionPath,
                         DocumentValidationState validation,
                         std::uint32_t itemCount)
{
    return openStandaloneAssetImpl(state, kind, primaryPath, companionPath, validation, itemCount);
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
                cancelFileDialog(state.menuBarState.fileDialog);
            }
            break;
        case DialogId::fileSelector :
        case DialogId::fileBrowser :
            state.dialogHost.cancel();
            break;
        case DialogId::preferences :
            state.menuBarState.preferencesDialog.visible = false;
            state.menuBarState.preferencesDialog.closeRequested = true;
            break;
        case DialogId::none :
            return false;
    }
    return true;
}

} // namespace uo_mule::gui
