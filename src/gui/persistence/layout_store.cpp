#include "gui/persistence/layout_store.h"
#include "core/app_info.h"
#include "gui_model/layout/panel_registry.h"
#include "gui_model/persistence/config_paths.h"
#include <cstddef>
#include <cstdint>
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

void loadPanel(std::ifstream & file, PanelRegistry & registry, bool validSchema)
{
    std::string stableId;
    bool visible = false;
    file >> std::quoted(stableId) >> visible;
    if (!validSchema)
    {
        return;
    }

    for (auto & panel : registry.panels())
    {
        if (panel.stableId == stableId)
        {
            panel.visible = visible;
        }
    }
}

void loadWindowGeometry(std::ifstream & file, WorkspaceLayout & layout, bool validSchema)
{
    WindowLayout window;
    file >> window.x >> window.y >> window.width >> window.height;
    if (validSchema && window.width > 0 && window.height > 0)
    {
        window.geometryValid = true;
        layout.window = window;
    }
}

void loadDockingIni(std::ifstream & file, WorkspaceLayout & layout, bool validSchema)
{
    std::size_t size = 0;
    file >> size;
    file.get();
    std::string dockingIni(size, '\0');
    file.read(dockingIni.data(), static_cast<std::streamsize>(size));
    const bool hasDelimiter = file.get() != std::char_traits<char>::eof();
    if (validSchema && hasDelimiter)
    {
        layout.dockingIni = std::move(dockingIni);
    }
}

} // namespace

std::filesystem::path layoutPath()
{
    const auto config = configDirectory();
    return config.empty() ? std::filesystem::path{} : config / std::string(core::applicationName()) / "layout.ini";
}

void loadLayout(PanelRegistry & registry, WorkspaceLayout & layout)
{
    const auto path = layoutPath();
    if (path.empty())
    {
        return;
    }

    loadLayout(path, registry, layout);
}

void loadLayout(const std::filesystem::path & path, PanelRegistry & registry, WorkspaceLayout & layout)
{
    if (path.empty())
    {
        return;
    }

    std::ifstream file(path);
    std::string key;
    std::string value;
    bool validSchema = false;
    while (file >> key)
    {
        if (key == "schema_version")
        {
            std::uint32_t version = 0;
            file >> version;
            validSchema = version == currentLayoutSchemaVersion;
        }
        else if (key == "panel")
        {
            loadPanel(file, registry, validSchema);
        }
        else if (key == "window_geometry")
        {
            loadWindowGeometry(file, layout, validSchema);
        }
        else if (key == "docking_ini_size")
        {
            loadDockingIni(file, layout, validSchema);
        }
        else
        {
            std::getline(file, value);
        }
    }
}

void saveLayout(const PanelRegistry & registry, const WorkspaceLayout & layout)
{
    const auto path = layoutPath();
    if (path.empty())
    {
        return;
    }

    saveLayout(path, registry, layout);
}

void saveLayout(const std::filesystem::path & path, const PanelRegistry & registry, const WorkspaceLayout & layout)
{
    if (path.empty())
    {
        return;
    }

    std::filesystem::create_directories(path.parent_path());
    const auto temporary = path.string() + ".tmp";
    std::ofstream file(temporary, std::ios::trunc);
    file << "schema_version " << currentLayoutSchemaVersion << '\n';
    for (const auto & panel : registry.panels())
    {
        file << "panel " << std::quoted(panel.stableId) << ' ' << panel.visible << '\n';
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

} // namespace uo_mule::gui
