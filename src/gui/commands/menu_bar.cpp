#include "gui/commands/menu_bar.h"
#include "gui/dialogs/preferences_dialog.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/dialogs/file_selector_dialog.h"
#include "gui/documents/document_identity.h"
#include "gui/metadata/document_contracts.h"
#include "gui/context/notification_state.h"
#include "gui/profile/data_profile.h"
#include "gui/views/radarcol_panel.h"
#include "gui/views/texmaps_panel.h"
#include "gui/commands/command_registry.h"
#include "gui/documents/map_workspace.h"
#include "gui/dialogs/new_map_dialog.h"
#include "gui/application/workbench_state.h"
#include "gui/profile/workbench_session.h"
#include "gui/views/panel_registry.h"
#include <imgui.h>
#include <string>
#include <utility>

namespace uo_mule::gui
{
namespace
{

void drawCommandItem(WorkbenchState & state, CommandId id, bool selected = false)
{
    const Command * command = state.commands.find(id);
    if (command == nullptr)
    {
        return;
    }
    const CommandAvailability commandState = state.commands.availability(id, state);
    const std::string shortcut = command->defaultShortcut;
    const bool activated = ImGui::MenuItem(command->label.c_str(),
                                           shortcut.empty() ? nullptr : shortcut.c_str(),
                                           selected,
                                           commandState.enabled);
    if (activated)
    {
        static_cast<void>(state.commands.execute(id, state));
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
    return false;
}

[[nodiscard]] bool applyFileBrowserResultToWorkbench(WorkbenchState & state, const FileBrowserResult & result)
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
    applyFileBrowserResult(state.radarcolState, result);
    applyFileBrowserResult(state.texmapsState, result);
    return false;
}

void drawFileMenu(WorkbenchState & state)
{
    if (!ImGui::BeginMenu("File"))
    {
        return;
    }

    drawCommandItem(state, CommandId::fileOpen);
    drawCommandItem(state, CommandId::fileNew);
    ImGui::Separator();
    drawCommandItem(state, CommandId::fileSaveAs);
    drawCommandItem(state, CommandId::fileDiscard);
    ImGui::Separator();
    drawCommandItem(state, CommandId::fileClose);
    drawCommandItem(state, CommandId::fileQuit);

    ImGui::EndMenu();
}

void drawViewMenu(WorkbenchState & state)
{
    if (!ImGui::BeginMenu("View"))
    {
        return;
    }

    const MapDisplayMode displayMode = state.workspace.displayMode();
    drawCommandItem(state, CommandId::viewTileIds, displayMode == MapDisplayMode::tileIds);
    drawCommandItem(state, CommandId::viewZ, displayMode == MapDisplayMode::z);
    drawCommandItem(state, CommandId::viewCombined, displayMode == MapDisplayMode::combined);

    ImGui::Separator();
    drawCommandItem(state, CommandId::panelTileInspector, state.panelRegistry.find(PanelId::tileInspector)->visible);
    drawCommandItem(state, CommandId::panelBlockInspector, state.panelRegistry.find(PanelId::blockInspector)->visible);
    drawCommandItem(state, CommandId::panelSelectionStatistics, state.panelRegistry.find(PanelId::selectionStatistics)->visible);
    drawCommandItem(state, CommandId::panelMapInformation, state.panelRegistry.find(PanelId::mapInformation)->visible);
    drawCommandItem(state, CommandId::panelEditTools, state.panelRegistry.find(PanelId::editTools)->visible);
    drawCommandItem(state, CommandId::panelRadarcol, state.panelRegistry.find(PanelId::radarcol)->visible);
    drawCommandItem(state, CommandId::panelTexmaps, state.panelRegistry.find(PanelId::texmaps)->visible);
    drawCommandItem(state, CommandId::viewToggleLogPanel, state.preferences.showLogPanel);
    drawCommandItem(state, CommandId::resetLayout);
    ImGui::EndMenu();
}

void drawSelectionMenu(WorkbenchState & state)
{
    if (!ImGui::BeginMenu("Selection"))
    {
        return;
    }

    const MapSelectionMode selectionMode = state.workspace.selectionMode();
    drawCommandItem(state, CommandId::selectionTiles, selectionMode == MapSelectionMode::tiles);
    drawCommandItem(state, CommandId::selectionBlocks, selectionMode == MapSelectionMode::blocks);

    ImGui::EndMenu();
}

void drawEditMenu(WorkbenchState & state)
{
    if (!ImGui::BeginMenu("Edit"))
    {
        return;
    }

    const bool editMode = state.workspace.editMode();
    const Command * command = state.commands.find(CommandId::editToggle);
    const CommandAvailability commandState = state.commands.availability(CommandId::editToggle, state);
    const std::string label = editMode ? "Edit mode (ON)" : "Edit mode (OFF)";
    if (command != nullptr && ImGui::MenuItem(label.c_str(), nullptr, editMode, commandState.enabled))
    {
        static_cast<void>(state.commands.execute(CommandId::editToggle, state));
    }

    ImGui::EndMenu();
}

void drawMapMenu(WorkbenchState & state)
{
    if (!ImGui::BeginMenu("Map"))
    {
        return;
    }
    drawCommandItem(state, CommandId::mapFit);
    drawCommandItem(state, CommandId::mapFitSelection);
    drawCommandItem(state, CommandId::mapCenterSelection);
    drawCommandItem(state, CommandId::mapResetView);
    drawCommandItem(state, CommandId::mapGoToCoordinate);
    drawCommandItem(state, CommandId::mapGoToBlockCoordinate);
    drawCommandItem(state, CommandId::mapCopyCoordinate);
    drawCommandItem(state, CommandId::mapCopyTileData);
    ImGui::EndMenu();
}

void handleEscape(WorkbenchState & state, bool showMainMenu)
{
    if (!ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        return;
    }
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
    else if (!showMainMenu && state.session.startupState() == StartupState::mapWelcome)
    {
        state.session.setStartupState(StartupState::workbench);
    }
}

bool handleFileSelector(WorkbenchState & state)
{
    if (state.fileBrowser.visible || !state.fileSelector.visible)
    {
        return false;
    }

    const auto action = drawFileSelector(state.fileSelector);
    if (action == FileSelectorAction::accept)
    {
        const auto selected = selectedFileSelectorResult(state.fileSelector);
        cancelFileSelector(state.fileSelector);
        return applyFileBrowserResultToWorkbench(state, selected);
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
    if (state.menuBarState.fileDialog.shouldClose)
    {
        state.shouldClose = true;
        state.menuBarState.fileDialog.shouldClose = false;
    }

    const bool browserWasVisible = state.fileBrowser.visible;
    if (!browserWasVisible && !state.fileSelector.visible)
    {
        drawNewMapDialog(state.workspace,
                         state.menuBarState.fileDialog,
                         state.operationController,
                         state.activeProfile.outputCollisionPolicy,
                         state.notifications,
                         state.fileBrowser,
                         state.shouldClose);
        profileChanged = drawPreferencesDialog(
            state.menuBarState.preferencesDialog, state.activeProfile, state.preferences, state.fileBrowser);
    }
    drawFileBrowser(state.fileBrowser);
    const auto result = consumeFileBrowserResult(state.fileBrowser);
    if (!state.fileBrowser.visible && browserWasVisible && !result &&
        state.menuBarState.fileDialog.mode != MapFileDialogMode::none)
    {
        cancelMapDialog(state.menuBarState.fileDialog);
    }
    if (result)
    {
        profileChanged = applyFileBrowserResultToWorkbench(state, *result) || profileChanged;
    }
    return handleFileSelector(state) || profileChanged;
}

} // namespace

bool drawMenuBar(WorkbenchState & state, bool showMainMenu, bool drawDialogs)
{
    if (state.menuBarState.windowCloseRequested && state.menuBarState.fileDialog.mode == MapFileDialogMode::none)
    {
        state.menuBarState.windowCloseRequested = false;
        static_cast<void>(state.commands.execute(CommandId::fileQuit, state));
    }

    if (showMainMenu && ImGui::BeginMainMenuBar())
    {
        drawFileMenu(state);
        drawViewMenu(state);
        drawSelectionMenu(state);
        drawEditMenu(state);
        drawMapMenu(state);

        if (ImGui::BeginMenu("Data"))
        {
            drawCommandItem(state, CommandId::dataPaths);

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    handleEscape(state, showMainMenu);

    if (!drawDialogs)
    {
        return false;
    }

    return drawFileDialogs(state);
}

void requestWindowClose(WorkbenchState & state)
{
    state.menuBarState.windowCloseRequested = true;
}

} // namespace uo_mule::gui
