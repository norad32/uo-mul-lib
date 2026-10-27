#include "gui/dialogs/map_workflow_dialog.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/dialogs/map_dimension_fields.h"
#include "gui/dialogs/map_facet_dialog.h"
#include "gui/presentation/bounded_buffer.h"
#include "gui/presentation/dialog_presentation.h"
#include "gui/presentation/iconography.h"
#include "gui/presentation/strings.h"
#include "gui/presentation/theme.h"
#include "gui/workbench/notification_state.h"
#include "gui_model/dialogs/dialog_contracts.h"
#include "gui_model/dialogs/dialog_host.h"
#include "gui_model/documents/map_document_manager.h"
#include "gui_model/documents/map_document_state.h"
#include "gui_model/operations/operation_controller.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/profile/data_profile.h"
#include "uomul/facet_sizes.h"
#include "uomul/map/map.h"
#include "uomul/mul_common/mul_common.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <imgui.h>
#include <limits>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace uo_mule::gui
{

namespace
{

constexpr int maximumTileId = std::numeric_limits<std::uint16_t>::max();
constexpr int minimumTileZ = -std::numeric_limits<std::int8_t>::max() - 1;
constexpr int maximumTileZ = std::numeric_limits<std::int8_t>::max();
constexpr float dimensionFieldWidth = 200.0F;
constexpr float newFileTypeFieldWidth = 220.0F;
constexpr ImGuiWindowFlags hostFlags = static_cast<ImGuiWindowFlags>(
    static_cast<unsigned int>(ImGuiWindowFlags_NoDecoration) | static_cast<unsigned int>(ImGuiWindowFlags_NoBackground) |
    static_cast<unsigned int>(ImGuiWindowFlags_NoSavedSettings) | static_cast<unsigned int>(ImGuiWindowFlags_NoInputs) |
    static_cast<unsigned int>(ImGuiWindowFlags_NoNav) | static_cast<unsigned int>(ImGuiWindowFlags_NoFocusOnAppearing) |
    static_cast<unsigned int>(ImGuiWindowFlags_NoBringToFrontOnFocus));
constexpr float dialogCenterPivot = 0.5F;

void closeDialogPopup(NewFileDialogState & state)
{
    state.activePopup = MapFileDialogMode::none;
    if (state.rendering)
    {
        ImGui::CloseCurrentPopup();
    }
}

void deferWorkflow(const NewFileDialogState & state, std::function<void()> action)
{
    if (state.deferWorkflow)
    {
        state.deferWorkflow(std::move(action));
        return;
    }
    action();
}

[[nodiscard]] const char * popupName(MapFileDialogMode mode) noexcept
{
    switch (mode)
    {
        case MapFileDialogMode::openMap :
            return "Open File###dialog.map-open";
        case MapFileDialogMode::selectMapFacet :
            return "Select Facet###dialog.map-facet";
        case MapFileDialogMode::newType :
            return "Create new mul file###dialog.new-file-type";
        case MapFileDialogMode::saveAs :
            return "Save As / Commit###dialog.map-save";
        case MapFileDialogMode::dirtyChanges :
            return "Pending Changes###dialog.map-pending-changes";
        case MapFileDialogMode::overwrite :
            return "Confirm Overwrite###dialog.map-overwrite";
        case MapFileDialogMode::error :
            return "Map Operation Failed###dialog.map-error";
        case MapFileDialogMode::none :
            break;
    }
    return "###dialog.map-none";
}

void closeDialog(NewFileDialogState & state)
{
    if (state.dialogHost != nullptr && state.dialogHost->owns(DialogId::mapWorkflow))
    {
        state.dialogHost->cancel();
    }
    closeDialogPopup(state);
    state.operation = {};
    state.overwriteKind = MapOverwriteKind::none;
    state.saveForPendingOperation = false;
    state.standaloneOverwritePath.clear();
    state.standaloneOverwriteDecision = {};
    state.mode = MapFileDialogMode::none;
    state.activePopup = MapFileDialogMode::none;
    state.openWithoutReview = false;
    state.openCandidates.clear();
    state.error.clear();
}

void showError(NewFileDialogState & state, const uomul::Error & error)
{
    state.error = strings::formatError(error);
    if (state.notifications != nullptr)
    {
        state.notifications->add(NotificationSeverity::error, state.error);
    }
    closeDialogPopup(state);
    state.mode = MapFileDialogMode::error;
}

[[nodiscard]] bool outputExists(const std::filesystem::path & path, NewFileDialogState & state)
{
    std::error_code filesystemError;
    const bool exists = std::filesystem::exists(path, filesystemError);

    if (filesystemError)
    {
        state.error = path.string() + ": " + filesystemError.message();
        closeDialogPopup(state);
        state.mode = MapFileDialogMode::error;
        return false;
    }

    return exists;
}

void showOverwrite(NewFileDialogState & state, MapOverwriteKind kind)
{
    closeDialogPopup(state);
    state.overwriteKind = kind;

    if (kind == MapOverwriteKind::newMap)
    {
        state.operationOverwriteOutput = true;
    }

    state.mode = MapFileDialogMode::overwrite;
}

void executeOperation(MapDocumentManager & mapDocument,
                      NewFileDialogState & state,
                      PendingChangesAction action,
                      const std::optional<std::filesystem::path> & savePath,
                      bool overwrite)
{
    std::expected<void, uomul::Error> result;

    switch (state.operation.kind)
    {
        case PendingMapOperationKind::openMap :
            result = mapDocument.openMap(state.operation.path, state.operation.dimensions, action, savePath, overwrite);
            break;
        case PendingMapOperationKind::newMap :
            result = mapDocument.createEmptyMap(
                state.operation.path, state.operation.dimensions, action, savePath, overwrite, state.operation.initialTile);
            break;
        case PendingMapOperationKind::none :
            break;
    }

    if (!result)
    {
        showError(state, result.error());
        return;
    }

    state.operation = {};
    state.overwriteKind = MapOverwriteKind::none;
    state.saveForPendingOperation = false;
    state.standaloneOverwritePath.clear();
    state.standaloneOverwriteDecision = {};
    closeDialogPopup(state);
    state.mode = MapFileDialogMode::none;
    state.activePopup = MapFileDialogMode::none;
}

void continueOperation(MapDocumentManager & mapDocument, NewFileDialogState & state, bool overwrite)
{
    state.operationOverwriteOutput = overwrite;

    if (mapDocument.active().changes().dirty())
    {
        closeDialogPopup(state);
        state.mode = MapFileDialogMode::dirtyChanges;
        return;
    }

    executeOperation(mapDocument, state, PendingChangesAction::cancel, std::nullopt, overwrite);
}

void cancelControlledOperation(const NewFileDialogState & state)
{
    if (state.controller != nullptr)
    {
        static_cast<void>(state.controller->cancel());
    }
}

void discardMapChanges(MapDocumentManager & mapDocument, NewFileDialogState & state)
{
    if (state.controller != nullptr)
    {
        static_cast<void>(state.controller->resolveDirty(PendingChangesAction::discard));
    }
    executeOperation(mapDocument, state, PendingChangesAction::discard, std::nullopt, state.operationOverwriteOutput);
}

void saveDirtyMap(MapDocumentManager & mapDocument, NewFileDialogState & state)
{
    if (state.controller != nullptr)
    {
        static_cast<void>(state.controller->resolveDirty(PendingChangesAction::save));
    }
    const auto saved = mapDocument.save(mapDocument.active().changes().originalPath, false);
    if (!saved)
    {
        if (state.controller != nullptr)
        {
            static_cast<void>(state.controller->completeSave(false));
        }
        showError(state, saved.error());
        return;
    }
    const auto transition = state.controller == nullptr
                                ? OperationTransition{.kind = OperationTransitionKind::execute, .error = {}}
                                : state.controller->completeSave(true);
    if (transition.kind == OperationTransitionKind::execute)
    {
        continueOperation(mapDocument, state, state.operationOverwriteOutput);
    }
    else
    {
        closeDialog(state);
    }
}

[[nodiscard]] bool beginControlledOperation(NewFileDialogState & state, const MapDocumentManager & mapDocument)
{
    if (state.controller == nullptr)
    {
        return true;
    }

    const OperationKind kind =
        state.operation.kind == PendingMapOperationKind::newMap ? OperationKind::newMap : OperationKind::openMap;
    std::error_code outputError;
    const bool outputExists = kind == OperationKind::newMap && std::filesystem::exists(state.operation.path, outputError);
    const auto transition = state.controller->begin(
        {
            .kind = kind,
            .documentId = std::nullopt,
            .path = state.operation.path,
            .dimensions = state.operation.dimensions,
            .initialTile = state.operation.initialTile,
        },
        mapDocument.active().changes().dirty(),
        outputExists,
        state.collisionPolicy);
    if (transition.kind == OperationTransitionKind::rejected)
    {
        state.error = transition.error;
        if (state.notifications != nullptr)
        {
            state.notifications->add(NotificationSeverity::error, state.error);
        }
        closeDialogPopup(state);
        state.mode = MapFileDialogMode::error;
        return false;
    }
    if (transition.kind == OperationTransitionKind::showDirtyDecision)
    {
        closeDialogPopup(state);
        state.mode = MapFileDialogMode::dirtyChanges;
        return false;
    }
    if (transition.kind == OperationTransitionKind::showOverwriteConfirmation)
    {
        showOverwrite(state, MapOverwriteKind::newMap);
        return false;
    }
    return true;
}

void requestOperation(MapDocumentManager & mapDocument, NewFileDialogState & state)
{
    if (!beginControlledOperation(state, mapDocument))
    {
        return;
    }

    if (state.operation.kind == PendingMapOperationKind::openMap)
    {
        const auto candidate = uomul::MapReader::open(state.operation.path, state.operation.dimensions);
        if (!candidate)
        {
            showError(state, candidate.error());
            return;
        }
    }

    if (state.operation.kind == PendingMapOperationKind::newMap)
    {
        std::error_code outputPathError;
        const std::filesystem::path output = std::filesystem::weakly_canonical(state.operation.path, outputPathError);

        if (outputPathError)
        {
            state.error = state.operation.path.string() + ": " + outputPathError.message();
            closeDialogPopup(state);
            state.mode = MapFileDialogMode::error;
            return;
        }

        if (mapDocument.active().changes().reader.has_value())
        {
            std::error_code sourcePathError;
            const std::filesystem::path source =
                std::filesystem::weakly_canonical(mapDocument.active().changes().mapPath, sourcePathError);
            if (!sourcePathError && output == source)
            {
                state.error = "New map output must not replace the currently open map";
                closeDialogPopup(state);
                state.mode = MapFileDialogMode::error;
                return;
            }
        }

        if (outputExists(state.operation.path, state))
        {
            showOverwrite(state, MapOverwriteKind::newMap);
            return;
        }

        if (state.mode == MapFileDialogMode::error)
        {
            return;
        }
    }

    continueOperation(mapDocument, state, false);
}

[[nodiscard]] bool validInitialTile(int tileId, int tileZ) noexcept
{
    return tileId >= 0 && tileId <= maximumTileId && tileZ >= minimumTileZ && tileZ <= maximumTileZ;
}

[[nodiscard]] bool
drawDimensionsFields(const char * id, int & preset, std::uint32_t & width, std::uint32_t & height, bool useMapLabel = false)
{
    ImGui::PushID(id);
    const bool valid = drawMapDimensionFields(
        preset, width, height, useMapLabel, {.fieldWidth = dimensionFieldWidth, .labelWidth = std::nullopt});
    ImGui::PopID();
    return valid;
}

void applyMapCandidateDimensions(NewFileDialogState & state, const FileBrowserAssetCandidate & candidate)
{
    if (candidate.kind != MulAssetKind::map || !candidate.dimensions)
    {
        return;
    }

    state.openWidth = candidate.dimensions->widthTiles;
    state.openHeight = candidate.dimensions->heightTiles;
    std::size_t index = 0;
    for (const auto & facet : uomul::knownFacetSizes())
    {
        if (facet.widthTiles == state.openWidth && facet.heightTiles == state.openHeight)
        {
            state.openFacet = static_cast<int>(index + 1U);
            return;
        }
        index++;
    }
}

void drawOpenDialog(MapDocumentManager & mapDocument, NewFileDialogState & state)
{
    if (state.openWithoutReview && state.openPath.front() != '\0')
    {
        state.operation = {
            .kind = PendingMapOperationKind::openMap,
            .path = state.openPath.data(),
            .dimensions = uomul::Dimensions{.widthTiles = state.openWidth, .heightTiles = state.openHeight},
        };
        requestOperation(mapDocument, state);
        return;
    }

    dialogFieldLabel("Map path");
    ImGui::TextUnformatted(state.openPath.data());
    if (ImGui::IsItemClicked() && state.browser != nullptr)
    {
        requestFileBrowser(*state.browser,
                           {
                               .mode = FileBrowserMode::openFile,
                               .target = FileBrowserTarget::openMap,
                               .initialPath = state.openPath.data(),
                               .extension = ".mul",
                               .title = strings::openMulFile(),
                           });
    }
    const bool validDimensions = drawDimensionsFields("OpenDimensions", state.openFacet, state.openWidth, state.openHeight);
    if (!validDimensions)
    {
        ImGui::TextColored(theme::imguiColor(theme::warning), "%s", strings::invalidDimensions());
    }
    beginDialogFooter({"Open", "Cancel"});
    if (dialogButton(iconLabel(IconId::folder, "Open").c_str(), state.openPath.front() != '\0' && validDimensions))
    {
        const auto dimensions = resolveMapDimensions(state.openFacet, state.openWidth, state.openHeight);
        if (dimensions)
        {
            state.operation = {
                .kind = PendingMapOperationKind::openMap,
                .path = state.openPath.data(),
                .dimensions = *dimensions,
            };
            deferWorkflow(state, [&mapDocument, &state] { requestOperation(mapDocument, state); });
        }
    }

    ImGui::SameLine();
    if (dialogButton(iconLabel(IconId::cancel, "Cancel").c_str()))
    {
        state.operation = {};
        closeDialog(state);
    }
    endDialogFooter();
}

void startNewMapOperation(MapDocumentManager & mapDocument, NewFileDialogState & state)
{
    const auto dimensions = resolveMapDimensions(state.newMapFacet, state.newMapWidth, state.newMapHeight);
    if (!dimensions)
    {
        state.error = dimensions.error();
        closeDialogPopup(state);
        state.mode = MapFileDialogMode::error;
        return;
    }

    state.operation = {
        .kind = PendingMapOperationKind::newMap,
        .path = state.newMapPath.data(),
        .dimensions = *dimensions,
        .initialTile =
            {
                .id = static_cast<std::uint16_t>(state.initialTileId),
                .z = static_cast<std::int8_t>(state.initialTileZ),
            },
    };
    deferWorkflow(state, [&mapDocument, &state] { requestOperation(mapDocument, state); });
}

void requestNewFileOutput(NewFileDialogState & state)
{
    FileBrowserTarget target = FileBrowserTarget::createMap;
    switch (state.newFileKind)
    {
        case NewFileKind::map :
            target = FileBrowserTarget::createMap;
            break;
        case NewFileKind::radarcol :
            target = FileBrowserTarget::createRadarcol;
            break;
        case NewFileKind::texmaps :
            target = FileBrowserTarget::createTexmaps;
            break;
        case NewFileKind::tiledata :
            target = FileBrowserTarget::createTiledata;
            break;
    }

    if (state.browser != nullptr)
    {
        requestFileBrowser(*state.browser,
                           {
                               .mode = FileBrowserMode::saveFile,
                               .target = target,
                               .initialPath = {},
                               .extension = ".mul",
                               .title = "Create new mul file",
                           });
    }
    state.activePopup = MapFileDialogMode::none;
    closeDialogPopup(state);
}

void drawNewTypeDialog(MapDocumentManager & mapDocument, NewFileDialogState & state)
{
    constexpr std::array<const char *, 4> fileTypeLabels{"Map", "Radarcol", "Texmaps", "Tiledata"};
    int selectedType = static_cast<int>(state.newFileKind);
    dialogFieldLabel("File type");
    dialogFieldWidth(newFileTypeFieldWidth);
    if (ImGui::Combo("##NewFileType", &selectedType, fileTypeLabels.data(), static_cast<int>(fileTypeLabels.size())))
    {
        state.newFileKind = static_cast<NewFileKind>(selectedType);
        state.newMapPath.fill('\0');
    }

    bool valid = true;
    if (state.newFileKind == NewFileKind::map)
    {
        const bool validDimensions =
            drawDimensionsFields("NewDimensions", state.newMapFacet, state.newMapWidth, state.newMapHeight, true);
        if (!validDimensions)
        {
            ImGui::TextColored(theme::imguiColor(theme::warning), "%s", strings::invalidDimensions());
        }

        dialogFieldLabel(iconLabel(IconId::identifier, strings::tileId()).c_str());
        dialogFieldWidth(dimensionFieldWidth);
        const auto idInputFlags =
            state.idFormat == IdFormat::hexadecimal ? ImGuiInputTextFlags_CharsHexadecimal : ImGuiInputTextFlags_CharsDecimal;
        ImGui::InputScalar("##TileId",
                           ImGuiDataType_S32,
                           &state.initialTileId,
                           nullptr,
                           nullptr,
                           state.idFormat == IdFormat::hexadecimal ? "%04X" : "%d",
                           idInputFlags);

        dialogFieldLabel(iconLabel(IconId::axisZArrow, strings::elevation()).c_str());
        dialogFieldWidth(dimensionFieldWidth);
        ImGui::InputScalar("##Elevation", ImGuiDataType_S32, &state.initialTileZ, nullptr, nullptr, "%d");
        const bool validTile = validInitialTile(state.initialTileId, state.initialTileZ);
        if (!validTile)
        {
            ImGui::TextColored(theme::imguiColor(theme::warning), "ID must be 0-65535 and Z must be -128 to 127.");
        }
        valid = validDimensions && validTile;
    }
    else if (state.newFileKind == NewFileKind::radarcol)
    {
        dialogFieldLabel("Default color");
        dialogFieldWidth(dimensionFieldWidth);
        ImGui::ColorEdit3("##RadarcolDefaultColor", state.radarDefaultColor.data(), ImGuiColorEditFlags_NoInputs);
        ImGui::Checkbox("Use embedded colors", &state.useEmbeddedRadarColors);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("The inputted color will not be used when embedded colors are enabled.");
        }
    }

    const std::string okLabel = iconLabel(IconId::ok, strings::ok());
    const std::string cancelLabel = iconLabel(IconId::cancel, strings::cancel());
    beginDialogFooter({okLabel.c_str(), cancelLabel.c_str()}, false);
    if (dialogButton(okLabel.c_str(), valid))
    {
        if (state.newFileKind == NewFileKind::map && state.newMapPath.front() != '\0')
        {
            startNewMapOperation(mapDocument, state);
        }
        else
        {
            requestNewFileOutput(state);
        }
    }
    ImGui::SameLine();
    if (dialogButton(cancelLabel.c_str()))
    {
        closeDialog(state);
    }
    endDialogFooter();
}

[[nodiscard]] bool
finishPendingSave(MapDocumentManager & mapDocument, NewFileDialogState & state, const std::filesystem::path & path)
{
    if (state.controller != nullptr)
    {
        const auto transition = state.controller->completeSave(true);
        if (transition.kind == OperationTransitionKind::rejected)
        {
            showError(state,
                      uomul::Error{
                          .code = uomul::ErrorCode::io,
                          .path = path,
                          .offset = 0,
                          .message = transition.error,
                      });
            return false;
        }
    }

    const bool overwriteOperation = state.operationOverwriteOutput;
    state.overwriteKind = MapOverwriteKind::none;
    continueOperation(mapDocument, state, overwriteOperation);
    closeDialogPopup(state);
    return true;
}

[[nodiscard]] bool
saveAs(MapDocumentManager & mapDocument, NewFileDialogState & state, const std::filesystem::path & path, bool overwrite = false)
{
    const auto saved = mapDocument.save(path, overwrite);
    if (!saved)
    {
        if (state.controller != nullptr && state.controller->phase() == OperationPhase::awaitingSave)
        {
            static_cast<void>(state.controller->completeSave(false));
        }
        showError(state, saved.error());
        return false;
    }
    if (state.saveForPendingOperation && !finishPendingSave(mapDocument, state, path))
    {
        return false;
    }
    if (!state.saveForPendingOperation)
    {
        if (state.controller != nullptr && state.controller->phase() == OperationPhase::awaitingSave)
        {
            const auto transition = state.controller->completeSave(true);
            if (transition.kind != OperationTransitionKind::execute)
            {
                showError(state,
                          uomul::Error{.code = uomul::ErrorCode::io, .path = path, .offset = 0, .message = transition.error});
                return false;
            }
        }
        closeDialog(state);
    }
    return true;
}

void requestSaveAs(MapDocumentManager & mapDocument, NewFileDialogState & state, const std::filesystem::path & path)
{
    std::error_code outputError;
    const bool exists = std::filesystem::exists(path, outputError);
    if (outputError)
    {
        showError(state,
                  uomul::Error{.code = uomul::ErrorCode::io, .path = path, .offset = 0, .message = outputError.message()});
        return;
    }
    if (state.controller != nullptr && state.controller->phase() == OperationPhase::idle)
    {
        OperationRequest request{
            .kind = OperationKind::saveAs,
            .documentId = mapDocument.activeDocumentId(),
            .path = path,
            .dimensions = mapDocument.active().changes().dimensions,
        };
        const auto transition =
            state.controller->begin(std::move(request), mapDocument.active().changes().dirty(), exists, state.collisionPolicy);
        if (transition.kind == OperationTransitionKind::showOverwriteConfirmation)
        {
            showOverwrite(state, MapOverwriteKind::save);
            return;
        }
        if (transition.kind == OperationTransitionKind::showDirtyDecision)
        {
            state.mode = MapFileDialogMode::dirtyChanges;
            state.activePopup = MapFileDialogMode::none;
            return;
        }
        if (transition.kind == OperationTransitionKind::rejected)
        {
            showError(state,
                      uomul::Error{.code = uomul::ErrorCode::io, .path = path, .offset = 0, .message = transition.error});
            return;
        }
    }
    if (state.mode != MapFileDialogMode::error)
    {
        static_cast<void>(saveAs(mapDocument, state, path));
    }
}

void drawSaveAsDialog(MapDocumentManager & mapDocument, NewFileDialogState & state)
{
    dialogFieldLabel("Commit path");
    ImGui::TextUnformatted(state.savePath.data());
    if (ImGui::IsItemClicked() && state.browser != nullptr)
    {
        requestFileBrowser(*state.browser,
                           {
                               .mode = FileBrowserMode::saveFile,
                               .target = FileBrowserTarget::saveMapAs,
                               .initialPath = state.savePath.data(),
                               .extension = ".mul",
                               .title = "Save Map",
                           });
    }

    beginDialogFooter({"Save", "Cancel"});
    if (dialogButton(iconLabel(IconId::save, "Save").c_str(), state.savePath.front() != '\0'))
    {
        const std::filesystem::path path(state.savePath.data());
        deferWorkflow(state, [&mapDocument, &state, path] { requestSaveAs(mapDocument, state, path); });
        closeDialogPopup(state);
    }

    ImGui::SameLine();

    if (dialogButton(iconLabel(IconId::cancel, "Cancel").c_str()))
    {
        state.saveForPendingOperation = false;
        closeDialog(state);
    }
    endDialogFooter();
}

void drawDirtyDialog(MapDocumentManager & mapDocument, NewFileDialogState & state)
{
    ImGui::TextWrapped("The current map has staged changes. Save, save as, discard, or cancel this operation?");

    beginDialogFooter({"Save", "Save As...", "Discard", "Cancel"});
    if (dialogButton(iconLabel(IconId::save, "Save").c_str()))
    {
        deferWorkflow(state, [&mapDocument, &state] { saveDirtyMap(mapDocument, state); });
        closeDialogPopup(state);
    }

    ImGui::SameLine();

    if (dialogButton(iconLabel(IconId::save, "Save As...").c_str()))
    {
        deferWorkflow(state,
                      [&state]
                      {
                          if (state.controller != nullptr)
                          {
                              static_cast<void>(state.controller->resolveDirty(PendingChangesAction::save));
                          }
                      });
        state.saveForPendingOperation = true;
        state.savePath.fill('\0');
        requestFileBrowser(*state.browser,
                           {
                               .mode = FileBrowserMode::saveFile,
                               .target = FileBrowserTarget::saveMapAs,
                               .initialPath = mapDocument.active().changes().originalPath,
                               .extension = ".mul",
                               .title = "Save Map As",
                           });
        state.mode = MapFileDialogMode::saveAs;
        closeDialogPopup(state);
    }

    ImGui::SameLine();

    if (dialogButton(iconLabel(IconId::cancel, "Discard").c_str()))
    {
        deferWorkflow(state, [&mapDocument, &state] { discardMapChanges(mapDocument, state); });
        closeDialogPopup(state);
    }

    ImGui::SameLine();

    if (dialogButton(iconLabel(IconId::cancel, "Cancel").c_str()))
    {
        deferWorkflow(state, [&state] { cancelControlledOperation(state); });
        state.operation = {};
        state.overwriteKind = MapOverwriteKind::none;
        closeDialog(state);
    }
    endDialogFooter();
}

void handleOverwriteSave(MapDocumentManager & mapDocument, NewFileDialogState & state, const std::filesystem::path & path)
{
    if (state.controller != nullptr && state.controller->phase() == OperationPhase::awaitingOverwrite)
    {
        const auto transition = state.controller->resolveOverwrite(true);
        if (transition.kind == OperationTransitionKind::showDirtyDecision)
        {
            state.mode = MapFileDialogMode::dirtyChanges;
            state.activePopup = MapFileDialogMode::none;
            closeDialogPopup(state);
            return;
        }
        if (transition.kind != OperationTransitionKind::showSaveDialog)
        {
            showError(state,
                      uomul::Error{.code = uomul::ErrorCode::io, .path = path, .offset = 0, .message = transition.error});
            return;
        }
    }
    const auto saved = saveAs(mapDocument, state, path, true);
    if (!saved)
    {
        return;
    }
    if (!state.saveForPendingOperation)
    {
        state.mode = MapFileDialogMode::none;
        return;
    }
    const bool overwriteOperation = state.operationOverwriteOutput;
    state.overwriteKind = MapOverwriteKind::none;
    continueOperation(mapDocument, state, overwriteOperation);
}

void handleOverwriteNewMap(MapDocumentManager & mapDocument, NewFileDialogState & state)
{
    if (mapDocument.active().changes().dirty())
    {
        state.overwriteKind = MapOverwriteKind::newMap;
        state.mode = MapFileDialogMode::dirtyChanges;
        return;
    }
    executeOperation(mapDocument, state, PendingChangesAction::cancel, std::nullopt, true);
}

void drawOverwriteDialog(MapDocumentManager & mapDocument, NewFileDialogState & state)
{
    std::filesystem::path path = state.operation.path;
    if (state.overwriteKind == MapOverwriteKind::save)
    {
        path = state.savePath.data();
    }
    else if (state.overwriteKind == MapOverwriteKind::standaloneAsset)
    {
        path = state.standaloneOverwritePath;
    }
    ImGui::TextWrapped("The file already exists:\n%s\nReplace it?", path.string().c_str());

    beginDialogFooter({"Overwrite", "Cancel"});
    if (dialogButton(iconLabel(IconId::ok, "Overwrite").c_str()))
    {
        if (state.overwriteKind == MapOverwriteKind::standaloneAsset)
        {
            deferWorkflow(state,
                          [&state]
                          {
                              const auto decision = std::move(state.standaloneOverwriteDecision);
                              state.standaloneOverwritePath.clear();
                              state.standaloneOverwriteDecision = {};
                              closeDialog(state);
                              if (decision)
                              {
                                  decision(true);
                              }
                          });
            closeDialogPopup(state);
            endDialogFooter();
            return;
        }
        deferWorkflow(state,
                      [&mapDocument, &state, path]
                      {
                          if (state.controller != nullptr)
                          {
                              static_cast<void>(state.controller->resolveOverwrite(true));
                          }
                          if (state.overwriteKind == MapOverwriteKind::save)
                          {
                              handleOverwriteSave(mapDocument, state, path);
                          }
                          else
                          {
                              handleOverwriteNewMap(mapDocument, state);
                          }
                      });
        closeDialogPopup(state);
    }

    ImGui::SameLine();

    if (dialogButton(iconLabel(IconId::cancel, "Cancel").c_str()))
    {
        if (state.overwriteKind == MapOverwriteKind::standaloneAsset)
        {
            state.standaloneOverwritePath.clear();
            state.standaloneOverwriteDecision = {};
            closeDialog(state);
            endDialogFooter();
            return;
        }
        deferWorkflow(state,
                      [&state]
                      {
                          if (state.controller != nullptr)
                          {
                              static_cast<void>(state.controller->resolveOverwrite(false));
                          }
                      });
        state.overwriteKind = MapOverwriteKind::none;
        state.operation = {};
        state.saveForPendingOperation = false;
        closeDialog(state);
    }
    endDialogFooter();
}

void drawMapFacetWorkflow(MapDocumentManager & mapDocument, NewFileDialogState & state)
{
    const auto result = drawMapFacetDialog(state);
    if (result == MapFacetDialogResult::accept)
    {
        const auto dimensions = resolveMapDimensions(state.openFacet, state.openWidth, state.openHeight);
        if (dimensions)
        {
            state.operation = {
                .kind = PendingMapOperationKind::openMap,
                .path = state.openPath.data(),
                .dimensions = *dimensions,
            };
            deferWorkflow(state, [&mapDocument, &state] { requestOperation(mapDocument, state); });
        }
    }
    else if (result == MapFacetDialogResult::cancel)
    {
        if (state.browser != nullptr)
        {
            returnToFileBrowser(state, *state.browser);
        }
        else
        {
            closeDialog(state);
        }
    }
}

void drawErrorDialog(NewFileDialogState & state)
{
    ImGui::TextColored(theme::imguiColor(theme::error), "%s", state.error.c_str());
    beginDialogFooter({"OK"});
    if (dialogButton(iconLabel(IconId::ok, "OK").c_str()))
    {
        if (state.controller != nullptr && state.controller->request().kind == OperationKind::saveAs &&
            state.controller->phase() == OperationPhase::awaitingSave)
        {
            state.error.clear();
            state.mode = MapFileDialogMode::saveAs;
            state.activePopup = MapFileDialogMode::none;
            closeDialogPopup(state);
            endDialogFooter();
            return;
        }
        state.operation = {};
        state.overwriteKind = MapOverwriteKind::none;
        state.saveForPendingOperation = false;
        closeDialog(state);
    }
    endDialogFooter();
}

void drawDialogContent(MapDocumentManager & mapDocument, NewFileDialogState & state, MapFileDialogMode mode)
{
    switch (mode)
    {
        case MapFileDialogMode::newType :
            drawNewTypeDialog(mapDocument, state);
            break;
        case MapFileDialogMode::selectMapFacet :
            drawMapFacetWorkflow(mapDocument, state);
            break;
        case MapFileDialogMode::openMap :
            drawOpenDialog(mapDocument, state);
            break;
        case MapFileDialogMode::saveAs :
            drawSaveAsDialog(mapDocument, state);
            break;
        case MapFileDialogMode::dirtyChanges :
            drawDirtyDialog(mapDocument, state);
            break;
        case MapFileDialogMode::overwrite :
            drawOverwriteDialog(mapDocument, state);
            break;
        case MapFileDialogMode::error :
            drawErrorDialog(state);
            break;
        case MapFileDialogMode::none :
            break;
    }
}

} // namespace

