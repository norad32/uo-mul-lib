#include "gui/application/layout_store.h"
#include "gui_model/layout/panel_registry.h"
#include "gui_model/layout/window_registry.h"
#include "gui_model/profile/data_profile.h"
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <string>
#include <utility>

namespace uo_mule::gui
{
namespace
{

template <typename Registry>
void loadSurface(std::ifstream & file, Registry & registry, auto getSurfaces)
{
    std::string stableId;
    bool visible = false;
    file >> std::quoted(stableId) >> visible;
    for (auto & surface : getSurfaces(registry))
    {
        if (surface.stableId == stableId)
        {
            surface.visible = visible;
        }
    }
}

void loadWindowGeometry(std::ifstream & file, AppLayout & layout)
{
    WindowLayout window;
    file >> window.x >> window.y >> window.width >> window.height;
    if (window.width > 0 && window.height > 0)
    {
        window.geometryValid = true;
        layout.window = window;
    }
}

void loadDockingIni(std::ifstream & file, AppLayout & layout)
{
    std::size_t size = 0;
    file >> size;
    file.get();
    std::string dockingIni(size, '\0');
    file.read(dockingIni.data(), static_cast<std::streamsize>(size));
    const bool hasDelimiter = file.get() != std::char_traits<char>::eof();
    if (hasDelimiter)
    {
        layout.dockingIni = std::move(dockingIni);
    }
}

} // namespace

void loadLayout(const std::filesystem::path & path, PanelRegistry & panels, WindowRegistry & windows, AppLayout & layout)
{
    if (path.empty())
    {
        return;
    }

    std::ifstream file(path);
    std::string key;
    std::string value;
    while (file >> key)
    {
        if (key == "panel")
        {
            loadSurface(file, panels, [](PanelRegistry & registry) -> auto & { return registry.panels(); });
        }
        else if (key == "window")
        {
            loadSurface(file, windows, [](WindowRegistry & registry) -> auto & { return registry.windows(); });
        }
        else if (key == "window_geometry")
        {
            loadWindowGeometry(file, layout);
        }
        else if (key == "docking_ini_size")
        {
            loadDockingIni(file, layout);
        }
        else
        {
            std::getline(file, value);
        }
    }
}

void saveLayout(const std::filesystem::path & path,
                const PanelRegistry & panels,
                const WindowRegistry & windows,
                const AppLayout & layout)
{
    if (path.empty())
    {
        return;
    }

    std::filesystem::create_directories(path.parent_path());
    const auto temporary = path.string() + ".tmp";
    std::ofstream file(temporary, std::ios::trunc);
    file << "schema_version " << currentLayoutSchemaVersion << '\n';
    for (const auto & panel : panels.panels())
    {
        file << "panel " << std::quoted(panel.stableId) << ' ' << panel.visible << '\n';
    }
    for (const auto & window : windows.windows())
    {
        file << "window " << std::quoted(window.stableId) << ' ' << window.visible << '\n';
    }
    if (layout.window.geometryValid)
    {
        file << "window_geometry " << layout.window.x << ' ' << layout.window.y << ' ' << layout.window.width << ' '
             << layout.window.height << '\n';
    }
    file << "docking_ini_size " << layout.dockingIni.size() << '\n';
    file.write(layout.dockingIni.data(), static_cast<std::streamsize>(layout.dockingIni.size()));
    file << '\n';
    file.close();
    std::filesystem::rename(temporary, path);
}

void loadProfileLayout(const ProfileLayout & source, PanelRegistry & panels, WindowRegistry & windows, AppLayout & layout)
{
    layout.window.geometryValid = source.window.geometryValid;
    layout.window.x = source.window.x;
    layout.window.y = source.window.y;
    layout.window.width = source.window.width;
    layout.window.height = source.window.height;
    layout.dockingIni = source.dockingIni;
    for (auto & panel : panels.panels())
    {
        if (const auto visible = source.panels.find(panel.stableId); visible != source.panels.end())
        {
            panel.visible = visible->second;
        }
    }
    for (auto & window : windows.windows())
    {
        if (const auto visible = source.windows.find(window.stableId); visible != source.windows.end())
        {
            window.visible = visible->second;
        }
    }
}

void saveProfileLayout(ProfileLayout & destination,
                       const PanelRegistry & panels,
                       const WindowRegistry & windows,
                       const AppLayout & layout)
{
    destination.window.geometryValid = layout.window.geometryValid;
    destination.window.x = layout.window.x;
    destination.window.y = layout.window.y;
    destination.window.width = layout.window.width;
    destination.window.height = layout.window.height;
    destination.dockingIni = layout.dockingIni;
    destination.panels.clear();
    destination.windows.clear();
    for (const auto & panel : panels.panels())
    {
        destination.panels[panel.stableId] = panel.visible;
    }
    for (const auto & window : windows.windows())
    {
        destination.windows[window.stableId] = window.visible;
    }
}

} // namespace uo_mule::gui
