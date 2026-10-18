#include "gui/panels/block_inspector_panel.h"
#include "gui/map_workspace.h"
#include "gui/panels/common.h"
#include "gui/theme.h"
#include "uomul/map.h"
#include "uomul/mul_common/mul_common.h"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <imgui.h>
#include <optional>
#include <string>

namespace uo_mule::gui
{

namespace
{

constexpr float tileCellSize = 40.0F;
constexpr float coordinateColumnWidth = 38.0F;
constexpr float selectedBorderThickness = 2.0F;
constexpr float gridLineThickness = 1.0F;

void drawTileCell(const uomul::MapBlock & block,
                  uomul::BlockCoord blockCoordinate,
                  MapWorkspace & workspace,
                  std::size_t row,
                  std::size_t column,
                  ImVec2 gridOrigin)
{
    const std::size_t index = (row * uomul::mapBlockDimension) + column;
    const uomul::MapTile tile = block.at(index);
    const uomul::TileCoord tileCoordinate{
        .x = (blockCoordinate.x * uomul::mapBlockDimension) + static_cast<std::uint32_t>(column),
        .y = (blockCoordinate.y * uomul::mapBlockDimension) + static_cast<std::uint32_t>(row),
    };
    const bool selected = workspace.selectedTile() && workspace.selectedTile()->x == tileCoordinate.x &&
                          workspace.selectedTile()->y == tileCoordinate.y;
    const MapColor mapColor = tileDisplayColor(workspace.displayMode(), tile);
    const ImVec2 cellMinimum(gridOrigin.x + coordinateColumnWidth + (static_cast<float>(column) * tileCellSize),
                             gridOrigin.y + ImGui::GetTextLineHeightWithSpacing() + (static_cast<float>(row) * tileCellSize));
    const ImVec2 cellMaximum(cellMinimum.x + tileCellSize, cellMinimum.y + tileCellSize);

    ImGui::SetCursorScreenPos(cellMinimum);
    ImGui::PushID(static_cast<int>(index));
    const bool clicked = ImGui::InvisibleButton("##BlockTile", ImVec2(tileCellSize, tileCellSize));
    ImGui::PopID();

    ImDrawList * drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(cellMinimum, cellMaximum, IM_COL32(mapColor.red, mapColor.green, mapColor.blue, 255));
    drawList->AddRect(cellMinimum,
                      cellMaximum,
                      ImGui::ColorConvertFloat4ToU32(theme::imguiColor(theme::stoneBorder)),
                      0.0F,
                      ImDrawFlags_None,
                      gridLineThickness);
    if (selected)
    {
        drawList->AddRect(
            cellMinimum, cellMaximum, theme::packedColor(theme::brightBrass), 0.0F, ImDrawFlags_None, selectedBorderThickness);
    }

    const std::string label = tileLabel(workspace.displayMode(), tile);
    const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
    const ImVec2 textPosition(cellMinimum.x + ((tileCellSize - textSize.x) * 0.5F),
                              cellMinimum.y + ((tileCellSize - textSize.y) * 0.5F));
    drawOutlinedText(*drawList, textPosition, label);

    if (clicked)
    {
        const auto result = workspace.updateSelectedTile(tileCoordinate);
        static_cast<void>(result);
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Tile (%u, %u): ID %u, Z %d",
                          tileCoordinate.x,
                          tileCoordinate.y,
                          static_cast<unsigned int>(tile.id),
                          static_cast<int>(tile.z));
    }
}

void drawBlock(const uomul::MapBlock & block, uomul::BlockCoord blockCoordinate, MapWorkspace & workspace)
{
    const ImVec2 gridOrigin = ImGui::GetCursorScreenPos();
    ImDrawList * drawList = ImGui::GetWindowDrawList();
    const float axisTextY = gridOrigin.y;
    drawList->AddText(ImVec2(gridOrigin.x, axisTextY), ImGui::GetColorU32(ImGuiCol_Text), "Y/X");

    for (std::size_t column = 0; column < uomul::mapBlockDimension; ++column)
    {
        const std::string coordinate =
            std::to_string((blockCoordinate.x * uomul::mapBlockDimension) + static_cast<std::uint32_t>(column));
        const ImVec2 textSize = ImGui::CalcTextSize(coordinate.c_str());
        const float x = gridOrigin.x + coordinateColumnWidth + (static_cast<float>(column) * tileCellSize) +
                        ((tileCellSize - textSize.x) * 0.5F);
        drawList->AddText(ImVec2(x, axisTextY), ImGui::GetColorU32(ImGuiCol_Text), coordinate.c_str());
    }

    for (std::size_t row = 0; row < uomul::mapBlockDimension; ++row)
    {
        const std::string coordinate =
            std::to_string((blockCoordinate.y * uomul::mapBlockDimension) + static_cast<std::uint32_t>(row));
        const ImVec2 textSize = ImGui::CalcTextSize(coordinate.c_str());
        const float y = gridOrigin.y + ImGui::GetTextLineHeightWithSpacing() + (static_cast<float>(row) * tileCellSize) +
                        ((tileCellSize - textSize.y) * 0.5F);
        drawList->AddText(ImVec2(gridOrigin.x + coordinateColumnWidth - textSize.x - ImGui::GetStyle().ItemSpacing.x, y),
                          ImGui::GetColorU32(ImGuiCol_Text),
                          coordinate.c_str());
        for (std::size_t column = 0; column < uomul::mapBlockDimension; ++column)
        {
            drawTileCell(block, blockCoordinate, workspace, row, column, gridOrigin);
        }
    }

    const float gridHeight =
        ImGui::GetTextLineHeightWithSpacing() + (static_cast<float>(uomul::mapBlockDimension) * tileCellSize);
    ImGui::SetCursorScreenPos(ImVec2(gridOrigin.x, gridOrigin.y + gridHeight));
    ImGui::Dummy(ImVec2(coordinateColumnWidth + (static_cast<float>(uomul::mapBlockDimension) * tileCellSize), 0.0F));
}

void refreshBlock(const uomul::MapReader & reader,
                  const std::filesystem::path & path,
                  uomul::BlockCoord coordinate,
                  BlockInspectorPanelState & state)
{
    state.mapPath = path;
    state.blockCoordinate = coordinate;
    state.block.reset();
    state.error.reset();
    state.cacheValid = true;
    auto result = reader.readBlock(coordinate);

    if (result)
    {
        state.block = *result;
    }
    else
    {
        state.error = result.error();
    }
}

void drawBlockError(const uomul::Error & error)
{
    ImGui::TextColored(theme::imguiColor(theme::coral), "Block read error: %s", error.message.c_str());
}

} // namespace

