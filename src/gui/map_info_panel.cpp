#include "gui/map_info_panel.h"

#include <imgui.h>

#include <string>

#include "gui/map_workspace.h"
#include "uomul/map.h"

namespace im_cpp::gui
{
namespace
{

constexpr float dirtyRedChannel = 1.0F;
constexpr float dirtyGreenChannel = 0.75F;
constexpr float dirtyBlueChannel = 0.25F;
constexpr float errorColorChannel = 0.4F;

} // namespace

void drawMapInfoPanel(const MapWorkspace & workspace, bool & visible)
{
    const bool contentsVisible = ImGui::Begin("Map Information", &visible);
    if (contentsVisible)
    {
        const uomul::MapReader * reader = workspace.reader();
        if (reader == nullptr)
        {
            ImGui::TextUnformatted("No map open");
        }
        else
        {
            const uomul::Dimensions dimensions = workspace.dimensions();
            const std::string sourcePath = workspace.originalPath().string();
            ImGui::Text("Dimensions: %u x %u tiles", dimensions.widthTiles, dimensions.heightTiles);
            ImGui::Text("Source: %s", sourcePath.c_str());
            if (workspace.workingPath())
            {
                const std::string workingPath = workspace.workingPath()->string();
                ImGui::Text("Staged working copy: %s", workingPath.c_str());
            }
            ImGui::Text("Complete blocks: %llu", static_cast<unsigned long long>(reader->completeBlocks()));
            ImGui::Text("Missing blocks: %llu", static_cast<unsigned long long>(reader->missingBlocks()));
            ImGui::Text("Trailing bytes: %llu", static_cast<unsigned long long>(reader->ignoredTrailingBytes()));
            ImGui::TextColored(workspace.dirty() ? ImVec4(dirtyRedChannel, dirtyGreenChannel, dirtyBlueChannel, 1.0F)
                                                 : ImGui::GetStyleColorVec4(ImGuiCol_Text),
                               "Edit state: %s",
                               workspace.dirty() ? "Unsaved staged changes" : "Clean");
            ImGui::Text("Edit mode: %s", workspace.editMode() ? "Enabled" : "Disabled");
        }
        if (workspace.error())
        {
            ImGui::TextColored(ImVec4(dirtyRedChannel, errorColorChannel, errorColorChannel, 1.0F),
                               "Map error: %s",
                               workspace.error()->message.c_str());
        }
    }
    ImGui::End();
}

} // namespace im_cpp::gui
