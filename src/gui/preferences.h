#pragma once

#include <filesystem>

namespace uo_mule::gui
{

struct Preferences
{
    bool showLogPanel = false;
    bool autoScroll = true;
    bool logPanelCollapsed = false;
    float logPanelHeight = 0.0F;
};

[[nodiscard]] Preferences loadPreferences();
void savePreferences(const Preferences & preferences);

} // namespace uo_mule::gui
