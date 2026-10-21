#pragma once

#include <string>

namespace uo_mule::gui
{

struct Preferences
{
    bool showLogPanel = false;
    bool autoScroll = true;
    bool logPanelCollapsed = false;
    float logPanelHeight = 0.0F;
    std::string activeDataProfile;
    bool showWelcomeOnStartup = true;
};

[[nodiscard]] Preferences loadPreferences();
void savePreferences(const Preferences & preferences);

} // namespace uo_mule::gui