void drawBlockInspectorPanel(MapWorkspace & workspace, BlockInspectorPanelState & state, bool & visible)
{
    const uomul::MapReader * reader = workspace.reader();
    if (reader == nullptr || (state.cacheValid && state.mapPath != workspace.mapPath()))
    {
        state.cacheValid = false;
        state.block.reset();
        state.error.reset();
        state.blockCoordinate.reset();
    }

    const std::optional<uomul::TileCoord> inspectedTile =
        workspace.hoveredTile() ? workspace.hoveredTile() : workspace.selectedTile();

    if (reader != nullptr && inspectedTile)
    {
        const uomul::BlockCoord coordinate = tileToBlock(*inspectedTile);

        if (!state.cacheValid || !state.blockCoordinate || state.blockCoordinate->x != coordinate.x ||
            state.blockCoordinate->y != coordinate.y)
        {
            refreshBlock(*reader, workspace.mapPath(), coordinate, state);
        }
    }

    const bool contentsVisible = ImGui::Begin("Block Inspector", &visible);
    if (contentsVisible)
    {
        if (reader == nullptr)
        {
            ImGui::TextUnformatted("No map open");
        }
        else if (!state.blockCoordinate)
        {
            ImGui::TextUnformatted("Hover a block in the map canvas to inspect it");
        }
        else
        {
            const uomul::BlockCoord blockCoordinate = *state.blockCoordinate;
            ImGui::Text("Block: (%u, %u)", blockCoordinate.x, blockCoordinate.y);
            ImGui::Text("Tile coordinates: X %u-%u, Y %u-%u",
                        blockCoordinate.x * uomul::mapBlockDimension,
                        (blockCoordinate.x * uomul::mapBlockDimension) + uomul::mapBlockDimension - 1U,
                        blockCoordinate.y * uomul::mapBlockDimension,
                        (blockCoordinate.y * uomul::mapBlockDimension) + uomul::mapBlockDimension - 1U);
            if (state.block)
            {
                drawBlock(*state.block, blockCoordinate, workspace);
                if (workspace.selectedTile() && workspace.selectedValue())
                {
                    const uomul::TileCoord tileCoordinate = *workspace.selectedTile();
                    const uomul::MapTile tile = *workspace.selectedValue();
                    ImGui::Text("Selected tile (%u, %u): ID %u, Z %d",
                                tileCoordinate.x,
                                tileCoordinate.y,
                                static_cast<unsigned int>(tile.id),
                                static_cast<int>(tile.z));
                }
            }
            else if (state.error)
            {
                drawBlockError(*state.error);
            }
        }

        if (workspace.error())
        {
            const std::string message = "Workspace error: " + workspace.error()->message;
            ImGui::TextColored(theme::imguiColor(theme::coral), "%s", message.c_str());
        }
    }

    ImGui::End();
}

} // namespace uo_mule::gui
