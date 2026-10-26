#pragma once

#include <imgui.h>
#include <utility>

namespace uo_mule::gui
{

template <typename DrawContents>
void drawPanelView(const char * panelName, bool & visible, DrawContents && drawContents)
{
    if (!ImGui::Begin(panelName, &visible))
    {
        ImGui::End();
        return;
    }
    std::forward<DrawContents>(drawContents)();
    ImGui::End();
}

} // namespace uo_mule::gui
