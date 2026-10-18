#include "gui/new_map_dialog.h"
#include "gui/data_profile.h"
#include "gui/theme.h"
#include "gui/map_workspace.h"
#include "gui/panels/common.h"
#include "uomul/map.h"
#include "uomul/mul_common/mul_common.h"
#include <algorithm>
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

namespace uo_mule::gui
{

namespace
{

constexpr std::size_t facetCount = 7;
constexpr int customFacetIndex = 0;
constexpr std::size_t customFacetArrayIndex = 0;
constexpr std::size_t facetNameBufferSize = 32;
constexpr std::array<std::array<char, facetNameBufferSize>, facetCount> facetNames{
    std::array<char, facetNameBufferSize>{"Custom dimensions"},
    std::array<char, facetNameBufferSize>{"Felucca"},
    std::array<char, facetNameBufferSize>{"Trammel"},
    std::array<char, facetNameBufferSize>{"Ilshenar"},
    std::array<char, facetNameBufferSize>{"Malas"},
    std::array<char, facetNameBufferSize>{"Tokuno"},
    std::array<char, facetNameBufferSize>{"Ter-Mur"},
};
constexpr std::uint32_t minimumDimension = uomul::mapBlockDimension;
constexpr std::uint32_t dimensionMultiple = uomul::mapBlockDimension;
constexpr int maximumTileId = std::numeric_limits<std::uint16_t>::max();
constexpr int minimumTileZ = -std::numeric_limits<std::int8_t>::max() - 1;
constexpr int maximumTileZ = std::numeric_limits<std::int8_t>::max();
constexpr int feluccaFacetIndex = 1;
constexpr int trammelFacetIndex = 2;
constexpr int ilshenarFacetIndex = 3;
constexpr int malasFacetIndex = 4;
constexpr int tokunoFacetIndex = 5;
constexpr int terMurFacetIndex = 6;
constexpr float dialogInputWidth = 420.0F;
constexpr ImGuiWindowFlags modalFlags = ImGuiWindowFlags_AlwaysAutoResize;
constexpr ImGuiWindowFlags hostFlags = static_cast<ImGuiWindowFlags>(
    static_cast<unsigned int>(ImGuiWindowFlags_NoDecoration) | static_cast<unsigned int>(ImGuiWindowFlags_NoBackground) |
    static_cast<unsigned int>(ImGuiWindowFlags_NoSavedSettings) | static_cast<unsigned int>(ImGuiWindowFlags_NoInputs) |
    static_cast<unsigned int>(ImGuiWindowFlags_NoNav) | static_cast<unsigned int>(ImGuiWindowFlags_NoFocusOnAppearing) |
    static_cast<unsigned int>(ImGuiWindowFlags_NoBringToFrontOnFocus));

[[nodiscard]] const char * popupName(MapFileDialogMode mode) noexcept
{
    switch (mode)
    {
        case MapFileDialogMode::openMap :
            return "Open Map###OpenMapDialog";
        case MapFileDialogMode::newMap :
            return "New Empty Map###NewMapDialog";
        case MapFileDialogMode::saveAs :
            return "Save As / Commit###SaveMapDialog";
        case MapFileDialogMode::dirtyChanges :
            return "Pending Changes###PendingChangesDialog";
        case MapFileDialogMode::overwrite :
            return "Confirm Overwrite###OverwriteMapDialog";
        case MapFileDialogMode::error :
            return "Map Operation Failed###MapDialogError";
        case MapFileDialogMode::none :
            break;
    }
    return "###NoMapDialog";
}

void closeDialog(NewMapDialogState & state)
{
    ImGui::CloseCurrentPopup();
    state.operation = {};
    state.overwriteKind = MapOverwriteKind::none;
    state.saveForPendingOperation = false;
    state.mode = MapFileDialogMode::none;
    state.activePopup = MapFileDialogMode::none;
    state.error.clear();
}

void showError(NewMapDialogState & state, const uomul::Error & error)
{
    state.error = formatError(error);
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

    std::string presetName;

    switch (preset)
    {
        case feluccaFacetIndex :
            presetName = "felucca";
            break;
        case trammelFacetIndex :
            presetName = "trammel";
            break;
        case ilshenarFacetIndex :
            presetName = "ilshenar";
            break;
        case malasFacetIndex :
            presetName = "malas";
            break;
        case tokunoFacetIndex :
            presetName = "tokuno";
            break;
        case terMurFacetIndex :
            presetName = "ter-mur";
            break;
        default :
            break;
    }

    if (!presetName.empty())
    {
        const auto dimensions = uomul::facetDimensions(presetName);
        if (!dimensions)
        {
            return std::unexpected(dimensions.error().message);
        }

        return *dimensions;
    }

    return std::unexpected("Select a supported facet preset or custom dimensions");
}

[[nodiscard]] bool outputExists(const std::filesystem::path & path, NewMapDialogState & state)
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

void showOverwrite(NewMapDialogState & state, MapOverwriteKind kind)
{
    ImGui::CloseCurrentPopup();
    state.overwriteKind = kind;

    if (kind == MapOverwriteKind::newMap)
    {
        state.operationOverwriteOutput = true;
    }

    state.mode = MapFileDialogMode::overwrite;
}

void executeOperation(MapWorkspace & workspace,
                      NewMapDialogState & state,
                      PendingChangesAction action,
                      const std::optional<std::filesystem::path> & savePath,
                      bool overwrite)
{
    std::expected<void, uomul::Error> result;

    switch (state.operation.kind)
    {
        case PendingMapOperationKind::openMap :
            result = workspace.openMap(state.operation.path, state.operation.dimensions, action, savePath, overwrite);
            break;
        case PendingMapOperationKind::newMap :
            result = workspace.createEmptyMap(
                state.operation.path, state.operation.dimensions, action, savePath, overwrite, state.operation.initialTile);
            break;
        case PendingMapOperationKind::close :
        case PendingMapOperationKind::quit :
            if (action == PendingChangesAction::discard)
            {
                result = workspace.discard();
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
        const auto closed = workspace.closeMap();
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

void continueOperation(MapWorkspace & workspace, NewMapDialogState & state, bool overwrite)
{
    state.operationOverwriteOutput = overwrite;

    if (workspace.dirty())
    {
        ImGui::CloseCurrentPopup();
        state.mode = MapFileDialogMode::dirtyChanges;
        return;
    }

    executeOperation(workspace, state, PendingChangesAction::cancel, std::nullopt, overwrite);
}

void requestOperation(MapWorkspace & workspace, NewMapDialogState & state)
{
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

        if (workspace.reader() != nullptr)
        {
            std::error_code sourcePathError;
            const std::filesystem::path source = std::filesystem::weakly_canonical(workspace.mapPath(), sourcePathError);
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

    continueOperation(workspace, state, false);
}

void drawFacetCombo(int & preset, std::uint32_t & width, std::uint32_t & height)
{
    const std::size_t selectedIndex = preset >= 0 && static_cast<std::size_t>(preset) < facetNames.size()
                                          ? static_cast<std::size_t>(preset)
                                          : customFacetArrayIndex;
    if (!ImGui::BeginCombo("Facet", facetNames.at(selectedIndex).data()))
    {
        return;
    }

    for (std::size_t index = 0; index < facetNames.size(); index++)
    {
        const std::size_t activeIndex = preset >= 0 ? static_cast<std::size_t>(preset) : customFacetArrayIndex;
        const bool selected = activeIndex == index;
        if (ImGui::Selectable(facetNames.at(index).data(), selected))
        {
            preset = static_cast<int>(index);
            if (index != customFacetArrayIndex)
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

[[nodiscard]] bool drawCustomDimensions(std::uint32_t & width, std::uint32_t & height)
{
    ImGui::InputScalar("Width (tiles)", ImGuiDataType_U32, &width);
    ImGui::InputScalar("Height (tiles)", ImGuiDataType_U32, &height);
    return width >= minimumDimension && height >= minimumDimension && width % dimensionMultiple == 0U &&
           height % dimensionMultiple == 0U;
}

[[nodiscard]] bool drawDimensionsFields(const char * id, int & preset, std::uint32_t & width, std::uint32_t & height)
{
    ImGui::PushID(id);
    drawFacetCombo(preset, width, height);
    const bool valid = preset != customFacetIndex || drawCustomDimensions(width, height);
    ImGui::PopID();
    return valid;
}

void initializeProfileMapSelection(const DataProfile & profile, NewMapDialogState & state)
{
    const auto availableMapCount = static_cast<int>(std::min(mapFacetCount, profile.files.size()));

    if (state.openProfileInitialized)
    {
        return;
    }

    state.openProfileInitialized = true;
    state.openMapRole = std::clamp(state.openMapRole, 0, std::max(0, availableMapCount - 1));
}

[[nodiscard]] std::optional<std::filesystem::path>
profileMapPath(const DataProfile & profile, int mapRoleIndex)
{
    const auto role = static_cast<DataFileRole>(mapRoleIndex);
    auto path = resolvedDataFilePath(profile, role);
    if (path.empty())
    {
        return std::nullopt;
    }

    return path;
}

[[nodiscard]] std::optional<uomul::Dimensions>
profileMapDimensions(const DataProfile & profile, int mapRoleIndex)
{
    const auto index = static_cast<std::size_t>(mapRoleIndex);
    return profile.facets.at(index).dimensions;
}

void drawOpenDialog(MapWorkspace & workspace, const DataProfile & profile, NewMapDialogState & state)
{
    initializeProfileMapSelection(profile, state);
    std::array<std::string, mapFacetCount> mapLabels{};
    std::array<const char *, mapFacetCount> mapLabelPointers{};
    for (std::size_t index = 0; index < mapFacetCount; index++)
    {
        mapLabels.at(index) = std::string(dataFileRoleName(static_cast<DataFileRole>(index)));
        mapLabelPointers.at(index) = mapLabels.at(index).c_str();
    }

    ImGui::TextUnformatted("Map role");
    ImGui::SetNextItemWidth(dialogInputWidth);
    ImGui::Combo("##OpenMapRole", &state.openMapRole, mapLabelPointers.data(), static_cast<int>(mapLabelPointers.size()));
    const auto path = profileMapPath(profile, state.openMapRole);
    const auto dimensions = profileMapDimensions(profile, state.openMapRole);

    if (path)
    {
        ImGui::Text("Path: %s", path->string().c_str());
    }

    if (!path)
    {
        ImGui::TextColored(theme::imguiColor(theme::amber), "The selected map role is missing from the active profile.");
    }

    const bool validDimensions = dimensions.has_value();
    if (dimensions)
    {
        ImGui::Text("Dimensions: %u x %u", dimensions->widthTiles, dimensions->heightTiles);
    }

    if (!dimensions)
    {
        ImGui::TextColored(theme::imguiColor(theme::amber), "Configure facet dimensions in UO Data Paths first.");
    }

    if (!validDimensions)
    {
        ImGui::TextColored(theme::imguiColor(theme::amber), "Map dimensions are required before opening this role.");
    }

    if (ImGui::Button("Open") && path && validDimensions)
    {
        state.operation = {
            .kind = PendingMapOperationKind::openMap,
            .path = *path,
            .dimensions = *dimensions,
        };
        requestOperation(workspace, state);
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel"))
    {
        state.operation = {};
        closeDialog(state);
    }
}

void drawNewMapForm(MapWorkspace & workspace, NewMapDialogState & state)
{
    ImGui::TextUnformatted("Output path");
    ImGui::SetNextItemWidth(dialogInputWidth);
    ImGui::InputText("##NewMapPath", state.newMapPath.data(), state.newMapPath.size());
    const bool validDimensions =
        drawDimensionsFields("NewDimensions", state.newMapFacet, state.newMapWidth, state.newMapHeight);

    if (!validDimensions)
    {
        ImGui::TextColored(theme::imguiColor(theme::amber), "Dimensions must be positive multiples of 8 tiles.");
    }

    ImGui::InputInt("Default tile ID", &state.initialTileId);
    ImGui::InputInt("Default Z", &state.initialTileZ);
    const bool validTile = state.initialTileId >= 0 && state.initialTileId <= maximumTileId &&
                           state.initialTileZ >= minimumTileZ && state.initialTileZ <= maximumTileZ;
    if (!validTile)
    {
        ImGui::TextColored(theme::imguiColor(theme::amber), "ID must be 0-65535 and Z must be -128 to 127.");
    }

    if (ImGui::Button("Create") && state.newMapPath.front() != '\0' && validDimensions && validTile)
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
            requestOperation(workspace, state);
        }
        else
        {
            state.error = dimensions.error();
            ImGui::CloseCurrentPopup();
            state.mode = MapFileDialogMode::error;
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel"))
    {
        state.operation = {};
        closeDialog(state);
    }
}

void drawSaveAsDialog(MapWorkspace & workspace, NewMapDialogState & state)
{
    ImGui::TextUnformatted("Commit staged changes to");
    ImGui::SetNextItemWidth(dialogInputWidth);
    ImGui::InputText("##SaveMapPath", state.savePath.data(), state.savePath.size());

    if (ImGui::Button("Save / Commit") && state.savePath.front() != '\0')
    {
        const std::filesystem::path path(state.savePath.data());
        if (outputExists(path, state))
        {
            showOverwrite(state, MapOverwriteKind::save);
            return;
        }

        if (state.mode == MapFileDialogMode::error)
        {
            return;
        }

        const auto saved = workspace.save(path, false);

        if (!saved)
        {
            showError(state, saved.error());
            return;
        }

        if (state.saveForPendingOperation)
        {
            const PendingMapOperationKind operation = state.operation.kind;
            if (operation == PendingMapOperationKind::close || operation == PendingMapOperationKind::quit)
            {
                const bool quit = operation == PendingMapOperationKind::quit;
                const auto closed = workspace.closeMap();
                if (!closed)
                {
                    showError(state, closed.error());
                    return;
                }
                state.shouldClose = quit;
                state.operation = {};
                state.saveForPendingOperation = false;
                closeDialog(state);
            }
            else
            {
                const bool overwriteOperation = state.operationOverwriteOutput;
                state.overwriteKind = MapOverwriteKind::none;
                continueOperation(workspace, state, overwriteOperation);
                ImGui::CloseCurrentPopup();
            }
            return;
        }
        closeDialog(state);
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel"))
    {
        state.saveForPendingOperation = false;
        closeDialog(state);
    }
}

void drawDirtyDialog(MapWorkspace & workspace, NewMapDialogState & state)
{
    ImGui::TextWrapped("The current map has staged changes. Save, discard, or cancel this operation?");

    if (ImGui::Button("Save As..."))
    {
        state.saveForPendingOperation = true;
        state.savePath.fill('\0');
        state.mode = MapFileDialogMode::saveAs;
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Discard"))
    {
        executeOperation(workspace, state, PendingChangesAction::discard, std::nullopt, state.operationOverwriteOutput);
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel"))
    {
        state.operation = {};
        state.overwriteKind = MapOverwriteKind::none;
        closeDialog(state);
    }
}

void drawOverwriteDialog(MapWorkspace & workspace, NewMapDialogState & state)
{
    const std::filesystem::path path =
        state.overwriteKind == MapOverwriteKind::save ? std::filesystem::path(state.savePath.data()) : state.operation.path;
    ImGui::TextWrapped("The file already exists:\n%s\nReplace it?", path.string().c_str());

    if (ImGui::Button("Overwrite"))
    {
        if (state.overwriteKind == MapOverwriteKind::save)
        {
            const auto saved = workspace.save(path, true);
            if (!saved)
            {
                showError(state, saved.error());
            }
            else if (state.saveForPendingOperation)
            {
                if (state.operation.kind == PendingMapOperationKind::close ||
                    state.operation.kind == PendingMapOperationKind::quit)
                {
                    const bool quit = state.operation.kind == PendingMapOperationKind::quit;
                    const auto closed = workspace.closeMap();
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
                else
                {
                    const bool overwriteOperation = state.operationOverwriteOutput;
                    state.overwriteKind = MapOverwriteKind::none;
                    continueOperation(workspace, state, overwriteOperation);
                }
            }
            else
            {
                state.mode = MapFileDialogMode::none;
            }
        }
        else if (workspace.dirty())
        {
            state.overwriteKind = MapOverwriteKind::newMap;
            state.mode = MapFileDialogMode::dirtyChanges;
        }
        else
        {
            executeOperation(workspace, state, PendingChangesAction::cancel, std::nullopt, true);
        }

        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel"))
    {
        state.overwriteKind = MapOverwriteKind::none;
        state.operation = {};
        state.saveForPendingOperation = false;
        closeDialog(state);
    }
}

} // namespace

void drawNewMapDialog(MapWorkspace & workspace,
                      const DataProfile & profile,
                      NewMapDialogState & state,
                      bool & shouldClose)
{
    state.shouldClose = false;
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

    if (!ImGui::BeginPopupModal(popupName(currentMode), nullptr, modalFlags))
    {
        state.mode = MapFileDialogMode::none;
        state.activePopup = MapFileDialogMode::none;
        ImGui::PopID();
        ImGui::End();
        return;
    }

    switch (currentMode)
    {
        case MapFileDialogMode::openMap :
            drawOpenDialog(workspace, profile, state);
            break;
        case MapFileDialogMode::newMap :
            drawNewMapForm(workspace, state);
            break;
        case MapFileDialogMode::saveAs :
            drawSaveAsDialog(workspace, state);
            break;
        case MapFileDialogMode::dirtyChanges :
            drawDirtyDialog(workspace, state);
            break;
        case MapFileDialogMode::overwrite :
            drawOverwriteDialog(workspace, state);
            break;
        case MapFileDialogMode::error :
            ImGui::TextColored(theme::imguiColor(theme::coral), "%s", state.error.c_str());
            if (ImGui::Button("OK"))
            {
                state.operation = {};
                state.overwriteKind = MapOverwriteKind::none;
                state.saveForPendingOperation = false;
                closeDialog(state);
            }
            break;
        case MapFileDialogMode::none :
            break;
    }

    ImGui::EndPopup();
    ImGui::PopID();
    ImGui::End();

    if (state.shouldClose)
    {
        shouldClose = true;
        state.shouldClose = false;
    }
}

} // namespace uo_mule::gui
