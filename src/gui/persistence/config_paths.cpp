#include "gui/persistence/config_paths.h"
#include <cstdlib>
#include <filesystem>

namespace uo_mule::gui
{

std::filesystem::path configDirectory()
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

} // namespace uo_mule::gui
