#pragma once

#include "gui_model/layout/panel_registry.h"
#include "gui_model/layout/window_registry.h"
#include "gui_model/profile/data_profile.h"
#include <cstdint>
#include <filesystem>
#include <string>

namespace uo_mule::gui
{

inline constexpr std::uint32_t currentLayoutSchemaVersion = 0;
inline constexpr int defaultWindowWidth = 1280;
inline constexpr int defaultWindowHeight = 1000;

struct WindowLayout
{
    bool geometryValid = false;
    int x = 0;
    int y = 0;
    int width = defaultWindowWidth;
    int height = defaultWindowHeight;
};

struct AppLayout
{
    WindowLayout window;
    std::string dockingIni;
};

// The panel and window registries supply model-owned semantic visibility and stable IDs.
// This GUI adapter persists ImGui docking data and native window geometry.
void loadLayout(const std::filesystem::path & path, PanelRegistry & panels, WindowRegistry & windows, AppLayout & layout);
void saveLayout(const std::filesystem::path & path,
                const PanelRegistry & panels,
                const WindowRegistry & windows,
                const AppLayout & layout);
void loadProfileLayout(const ProfileLayout & source, PanelRegistry & panels, WindowRegistry & windows, AppLayout & layout);
void saveProfileLayout(ProfileLayout & destination,
                       const PanelRegistry & panels,
                       const WindowRegistry & windows,
                       const AppLayout & layout);

} // namespace uo_mule::gui
