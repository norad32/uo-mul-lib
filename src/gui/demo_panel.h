#pragma once

#include "gui/preferences.h"
#include <imgui.h>
#include <spdlog/logger.h>

namespace uo_mule::gui
{

struct DemoPanelState
{
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
    bool docked = true;
    bool hasWindowGeometry = false;
};

void drawDemoPanel(spdlog::logger & logger,
                   Preferences & preferences,
                   bool & shouldClose,
                   ImGuiID dockSpaceId,
                   float bottomPanelHeight,
                   DemoPanelState & state);

} // namespace uo_mule::gui
