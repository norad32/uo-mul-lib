#pragma once

#include "gui_model/layout/panel_registry.h"
#include <cstdint>
#include <filesystem>
#include <string>

namespace uo_mule::gui
{

inline constexpr std::uint32_t currentLayoutSchemaVersion = 0;
inline constexpr int defaultWindowWidth = 1280;
inline constexpr int defaultWindowHeight = 800;

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

// The panel registry supplies model-owned semantic visibility and stable IDs.
// This GUI adapter persists ImGui docking data and native window geometry.
[[nodiscard]] std::filesystem::path layoutPath();
void loadLayout(PanelRegistry & registry, AppLayout & layout);
void saveLayout(const PanelRegistry & registry, const AppLayout & layout);
void loadLayout(const std::filesystem::path & path, PanelRegistry & registry, AppLayout & layout);
void saveLayout(const std::filesystem::path & path, const PanelRegistry & registry, const AppLayout & layout);

} // namespace uo_mule::gui
