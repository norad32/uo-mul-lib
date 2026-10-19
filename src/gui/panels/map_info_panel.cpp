#include "gui/panels/map_info_panel.h"
#include "gui/map_workspace.h"
#include "gui/theme.h"
#include "uomul/map.h"
#include <imgui.h>
#include <string>

namespace uo_mule::gui
{

void drawMapInfoPanel(const MapWorkspace & workspace, bool & visible)
{
    const bool contentsVisible = ImGui::Begin("Map Information###view.map-information", &visible);

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
            ImGui::TextColored(workspace.dirty() ? theme::imguiColor(theme::amber) : ImGui::GetStyleColorVec4(ImGuiCol_Text),
                               "Edit state: %s",
                               workspace.dirty() ? "Unsaved staged changes" : "Clean");
            ImGui::Text("Edit mode: %s", workspace.editMode() ? "Enabled" : "Disabled");
        }

        if (workspace.error())
        {
            ImGui::TextColored(theme::imguiColor(theme::coral), "Map error: %s", workspace.error()->message.c_str());
        }
    }

    ImGui::End();
}

} // namespace uo_mule::gui