void cancelFileDialog(NewFileDialogState & state)
{
    if (state.dialogHost != nullptr && state.dialogHost->owns(DialogId::mapWorkflow))
    {
        state.dialogHost->cancel();
    }
    state.operation = {};
    state.overwriteKind = MapOverwriteKind::none;
    state.saveForPendingOperation = false;
    state.mode = MapFileDialogMode::none;
    state.activePopup = MapFileDialogMode::none;
    state.openWithoutReview = false;
    state.openCandidates.clear();
    state.error.clear();
}

void startNewMapCreation(MapDocumentManager & mapDocument, NewFileDialogState & state)
{
    auto deferredWorkflow = std::move(state.deferWorkflow);
    state.deferWorkflow = {};
    startNewMapOperation(mapDocument, state);
    state.deferWorkflow = std::move(deferredWorkflow);
    if (state.mode != MapFileDialogMode::none)
    {
        requestMapDialog(state, state.mode);
    }
}

void requestMapDialog(NewFileDialogState & state, MapFileDialogMode mode)
{
    const MapFileDialogMode previousMode = state.mode;
    if (mode == MapFileDialogMode::newType && previousMode != mode)
    {
        state.newMapPath.fill('\0');
        state.newFileKind = NewFileKind::map;
        state.initialTileId = 0;
        state.initialTileZ = 0;
        state.radarDefaultColor = {0.0F, 0.0F, 0.0F};
        state.useEmbeddedRadarColors = true;
    }
    state.mode = mode;
    state.activePopup = MapFileDialogMode::none;
    if (state.dialogHost == nullptr)
    {
        return;
    }

    auto request = makeDialogRequest(DialogId::mapWorkflow);
    request.replaceActive = true;
    static_cast<void>(state.dialogHost->request(std::move(request)));
}

