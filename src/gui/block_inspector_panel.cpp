#include "gui/block_inspector_panel.h"

#include <imgui.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include "gui/map_workspace.h"
#include "uomul/map.h"

namespace im_cpp::gui
{
namespace
{

constexpr int blockGridColumns = 8;
constexpr float errorColorChannel = 0.4F;

void drawBlock(const uomul::MapBlock & block, uomul::BlockCoord blockCoordinate, MapWorkspace & workspace)
{
    if (ImGui::BeginTable("BlockTiles", blockGridColumns, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame))
    {
        for (std::size_t row = 0; row < uomul::mapBlockDimension; ++row)
        {
            ImGui::TableNextRow();
            for (std::size_t column = 0; column < uomul::mapBlockDimension; ++column)
            {
                const std::size_t index = (row * uomul::mapBlockDimension) + column;
                const uomul::MapTile tile = block.at(index);
                ImGui::TableSetColumnIndex(static_cast<int>(column));
                const uomul::TileCoord tileCoordinate{
                    .x = (blockCoordinate.x * uomul::mapBlockDimension) + static_cast<std::uint32_t>(column),
                    .y = (blockCoordinate.y * uomul::mapBlockDimension) + static_cast<std::uint32_t>(row),
                };
                const bool selected = workspace.selectedTile() && workspace.selectedTile()->x == tileCoordinate.x &&
                                      workspace.selectedTile()->y == tileCoordinate.y;
                const std::string label = std::to_string(tile.id) + " / " + std::to_string(static_cast<int>(tile.z));
                if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_None, ImVec2(0.0F, 0.0F)))
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
        }
        ImGui::EndTable();
    }
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
    ImGui::TextColored(ImVec4(1.0F, errorColorChannel, errorColorChannel, 1.0F), "Block read error: %s", error.message.c_str());
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
    if (reader != nullptr && workspace.hoveredTile())
    {
        const uomul::BlockCoord coordinate = tileToBlock(*workspace.hoveredTile());
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
            ImGui::TextColored(ImVec4(1.0F, errorColorChannel, errorColorChannel, 1.0F), "%s", message.c_str());
        }
    }
    ImGui::End();
}

} // namespace im_cpp::gui
