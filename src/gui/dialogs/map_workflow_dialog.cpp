#include "gui/dialogs/map_workflow_dialog.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/dialogs/map_facet_dialog.h"
#include "gui/presentation/dialog_presentation.h"
#include "gui/presentation/iconography.h"
#include "gui/presentation/bounded_buffer.h"
#include "gui/presentation/strings.h"
#include "gui/presentation/theme.h"
#include "gui/workbench/notification_state.h"
#include "gui_model/dialogs/dialog_contracts.h"
#include "gui_model/documents/map_document_manager.h"
#include "gui_model/documents/map_document_state.h"
#include "gui_model/operations/operation_controller.h"
#include "gui_model/profile/data_profile.h"
#include "uomul/facet_sizes.h"
#include "uomul/map/map.h"
#include "uomul/mul_common/mul_common.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
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

constexpr int customFacetIndex = 0;
constexpr std::uint32_t minimumDimension = uomul::mapBlockDimension;
constexpr std::uint32_t dimensionMultiple = uomul::mapBlockDimension;
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

[[nodiscard]] const char * popupName(MapFileDialogMode mode) noexcept
{
    switch (mode)
    {
        case MapFileDialogMode::openMap :
            return "Open File###dialog.map-open";
        case MapFileDialogMode::selectMapFacet :
            return "Select Facet###dialog.map-facet";
        case MapFileDialogMode::newType :
            return "New###dialog.new-file-type";
        case MapFileDialogMode::newMap :
            return "New Empty Map###dialog.new-map";
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

[[nodiscard]] const uomul::FacetSize * facetAt(std::size_t index) noexcept
{
    std::size_t current = 0;
    for (const auto & facet : uomul::knownFacetSizes())
    {
        if (current == index)
        {
            return &facet;
        }
        current++;
    }
    return nullptr;
}

void closeDialog(NewFileDialogState & state)
{
    ImGui::CloseCurrentPopup();
    state.operation = {};
    state.overwriteKind = MapOverwriteKind::none;
    state.saveForPendingOperation = false;
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
    ImGui::CloseCurrentPopup();
    state.mode = MapFileDialogMode::error;
}

[[nodiscard]] std::expected<uomul::Dimensions, std::string>
resolveDimensions(int preset, std::uint32_t width, std::uint32_t height)
{
    if (preset == customFacetIndex)
    {
        if (width < minimumDimension || height < minimumDimension || width % dimensionMultiple != 0U ||
            height % dimensionMultiple != 0U)
        {
            return std::unexpected("Dimensions must be positive multiples of 8 tiles");
        }
        return uomul::Dimensions{.widthTiles = width, .heightTiles = height};
    }

    const auto facetIndex = static_cast<std::size_t>(preset - 1);
    if (facetIndex < uomul::knownFacetSizes().size())
    {
        const auto * facet = facetAt(facetIndex);
        if (facet == nullptr)
        {
            return std::unexpected("Select a supported facet preset or custom dimensions");
        }
        return uomul::Dimensions{.widthTiles = facet->widthTiles, .heightTiles = facet->heightTiles};
    }

    return std::unexpected("Select a supported facet preset or custom dimensions");
}

[[nodiscard]] bool outputExists(const std::filesystem::path & path, NewFileDialogState & state)
{
    std::error_code filesystemError;
    const bool exists = std::filesystem::exists(path, filesystemError);

    if (filesystemError)
    {
        state.error = path.string() + ": " + filesystemError.message();
        ImGui::CloseCurrentPopup();
        state.mode = MapFileDialogMode::error;
        return false;
    }

    return exists;
}

void showOverwrite(NewFileDialogState & state, MapOverwriteKind kind)
{
    ImGui::CloseCurrentPopup();
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
        case PendingMapOperationKind::close :
        case PendingMapOperationKind::quit :
            if (action == PendingChangesAction::discard)
            {
                result = mapDocument.discard();
            }
            break;
        case PendingMapOperationKind::none :
            break;
    }

    if (!result)
    {
        showError(state, result.error());
        return;
    }

    if (state.operation.kind == PendingMapOperationKind::close || state.operation.kind == PendingMapOperationKind::quit)
    {
        const bool quit = state.operation.kind == PendingMapOperationKind::quit;
        const auto closed = mapDocument.closeMap();
        if (!closed)
        {
            showError(state, closed.error());
            return;
        }
        state.shouldClose = quit;
    }

    state.operation = {};
    state.overwriteKind = MapOverwriteKind::none;
    state.saveForPendingOperation = false;
    ImGui::CloseCurrentPopup();
    state.mode = MapFileDialogMode::none;
    state.activePopup = MapFileDialogMode::none;
}

void continueOperation(MapDocumentManager & mapDocument, NewFileDialogState & state, bool overwrite)
{
    state.operationOverwriteOutput = overwrite;

    if (mapDocument.active().changes().dirty())
    {
        ImGui::CloseCurrentPopup();
        state.mode = MapFileDialogMode::dirtyChanges;
        return;
    }

    executeOperation(mapDocument, state, PendingChangesAction::cancel, std::nullopt, overwrite);
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
        ImGui::CloseCurrentPopup();
        state.mode = MapFileDialogMode::error;
        return false;
    }
    if (transition.kind == OperationTransitionKind::showDirtyDecision)
    {
        ImGui::CloseCurrentPopup();
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
            ImGui::CloseCurrentPopup();
            state.mode = MapFileDialogMode::error;
            return;
        }

        if (mapDocument.active().changes().reader_.has_value())
        {
            std::error_code sourcePathError;
            const std::filesystem::path source =
                std::filesystem::weakly_canonical(mapDocument.active().changes().mapPath_, sourcePathError);
            if (!sourcePathError && output == source)
            {
                state.error = "New map output must not replace the currently open map";
                ImGui::CloseCurrentPopup();
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

void drawFacetCombo(int & preset, std::uint32_t & width, std::uint32_t & height)
{
    const auto facetLabel = [](std::size_t index) -> const char *
    {
        if (index == 0U)
        {
            return "Custom dimensions";
        }
        return facetAt(index - 1U)->label;
    };
    const std::size_t facetCount = uomul::knownFacetSizes().size() + 1U;
    const std::size_t requestedIndex = preset < 0 ? 0U : static_cast<std::size_t>(preset);
    const std::size_t selectedIndex = requestedIndex < facetCount ? requestedIndex : 0U;
    dialogFieldLabel("Facet");
    dialogFieldWidth(dimensionFieldWidth);
    if (!ImGui::BeginCombo("##Facet", facetLabel(selectedIndex)))
    {
        return;
    }

    for (std::size_t index = 0; index < facetCount; index++)
    {
        const std::size_t activeIndex = preset < 0 ? 0U : static_cast<std::size_t>(preset);
        const bool selected = activeIndex == index;
        if (ImGui::Selectable(facetLabel(index), selected))
        {
            preset = static_cast<int>(index);
            if (index != 0U)
            {
                const auto dimensions = resolveDimensions(preset, width, height);
                if (dimensions)
                {
                    width = dimensions->widthTiles;
                    height = dimensions->heightTiles;
                }
            }
        }

        if (selected)
        {
            ImGui::SetItemDefaultFocus();
        }
    }

    ImGui::EndCombo();
}

[[nodiscard]] bool validDimensions(std::uint32_t width, std::uint32_t height) noexcept
{
    return width >= minimumDimension && height >= minimumDimension && width % dimensionMultiple == 0U &&
           height % dimensionMultiple == 0U;
}

[[nodiscard]] bool validInitialTile(int tileId, int tileZ) noexcept
{
    return tileId >= 0 && tileId <= maximumTileId && tileZ >= minimumTileZ && tileZ <= maximumTileZ;
}

[[nodiscard]] bool drawCustomDimensions(std::uint32_t & width, std::uint32_t & height)
{
    dialogFieldLabel("Width");
    dialogFieldWidth(dimensionFieldWidth);
    ImGui::InputScalar("##Width", ImGuiDataType_U32, &width, nullptr, nullptr, "%u", ImGuiInputTextFlags_CharsDecimal);
    dialogFieldLabel("Height");
    dialogFieldWidth(dimensionFieldWidth);
    ImGui::InputScalar("##Height", ImGuiDataType_U32, &height, nullptr, nullptr, "%u", ImGuiInputTextFlags_CharsDecimal);
    return validDimensions(width, height);
}

[[nodiscard]] bool drawDimensionsFields(const char * id, int & preset, std::uint32_t & width, std::uint32_t & height)
{
    ImGui::PushID(id);
    drawFacetCombo(preset, width, height);
    const bool valid = preset != customFacetIndex || drawCustomDimensions(width, height);
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
                               .target = FileBrowserTarget::mapOpen,
                               .initialPath = state.openPath.data(),
                               .extension = ".mul",
                               .title = "Open File",
                           });
    }
    const bool validDimensions = drawDimensionsFields("OpenDimensions", state.openFacet, state.openWidth, state.openHeight);
    if (!validDimensions)
    {
        ImGui::TextColored(theme::imguiColor(theme::warning), "Dimensions must be positive multiples of 8 tiles.");
    }
    beginDialogFooter({"Open", "Cancel"});
    if (dialogButton(iconLabel(IconId::folder, "Open").c_str(), state.openPath.front() != '\0' && validDimensions))
    {
        const auto dimensions = resolveDimensions(state.openFacet, state.openWidth, state.openHeight);
        if (dimensions)
        {
            state.operation = {
                .kind = PendingMapOperationKind::openMap,
                .path = state.openPath.data(),
                .dimensions = *dimensions,
            };
            requestOperation(mapDocument, state);
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

void drawNewTypeDialog(NewFileDialogState & state)
{
    constexpr std::array<const char *, 3> fileTypeLabels{"Map", "Radarcol", "Texmap Pair"};
    ImGui::TextUnformatted("Choose a file type to create.");
    ImGui::Separator();

    int selectedType = static_cast<int>(state.newFileKind);
    dialogFieldLabel("File type");
    dialogFieldWidth(newFileTypeFieldWidth);
    if (ImGui::Combo("##NewFileType", &selectedType, fileTypeLabels.data(), static_cast<int>(fileTypeLabels.size())))
    {
        state.newFileKind = static_cast<NewFileKind>(selectedType);
    }

    beginDialogFooter({"Choose location", "Cancel"});
    if (dialogButton(iconLabel(IconId::fileBrowserDialog, "Choose location").c_str()))
    {
        FileBrowserTarget target = FileBrowserTarget::newMapOutput;
        switch (state.newFileKind)
        {
            case NewFileKind::map :
                target = FileBrowserTarget::newMapOutput;
                break;
            case NewFileKind::radarcol :
                target = FileBrowserTarget::newRadarcolOutput;
                break;
            case NewFileKind::texmapPair :
                target = FileBrowserTarget::newTexmapsData;
                break;
        }
        requestFileBrowser(*state.browser,
                           {
                               .mode = FileBrowserMode::saveFile,
                               .target = target,
                               .initialPath = {},
                               .extension = ".mul",
                               .title = "New File",
                           });
        state.mode = state.newFileKind == NewFileKind::map ? MapFileDialogMode::newMap : MapFileDialogMode::none;
        state.activePopup = MapFileDialogMode::none;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (dialogButton(iconLabel(IconId::cancel, "Cancel").c_str()))
    {
        closeDialog(state);
    }
    endDialogFooter();
}

void drawNewMapForm(MapDocumentManager & mapDocument, NewFileDialogState & state)
{
    dialogFieldLabel("Output path");
    ImGui::TextUnformatted(state.newMapPath.data());
    if (ImGui::IsItemClicked() && state.browser != nullptr)
    {
        requestFileBrowser(*state.browser,
                           {
                               .mode = FileBrowserMode::saveFile,
                               .target = FileBrowserTarget::newMapOutput,
                               .initialPath = state.newMapPath.data(),
                               .extension = ".mul",
                               .title = "Create Map",
                           });
    }
    const bool validDimensions =
        drawDimensionsFields("NewDimensions", state.newMapFacet, state.newMapWidth, state.newMapHeight);

    if (!validDimensions)
    {
        ImGui::TextColored(theme::imguiColor(theme::warning), "Dimensions must be positive multiples of 8 tiles.");
    }

    dialogFieldLabel("Tile ID");
    dialogFieldWidth(dimensionFieldWidth);
    ImGui::InputScalar(
        "##TileId", ImGuiDataType_S32, &state.initialTileId, nullptr, nullptr, "%d", ImGuiInputTextFlags_CharsDecimal);
    dialogFieldLabel("Elevation");
    dialogFieldWidth(dimensionFieldWidth);
    ImGui::InputScalar("##Elevation", ImGuiDataType_S32, &state.initialTileZ, nullptr, nullptr, "%d");
    const bool validTile = validInitialTile(state.initialTileId, state.initialTileZ);
    if (!validTile)
    {
        ImGui::TextColored(theme::imguiColor(theme::warning), "ID must be 0-65535 and Z must be -128 to 127.");
    }

    beginDialogFooter({"Create", "Cancel"});
    if (dialogButton(iconLabel(IconId::ok, "Create").c_str(), state.newMapPath.front() != '\0' && validDimensions && validTile))
    {
        const auto dimensions = resolveDimensions(state.newMapFacet, state.newMapWidth, state.newMapHeight);
        if (dimensions)
        {
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
            requestOperation(mapDocument, state);
        }
        else
        {
            state.error = dimensions.error();
            ImGui::CloseCurrentPopup();
            state.mode = MapFileDialogMode::error;
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

    const PendingMapOperationKind operation = state.operation.kind;
    if (operation == PendingMapOperationKind::close || operation == PendingMapOperationKind::quit)
    {
        const bool quit = operation == PendingMapOperationKind::quit;
        const auto closed = mapDocument.closeMap();
        if (!closed)
        {
            showError(state, closed.error());
            return false;
        }
        state.shouldClose = quit;
        state.operation = {};
        state.saveForPendingOperation = false;
        closeDialog(state);
        return true;
    }

    const bool overwriteOperation = state.operationOverwriteOutput;
    state.overwriteKind = MapOverwriteKind::none;
    continueOperation(mapDocument, state, overwriteOperation);
    ImGui::CloseCurrentPopup();
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

void drawSaveAsDialog(MapDocumentManager & mapDocument, NewFileDialogState & state)
{
    dialogFieldLabel("Commit path");
    ImGui::TextUnformatted(state.savePath.data());
    if (ImGui::IsItemClicked() && state.browser != nullptr)
    {
        requestFileBrowser(*state.browser,
                           {
                               .mode = FileBrowserMode::saveFile,
                               .target = FileBrowserTarget::mapSaveAs,
                               .initialPath = state.savePath.data(),
                               .extension = ".mul",
                               .title = "Save Map",
                           });
    }

    beginDialogFooter({"Save", "Cancel"});
    if (dialogButton(iconLabel(IconId::save, "Save").c_str(), state.savePath.front() != '\0'))
    {
        const std::filesystem::path path(state.savePath.data());
        std::error_code outputError;
        const bool exists = std::filesystem::exists(path, outputError);
        if (outputError)
        {
            showError(state,
                      uomul::Error{.code = uomul::ErrorCode::io, .path = path, .offset = 0, .message = outputError.message()});
            endDialogFooter();
            return;
        }
        if (state.controller != nullptr && state.controller->phase() == OperationPhase::idle)
        {
            OperationRequest request{
                .kind = OperationKind::saveAs,
                .documentId = mapDocument.activeDocumentId(),
                .path = path,
                .dimensions = mapDocument.active().changes().dimensions_,
            };
            const auto transition = state.controller->begin(
                std::move(request), mapDocument.active().changes().dirty(), exists, state.collisionPolicy);
            if (transition.kind == OperationTransitionKind::showOverwriteConfirmation)
            {
                showOverwrite(state, MapOverwriteKind::save);
                endDialogFooter();
                return;
            }
            if (transition.kind == OperationTransitionKind::showDirtyDecision)
            {
                state.mode = MapFileDialogMode::dirtyChanges;
                state.activePopup = MapFileDialogMode::none;
                ImGui::CloseCurrentPopup();
                endDialogFooter();
                return;
            }
            if (transition.kind == OperationTransitionKind::rejected)
            {
                showError(state,
                          uomul::Error{.code = uomul::ErrorCode::io, .path = path, .offset = 0, .message = transition.error});
                endDialogFooter();
                return;
            }
        }
        if (state.mode == MapFileDialogMode::error)
        {
            endDialogFooter();
            return;
        }

        if (!saveAs(mapDocument, state, path))
        {
            endDialogFooter();
            return;
        }
    }

    ImGui::SameLine();

    if (dialogButton(iconLabel(IconId::cancel, "Cancel").c_str()))
    {
        state.saveForPendingOperation = false;
        closeDialog(state);
    }
    endDialogFooter();
}

void drawCloseDirtyDialog(MapDocumentManager & mapDocument, NewFileDialogState & state)
{
    std::string filename = mapDocument.active().changes().originalPath_.filename().string();
    if (filename.empty())
    {
        filename = "map";
    }
    ImGui::Text("Save changes to \"%s\" before closing?", filename.c_str());
    ImGui::TextColored(
        theme::imguiColor(theme::mutedText), "%s", "If you close without saving, your changes will be discarded.");

    beginDialogFooter({"Close without saving", "Cancel", "Save"});
    if (dialogButton(iconLabel(IconId::close, "Close without saving").c_str()))
    {
        if (state.controller != nullptr)
        {
            static_cast<void>(state.controller->resolveDirty(PendingChangesAction::discard));
        }
        executeOperation(mapDocument, state, PendingChangesAction::discard, std::nullopt, state.operationOverwriteOutput);
    }

    ImGui::SameLine();
    if (dialogButton(iconLabel(IconId::cancel, "Cancel").c_str()))
    {
        if (state.controller != nullptr)
        {
            static_cast<void>(state.controller->cancel());
        }
        closeDialog(state);
    }

    ImGui::SameLine();
    if (dialogButton(iconLabel(IconId::save, "Save").c_str()))
    {
        if (state.controller != nullptr)
        {
            static_cast<void>(state.controller->resolveDirty(PendingChangesAction::save));
        }
        const auto saved = mapDocument.save(mapDocument.active().changes().originalPath_, false);
        if (!saved)
        {
            if (state.controller != nullptr)
            {
                static_cast<void>(state.controller->completeSave(false));
            }
            showError(state, saved.error());
            endDialogFooter();
            return;
        }
        const auto transition = state.controller == nullptr
                                    ? OperationTransition{.kind = OperationTransitionKind::execute, .error = {}}
                                    : state.controller->completeSave(true);
        if (transition.kind == OperationTransitionKind::execute)
        {
            executeOperation(mapDocument, state, PendingChangesAction::cancel, std::nullopt, state.operationOverwriteOutput);
        }
        else
        {
            closeDialog(state);
        }
    }
    endDialogFooter();
}

void drawDirtyDialog(MapDocumentManager & mapDocument, NewFileDialogState & state)
{
    if (state.operation.kind == PendingMapOperationKind::close || state.operation.kind == PendingMapOperationKind::quit)
    {
        drawCloseDirtyDialog(mapDocument, state);
        return;
    }
    ImGui::TextWrapped("The current map has staged changes. Save, save as, discard, or cancel this operation?");

    beginDialogFooter({"Save", "Save As...", "Discard", "Cancel"});
    if (dialogButton(iconLabel(IconId::save, "Save").c_str()))
    {
        if (state.controller != nullptr)
        {
            static_cast<void>(state.controller->resolveDirty(PendingChangesAction::save));
        }
        const auto saved = mapDocument.save(mapDocument.active().changes().originalPath_, false);
        if (!saved)
        {
            if (state.controller != nullptr)
            {
                static_cast<void>(state.controller->completeSave(false));
            }
            showError(state, saved.error());
            endDialogFooter();
            return;
        }
        const auto transition = state.controller == nullptr
                                    ? OperationTransition{.kind = OperationTransitionKind::execute, .error = {}}
                                    : state.controller->completeSave(true);
        if (transition.kind == OperationTransitionKind::execute)
        {
            const bool overwriteOperation = state.operationOverwriteOutput;
            continueOperation(mapDocument, state, overwriteOperation);
        }
        else
        {
            closeDialog(state);
        }
    }

    ImGui::SameLine();

    if (dialogButton(iconLabel(IconId::save, "Save As...").c_str()))
    {
        if (state.controller != nullptr)
        {
            static_cast<void>(state.controller->resolveDirty(PendingChangesAction::save));
        }
        state.saveForPendingOperation = true;
        state.savePath.fill('\0');
        requestFileBrowser(*state.browser,
                           {
                               .mode = FileBrowserMode::saveFile,
                               .target = FileBrowserTarget::mapSaveAs,
                               .initialPath = mapDocument.active().changes().originalPath_,
                               .extension = ".mul",
                               .title = "Save Map As",
                           });
        state.mode = MapFileDialogMode::saveAs;
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (dialogButton(iconLabel(IconId::cancel, "Discard").c_str()))
    {
        if (state.controller != nullptr)
        {
            static_cast<void>(state.controller->resolveDirty(PendingChangesAction::discard));
        }
        executeOperation(mapDocument, state, PendingChangesAction::discard, std::nullopt, state.operationOverwriteOutput);
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (dialogButton(iconLabel(IconId::cancel, "Cancel").c_str()))
    {
        if (state.controller != nullptr)
        {
            static_cast<void>(state.controller->cancel());
        }
        state.operation = {};
        state.overwriteKind = MapOverwriteKind::none;
        closeDialog(state);
    }
    endDialogFooter();
}

void finishPendingCloseAfterSave(MapDocumentManager & mapDocument, NewFileDialogState & state)
{
    const bool quit = state.operation.kind == PendingMapOperationKind::quit;
    const auto closed = mapDocument.closeMap();
    if (!closed)
    {
        showError(state, closed.error());
        return;
    }
    state.shouldClose = quit;
    state.operation = {};
    state.saveForPendingOperation = false;
    state.mode = MapFileDialogMode::none;
    state.activePopup = MapFileDialogMode::none;
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
            ImGui::CloseCurrentPopup();
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
    if (state.operation.kind == PendingMapOperationKind::close || state.operation.kind == PendingMapOperationKind::quit)
    {
        finishPendingCloseAfterSave(mapDocument, state);
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
    const std::filesystem::path path =
        state.overwriteKind == MapOverwriteKind::save ? std::filesystem::path(state.savePath.data()) : state.operation.path;
    ImGui::TextWrapped("The file already exists:\n%s\nReplace it?", path.string().c_str());

    beginDialogFooter({"Overwrite", "Cancel"});
    if (dialogButton(iconLabel(IconId::ok, "Overwrite").c_str()))
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

        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (dialogButton(iconLabel(IconId::cancel, "Cancel").c_str()))
    {
        if (state.controller != nullptr)
        {
            static_cast<void>(state.controller->resolveOverwrite(false));
        }
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
        const auto dimensions = resolveDimensions(state.openFacet, state.openWidth, state.openHeight);
        if (dimensions)
        {
            state.operation = {
                .kind = PendingMapOperationKind::openMap,
                .path = state.openPath.data(),
                .dimensions = *dimensions,
            };
            requestOperation(mapDocument, state);
        }
    }
    else if (result == MapFacetDialogResult::cancel)
    {
        closeDialog(state);
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
            ImGui::CloseCurrentPopup();
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
            drawNewTypeDialog(state);
            break;
        case MapFileDialogMode::selectMapFacet :
            drawMapFacetWorkflow(mapDocument, state);
            break;
        case MapFileDialogMode::openMap :
            drawOpenDialog(mapDocument, state);
            break;
        case MapFileDialogMode::newMap :
            drawNewMapForm(mapDocument, state);
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
    state.operation = {};
    state.overwriteKind = MapOverwriteKind::none;
    state.saveForPendingOperation = false;
    state.mode = MapFileDialogMode::none;
    state.activePopup = MapFileDialogMode::none;
    state.openWithoutReview = false;
    state.openCandidates.clear();
    state.error.clear();
}

void applyFileBrowserResult(NewFileDialogState & state, const FileBrowserResult & result)
{
    switch (result.target)
    {
        case FileBrowserTarget::mapOpen :
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
            state.mode = MapFileDialogMode::selectMapFacet;
            state.activePopup = MapFileDialogMode::none;
            break;
        case FileBrowserTarget::newMapOutput :
            copyToBuffer(state.newMapPath, result.path);
            break;
        case FileBrowserTarget::mapSaveAs :
            copyToBuffer(state.savePath, result.path);
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
                       bool & shouldClose)
{
    state.shouldClose = false;
    state.controller = &controller;
    state.collisionPolicy = collisionPolicy;
    state.notifications = &notifications;
    state.browser = &browser;
    if (state.mode == MapFileDialogMode::none)
    {
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

    const DialogWindowSpec dialogSpec =
        currentMode == MapFileDialogMode::selectMapFacet ? mapFacetDialogSpec() : mapDialogSpec();
    if (currentMode == MapFileDialogMode::dirtyChanges)
    {
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(dialogCenterPivot, dialogCenterPivot));
    }
    if (!beginDialog(popupName(currentMode), nullptr, dialogSpec, ImGuiWindowFlags_AlwaysAutoResize))
    {
        state.mode = MapFileDialogMode::none;
        state.activePopup = MapFileDialogMode::none;
        ImGui::PopID();
        ImGui::End();
        return;
    }

    drawDialogContent(mapDocument, state, currentMode);

    endDialog();
    ImGui::PopID();
    ImGui::End();

    if (state.shouldClose)
    {
        shouldClose = true;
        state.shouldClose = false;
    }
}

void requestCloseOperation(MapDocumentManager & mapDocument,
                           NewFileDialogState & state,
                           OperationController & controller,
                           PendingMapOperationKind kind,
                           NotificationState & notifications)
{
    const OperationKind operationKind = kind == PendingMapOperationKind::quit ? OperationKind::quit : OperationKind::close;
    OperationRequest request{};
    request.kind = operationKind;
    const auto transition =
        controller.begin(request, mapDocument.active().changes().dirty(), false, OutputCollisionPolicy::confirm);
    state.operation = {};
    state.operation.kind = kind;
    if (transition.kind == OperationTransitionKind::showDirtyDecision)
    {
        state.mode = MapFileDialogMode::dirtyChanges;
        state.activePopup = MapFileDialogMode::none;
        return;
    }
    if (transition.kind == OperationTransitionKind::rejected)
    {
        state.error = transition.error;
        state.mode = MapFileDialogMode::error;
        notifications.add(NotificationSeverity::error, transition.error);
        return;
    }
    if (transition.kind == OperationTransitionKind::execute)
    {
        const auto closed = mapDocument.closeMap();
        if (!closed)
        {
            state.error = closed.error().message;
            state.mode = MapFileDialogMode::error;
            notifications.add(NotificationSeverity::error, state.error);
            return;
        }
        state.shouldClose = kind == PendingMapOperationKind::quit;
        state.operation = {};
    }
}

} // namespace uo_mule::gui