void returnToFileBrowser(NewFileDialogState & state, FileBrowserState & browser)
{
    const std::filesystem::path selectedPath(state.openPath.data());
    const std::filesystem::path initialPath = selectedPath.empty() ? std::filesystem::path{} : selectedPath.parent_path();
    cancelFileDialog(state);
    requestFileBrowser(browser,
                       {
                           .mode = FileBrowserMode::openFile,
                           .target = FileBrowserTarget::openGeneric,
                           .initialPath = initialPath,
                           .extension = ".mul",
                           .title = strings::openMulFile(),
                       });
}

void applyFileBrowserResult(NewFileDialogState & state, const FileBrowserResult & result)
{
    switch (result.target)
    {
        case FileBrowserTarget::openMap :
            copyToBuffer(state.openPath, result.path);
            state.openCandidates = result.mapCandidates;
            state.openFacet = 0;
            if (result.assetCandidate)
            {
                applyMapCandidateDimensions(state, *result.assetCandidate);
            }
            for (const auto & candidate : state.openCandidates)
            {
                if (!candidate.valid)
                {
                    continue;
                }
                std::size_t index = 0;
                for (const auto & facet : uomul::knownFacetSizes())
                {
                    if (facet.name == candidate.presetName)
                    {
                        state.openFacet = static_cast<int>(index + 1U);
                        state.openWidth = candidate.dimensions.widthTiles;
                        state.openHeight = candidate.dimensions.heightTiles;
                        break;
                    }
                    index++;
                }
                if (state.openFacet != 0)
                {
                    break;
                }
            }
            requestMapDialog(state, MapFileDialogMode::selectMapFacet);
            break;
        case FileBrowserTarget::createMap :
            copyToBuffer(state.newMapPath, result.path);
            if (state.mode != MapFileDialogMode::none)
            {
                requestMapDialog(state, state.mode);
            }
            break;
        case FileBrowserTarget::saveMapAs :
            copyToBuffer(state.savePath, result.path);
            if (state.mode != MapFileDialogMode::none)
            {
                requestMapDialog(state, state.mode);
            }
            break;
        default :
            return;
    }
}

