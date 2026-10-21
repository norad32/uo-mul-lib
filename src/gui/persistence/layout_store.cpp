#include "gui/persistence/layout_store.h"
#include "gui/views/panel_registry.h"
#include "core/app_info.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <cstdint>
#include <string>

namespace uo_mule::gui
{
namespace
{

[[nodiscard]] std::filesystem::path configDirectory()
{
    if (const char * config = std::getenv("XDG_CONFIG_HOME"); config != nullptr && std::filesystem::path(config).is_absolute())
    {
        return config;
    }
    if (const char * home = std::getenv("HOME"); home != nullptr && *home != '\0')
    {
        return std::filesystem::path(home) / ".config";
    }
    return {};
}

} // namespace

std::filesystem::path layoutPath()
{
    const auto config = configDirectory();
    return config.empty() ? std::filesystem::path{} : config / std::string(core::applicationName()) / "layout.ini";
}

void loadLayout(PanelRegistry & registry, std::string & dockingIni)
{
    const auto path = layoutPath();
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
            std::string stableId;
            bool visible = false;
            file >> std::quoted(stableId) >> visible;
            if (!validSchema)
            {
                continue;
            }
            for (auto & panel : registry.panels())
            {
                if (panel.stableId == stableId)
                {
                    panel.visible = visible;
                }
            }
        }
        else if (key == "docking_ini")
        {
            file >> std::quoted(value);
            if (validSchema)
            {
                dockingIni = value;
            }
        }
        else
        {
            std::getline(file, value);
        }
    }
}

void saveLayout(const PanelRegistry & registry, const std::string & dockingIni)
{
    const auto path = layoutPath();
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
    file << "docking_ini " << std::quoted(dockingIni) << '\n';
    file.close();
    std::filesystem::rename(temporary, path);
}

} // namespace uo_mule::gui
