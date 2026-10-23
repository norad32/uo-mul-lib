#pragma once

#include "gui/views/panel_registry.h"
#include <cstdint>
#include <filesystem>
#include <string>

namespace uo_mule::gui
{

inline constexpr std::uint32_t currentLayoutSchemaVersion = 2;
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

struct WorkspaceLayout
{
    WindowLayout window;
    std::string dockingIni;
};

[[nodiscard]] std::filesystem::path layoutPath();
void loadLayout(PanelRegistry & registry, WorkspaceLayout & layout);
void saveLayout(const PanelRegistry & registry, const WorkspaceLayout & layout);
void loadLayout(const std::filesystem::path & path, PanelRegistry & registry, WorkspaceLayout & layout);
void saveLayout(const std::filesystem::path & path, const PanelRegistry & registry, const WorkspaceLayout & layout);

} // namespace uo_mule::gui
