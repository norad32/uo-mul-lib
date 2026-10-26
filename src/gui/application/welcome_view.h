#pragma once

namespace uo_mule::gui
{

inline constexpr int welcomeWindowHeight = 800;
inline constexpr int welcomeWindowWidth = 1280;

struct WorkbenchState;

void drawWelcomeView(WorkbenchState & state);

} // namespace uo_mule::gui
