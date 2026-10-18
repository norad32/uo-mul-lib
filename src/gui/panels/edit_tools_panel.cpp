#include "gui/panels/edit_tools_panel.h"
#include "gui/map_workspace.h"
#include "gui/theme.h"
#include "uomul/map.h"
#include "uomul/mul_common/mul_common.h"
#include <cstdint>
#include <expected>
#include <imgui.h>
#include <limits>
#include <optional>
#include <string>

namespace uo_mule::gui
{

namespace
{

constexpr int minimumTileId = std::numeric_limits<std::uint16_t>::min();
constexpr int maximumTileId = std::numeric_limits<std::uint16_t>::max();
constexpr int minimumTileZ = -(std::numeric_limits<std::uint8_t>::max() / 2) - 1;
constexpr int maximumTileZ = std::numeric_limits<std::uint8_t>::max() / 2;
constexpr ImGuiWindowFlags confirmationFlags = ImGuiWindowFlags_AlwaysAutoResize;
constexpr const char * wholeMapConfirmationPopup = "Confirm Whole-Map Edit###EditToolsWholeMapConfirmation";
[[nodiscard]] ImVec4 statusColor(bool isError) noexcept
{
    return theme::imguiColor(isError ? theme::coral : theme::sageGreen);
}

[[nodiscard]] bool selectionAvailable(const MapWorkspace & workspace) noexcept
{
    if (workspace.selectionMode() == MapSelectionMode::tiles)
    {
        const std::optional<uomul::TileRegion> & selection = workspace.tileSelection();
        return selection && selection->min.x < selection->maxExclusive.x && selection->min.y < selection->maxExclusive.y;
    }

    const std::optional<uomul::BlockRegion> & selection = workspace.blockSelection();
    return selection && selection->min.x < selection->maxExclusive.x && selection->min.y < selection->maxExclusive.y;
}

[[nodiscard]] const char * selectionStyleName(MapSelectionMode mode) noexcept
{
    return mode == MapSelectionMode::tiles ? "Tile selection" : "Block selection";
}

[[nodiscard]] const char * operationName(EditToolsOperation operation) noexcept
{
    switch (operation)
    {
        case EditToolsOperation::fill :
            return "Fill both tile ID and Z";
        case EditToolsOperation::replaceTileId :
            return "Replace tile ID (preserve Z)";
        case EditToolsOperation::replaceZ :
            return "Replace Z (preserve tile ID)";
        case EditToolsOperation::none :
            break;
    }
    return "Edit";
}

[[nodiscard]] bool inputsValid(const EditToolsPanelState & state) noexcept
{
    return state.fillId >= minimumTileId && state.fillId <= maximumTileId && state.fillZ >= minimumTileZ &&
           state.fillZ <= maximumTileZ && state.replaceIdFrom >= minimumTileId && state.replaceIdFrom <= maximumTileId &&
           state.replaceIdTo >= minimumTileId && state.replaceIdTo <= maximumTileId && state.replaceZFrom >= minimumTileZ &&
           state.replaceZFrom <= maximumTileZ && state.replaceZTo >= minimumTileZ && state.replaceZTo <= maximumTileZ;
}

[[nodiscard]] bool operationInputsValid(const EditToolsPanelState & state, EditToolsOperation operation) noexcept
{
    switch (operation)
    {
        case EditToolsOperation::fill :
            return state.fillId >= minimumTileId && state.fillId <= maximumTileId && state.fillZ >= minimumTileZ &&
                   state.fillZ <= maximumTileZ;
        case EditToolsOperation::replaceTileId :
            return state.replaceIdFrom >= minimumTileId && state.replaceIdFrom <= maximumTileId &&
                   state.replaceIdTo >= minimumTileId && state.replaceIdTo <= maximumTileId;
        case EditToolsOperation::replaceZ :
            return state.replaceZFrom >= minimumTileZ && state.replaceZFrom <= maximumTileZ &&
                   state.replaceZTo >= minimumTileZ && state.replaceZTo <= maximumTileZ;
        case EditToolsOperation::none :
            return false;
    }
    return false;
}

void drawSelectionDetails(const MapWorkspace & workspace)
{
    ImGui::Text("Selection style: %s", selectionStyleName(workspace.selectionMode()));
    if (workspace.selectionMode() == MapSelectionMode::tiles && workspace.tileSelection())
    {
        const uomul::TileRegion & selection = *workspace.tileSelection();
        ImGui::Text("Selected tiles: (%u, %u) to (%u, %u), max exclusive",
                    selection.min.x,
                    selection.min.y,
                    selection.maxExclusive.x,
                    selection.maxExclusive.y);
        return;
    }

    if (workspace.selectionMode() == MapSelectionMode::blocks && workspace.blockSelection())
    {
        const uomul::BlockRegion & selection = *workspace.blockSelection();
        ImGui::Text("Selected blocks: (%u, %u) to (%u, %u), max exclusive",
                    selection.min.x,
                    selection.min.y,
                    selection.maxExclusive.x,
                    selection.maxExclusive.y);
        return;
    }

    ImGui::TextUnformatted("No active selection");
}

void drawOperationPreview(const EditToolsPanelState & state, EditToolsOperation operation)
{
    switch (operation)
    {
        case EditToolsOperation::fill :
            ImGui::Text("Operation: Fill both ID and Z with %d, %d", state.fillId, state.fillZ);
            break;
        case EditToolsOperation::replaceTileId :
            ImGui::Text("Operation: Replace ID %d with %d; preserve Z", state.replaceIdFrom, state.replaceIdTo);
            break;
        case EditToolsOperation::replaceZ :
            ImGui::Text("Operation: Replace Z %d with %d; preserve ID", state.replaceZFrom, state.replaceZTo);
            break;
        case EditToolsOperation::none :
            break;
    }
}

void applyOperation(MapWorkspace & workspace, EditToolsPanelState & state, EditToolsOperation operation, MapEditScope scope)
{
    std::expected<void, uomul::Error> result;

    switch (operation)
    {
        case EditToolsOperation::fill :
            result = workspace.fill(
                {.id = static_cast<std::uint16_t>(state.fillId), .z = static_cast<std::int8_t>(state.fillZ)}, scope);
            break;
        case EditToolsOperation::replaceTileId :
            result = workspace.replaceTileId(
                static_cast<std::uint16_t>(state.replaceIdFrom), static_cast<std::uint16_t>(state.replaceIdTo), scope);
            break;
        case EditToolsOperation::replaceZ :
            result = workspace.replaceZ(
                static_cast<std::int8_t>(state.replaceZFrom), static_cast<std::int8_t>(state.replaceZTo), scope);
            break;
        case EditToolsOperation::none :
            return;
    }

    if (!result)
    {
        const uomul::Error & error = result.error();
        state.status = error.path.empty() ? error.message : error.path.string() + ": " + error.message;
        state.statusIsError = true;
        return;
    }

    state.status = std::string(operationName(operation)) + " staged successfully";
    state.statusIsError = false;
}

void drawFillControls(EditToolsPanelState & state)
{
    ImGui::SeparatorText("Fill");
    ImGui::InputInt("Tile ID##Fill", &state.fillId);
    ImGui::SameLine();
    ImGui::Text("[%d, %d]", minimumTileId, maximumTileId);
    ImGui::InputInt("Z##Fill", &state.fillZ);
    ImGui::SameLine();
    ImGui::Text("[%d, %d]", minimumTileZ, maximumTileZ);
}

void drawIdReplacementControls(EditToolsPanelState & state)
{
    ImGui::SeparatorText("Replace tile ID");
    ImGui::InputInt("From ID", &state.replaceIdFrom);
    ImGui::SameLine();
    ImGui::InputInt("To ID", &state.replaceIdTo);
    ImGui::Text("IDs must be in [%d, %d]; Z is preserved", minimumTileId, maximumTileId);
}

void drawZReplacementControls(EditToolsPanelState & state)
{
    ImGui::SeparatorText("Replace Z");
    ImGui::InputInt("From Z", &state.replaceZFrom);
    ImGui::SameLine();
    ImGui::InputInt("To Z", &state.replaceZTo);
    ImGui::Text("Z must be in [%d, %d]; tile ID is preserved", minimumTileZ, maximumTileZ);
}

void drawConfirmationPopup(MapWorkspace & workspace, EditToolsPanelState & state, bool canApply)
{
    if (state.confirmationOperation != EditToolsOperation::none && canApply)
    {
        ImGui::OpenPopup(wholeMapConfirmationPopup);
    }

    if (ImGui::BeginPopupModal(wholeMapConfirmationPopup, nullptr, confirmationFlags))
    {
        ImGui::Text("Apply this operation to the entire map?");
        drawOperationPreview(state, state.confirmationOperation);
        ImGui::Text("Effective scope: Whole map");
        ImGui::BeginDisabled(!canApply || !operationInputsValid(state, state.confirmationOperation));
        if (ImGui::Button("Apply to Whole Map"))
        {
            applyOperation(workspace, state, state.confirmationOperation, MapEditScope::wholeMap);
            state.confirmationOperation = EditToolsOperation::none;
            state.scope = MapEditScope::selection;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndDisabled();
        ImGui::SameLine();

        if (ImGui::Button("Cancel"))
        {
            state.confirmationOperation = EditToolsOperation::none;
            state.scope = MapEditScope::selection;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void drawOperationButton(MapWorkspace & workspace,
                         EditToolsPanelState & state,
                         EditToolsOperation operation,
                         const char * label,
                         bool valid,
                         bool canApply,
                         bool hasSelection)
{
    const bool selectionBlocked = state.scope == MapEditScope::selection && !hasSelection;
    const bool wholeMapBlocked = state.scope == MapEditScope::wholeMap && !operationInputsValid(state, operation);
    ImGui::BeginDisabled(!canApply || !valid || selectionBlocked || wholeMapBlocked);

    if (ImGui::Button(label))
    {
        if (state.scope == MapEditScope::wholeMap)
        {
            state.confirmationOperation = operation;
        }
        else
        {
            applyOperation(workspace, state, operation, MapEditScope::selection);
        }
    }

    ImGui::EndDisabled();
}

void drawWarnings(const EditToolsPanelState & state, bool canApply, bool hasSelection)
{
    if (!inputsValid(state))
    {
        ImGui::TextColored(statusColor(true), "One or more values are outside the supported ID/Z ranges.");
    }

    if (!canApply)
    {
        ImGui::TextColored(statusColor(true), "Editing requires an open map and Edit mode ON.");
    }

    if (state.scope == MapEditScope::selection && !hasSelection)
    {
        ImGui::TextColored(statusColor(true), "Select a non-empty region to use selection scope.");
    }

    if (!state.status.empty())
    {
        ImGui::TextColored(statusColor(state.statusIsError), "%s", state.status.c_str());
    }
}

void drawEditToolsContents(MapWorkspace & workspace, EditToolsPanelState & state)
{
    const bool mapOpen = workspace.reader() != nullptr;
    const bool editEnabled = workspace.editMode();
    const bool hasSelection = selectionAvailable(workspace);
    const bool canApply = mapOpen && editEnabled;

    if (state.confirmationOperation != EditToolsOperation::none && !canApply)
    {
        state.confirmationOperation = EditToolsOperation::none;
    }

    ImGui::Text("Edit mode: %s", editEnabled ? "ON" : "OFF");
    ImGui::Text("Map: %s", mapOpen ? workspace.mapPath().string().c_str() : "No map open");
    drawSelectionDetails(workspace);

    ImGui::SeparatorText("Scope");
    const bool selectionScope = state.scope == MapEditScope::selection;

    if (ImGui::RadioButton("Current selection", selectionScope))
    {
        state.scope = MapEditScope::selection;
    }

    ImGui::SameLine();

    if (ImGui::RadioButton("Whole map", !selectionScope))
    {
        state.scope = MapEditScope::wholeMap;
    }

    ImGui::Text("Effective scope: %s",
                state.scope == MapEditScope::selection ? selectionStyleName(workspace.selectionMode()) : "Whole map");

    drawFillControls(state);
    drawOperationPreview(state, EditToolsOperation::fill);
    const bool fillValid = state.fillId >= minimumTileId && state.fillId <= maximumTileId && state.fillZ >= minimumTileZ &&
                           state.fillZ <= maximumTileZ;
    drawOperationButton(workspace, state, EditToolsOperation::fill, "Fill", fillValid, canApply, hasSelection);

    drawIdReplacementControls(state);
    drawOperationPreview(state, EditToolsOperation::replaceTileId);
    const bool idValid = state.replaceIdFrom >= minimumTileId && state.replaceIdFrom <= maximumTileId &&
                         state.replaceIdTo >= minimumTileId && state.replaceIdTo <= maximumTileId;
    drawOperationButton(workspace, state, EditToolsOperation::replaceTileId, "Replace ID", idValid, canApply, hasSelection);

    drawZReplacementControls(state);
    drawOperationPreview(state, EditToolsOperation::replaceZ);
    const bool zValid = state.replaceZFrom >= minimumTileZ && state.replaceZFrom <= maximumTileZ &&
                        state.replaceZTo >= minimumTileZ && state.replaceZTo <= maximumTileZ;
    drawOperationButton(workspace, state, EditToolsOperation::replaceZ, "Replace Z", zValid, canApply, hasSelection);

    drawWarnings(state, canApply, hasSelection);

    drawConfirmationPopup(workspace, state, canApply);
}

} // namespace

void drawEditToolsPanel(MapWorkspace & workspace, EditToolsPanelState & state, bool & visible)
{
    ImGui::Begin("Edit Tools", &visible);
    drawEditToolsContents(workspace, state);
    ImGui::End();
}

} // namespace uo_mule::gui