void drawNewFileDialog(MapDocumentManager & mapDocument,
                       NewFileDialogState & state,
                       OperationController & controller,
                       OutputCollisionPolicy collisionPolicy,
                       NotificationState & notifications,
                       FileBrowserState & browser,
                       IdFormat idFormat)
{
    state.rendering = false;
    state.controller = &controller;
    state.collisionPolicy = collisionPolicy;
    state.notifications = &notifications;
    state.browser = &browser;
    state.idFormat = idFormat;
    if (state.mode == MapFileDialogMode::none)
    {
        if (state.dialogHost != nullptr && state.dialogHost->owns(DialogId::mapWorkflow))
        {
            state.dialogHost->cancel();
        }
        return;
    }

    const ImGuiViewport * viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::Begin("MapFileDialogHost", nullptr, hostFlags);
    ImGui::PushID("MapFileDialogs");

    if (state.activePopup != state.mode)
    {
        ImGui::OpenPopup(popupName(state.mode));
        state.activePopup = state.mode;
    }

    const MapFileDialogMode currentMode = state.mode;

    if (currentMode == MapFileDialogMode::dirtyChanges)
    {
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(dialogCenterPivot, dialogCenterPivot));
    }
    const bool dialogVisible =
        currentMode == MapFileDialogMode::selectMapFacet || currentMode == MapFileDialogMode::newType
            ? beginAutoSizeDialog(popupName(currentMode), nullptr)
            : beginDialog(popupName(currentMode), nullptr, mapDialogSpec(), ImGuiWindowFlags_AlwaysAutoResize);
    if (!dialogVisible)
    {
        if (state.dialogHost != nullptr && state.dialogHost->owns(DialogId::mapWorkflow))
        {
            state.dialogHost->cancel();
        }
        state.mode = MapFileDialogMode::none;
        state.activePopup = MapFileDialogMode::none;
        ImGui::PopID();
        ImGui::End();
        return;
    }

    state.rendering = true;
    drawDialogContent(mapDocument, state, currentMode);
    state.rendering = false;

    endDialog();
    ImGui::PopID();
    ImGui::End();
}

} // namespace uo_mule::gui
