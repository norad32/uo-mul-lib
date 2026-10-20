#include "gui/recent_maps.h"
#include "core/app_info.h"
#include "uomul/map.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace uo_mule::gui
{
namespace
{

constexpr std::uint32_t recentMapsSchemaVersion = 3;
constexpr std::uint32_t minimumSupportedSchemaVersion = 1;

[[nodiscard]] std::int64_t currentTimestamp() noexcept
{
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

[[nodiscard]] std::int64_t fileTimestamp(const std::filesystem::path & path) noexcept
{
    std::error_code error;
    const auto fileTime = std::filesystem::last_write_time(path, error);
    if (error)
    {
        return 0;
    }

    const auto systemTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        fileTime - decltype(fileTime)::clock::now() + std::chrono::system_clock::now());
    return std::chrono::duration_cast<std::chrono::seconds>(systemTime.time_since_epoch()).count();
}

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

[[nodiscard]] std::string defaultDisplayName(const std::filesystem::path & path)
{
    const std::string name = path.stem().string();
    return name.empty() ? path.filename().string() : name;
}

void refreshEntryValidity(RecentMap & map)
{
    std::error_code error;
    if (!std::filesystem::is_regular_file(map.path, error))
    {
        map.failure = error ? error.message() : "Map file is missing";
    }
    else
    {
        map.failure.clear();
    }
}

} // namespace

std::filesystem::path recentMapsPath()
{
    const auto config = configDirectory();
    return config.empty() ? std::filesystem::path{} : config / std::string(core::applicationName()) / "recent_maps.conf";
}

std::vector<RecentMap> loadRecentMaps(const std::filesystem::path & path)
{
    std::vector<RecentMap> result;
    std::ifstream file(path);
    std::string key;
    std::uint32_t version = 0;
    while (file >> key)
    {
        if (key == "schema_version")
        {
            file >> version;
            continue;
        }
        if (key != "map")
        {
            std::string ignored;
            std::getline(file, ignored);
            continue;
        }

        RecentMap map;
        std::string mapPath;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        bool hasDimensions = false;
        file >> std::quoted(mapPath) >> std::quoted(map.displayName) >> hasDimensions;
        map.path = mapPath;
        if (hasDimensions)
        {
            file >> width >> height;
            map.dimensions = uomul::Dimensions{.widthTiles = width, .heightTiles = height};
        }
        file >> std::quoted(map.facet) >> std::quoted(map.failure);
        if (version >= recentMapsSchemaVersion)
        {
            file >> map.lastOpened;
            file >> map.lastModified;
        }
        else if (version == 2)
        {
            file >> map.lastOpened;
        }
        if (file && !map.path.empty())
        {
            if (map.displayName.empty())
            {
                map.displayName = defaultDisplayName(map.path);
            }
            if (map.lastModified == 0)
            {
                map.lastModified = fileTimestamp(map.path);
            }
            result.push_back(std::move(map));
        }
    }

    if (version < minimumSupportedSchemaVersion || version > recentMapsSchemaVersion)
    {
        result.clear();
    }
    for (auto & map : result)
    {
        refreshEntryValidity(map);
    }
    if (result.size() > recentFileLimit)
    {
        result.resize(recentFileLimit);
    }
    return result;
}

void saveRecentMaps(const std::vector<RecentMap> & maps, const std::filesystem::path & path)
{
    if (path.empty())
    {
        return;
    }
    std::filesystem::create_directories(path.parent_path());
    const auto temporary = path.string() + ".tmp";
    std::ofstream file(temporary, std::ios::trunc);
    file << "schema_version " << recentMapsSchemaVersion << '\n';
    for (const auto & map : maps)
    {
        file << "map " << std::quoted(map.path.string()) << ' ' << std::quoted(map.displayName) << ' '
             << static_cast<bool>(map.dimensions);
        if (map.dimensions)
        {
            file << ' ' << map.dimensions->widthTiles << ' ' << map.dimensions->heightTiles;
        }
        file << ' ' << std::quoted(map.facet) << ' ' << std::quoted(map.failure) << ' ' << map.lastOpened << ' '
             << map.lastModified << '\n';
    }
    file.close();
    std::filesystem::rename(temporary, path);
}

RecentMapStore::RecentMapStore(std::filesystem::path path) : path_(path.empty() ? recentMapsPath() : std::move(path))
{
    maps_ = loadRecentMaps(path_);
}

const std::vector<RecentMap> & RecentMapStore::maps() const noexcept
{
    return maps_;
}

void RecentMapStore::record(std::filesystem::path path, std::optional<uomul::Dimensions> dimensions, std::string facet)
{
    if (path.empty())
    {
        return;
    }
    const auto existing = std::ranges::find(maps_, path, &RecentMap::path);
    RecentMap entry{
        .path = std::move(path),
        .displayName = {},
        .dimensions = dimensions,
        .facet = std::move(facet),
        .failure = {},
        .lastOpened = currentTimestamp(),
        .lastModified = fileTimestamp(entry.path),
    };
    entry.displayName = defaultDisplayName(entry.path);
    if (existing != maps_.end())
    {
        entry.displayName = existing->displayName.empty() ? entry.displayName : existing->displayName;
        maps_.erase(existing);
    }
    maps_.insert(maps_.begin(), std::move(entry));
    if (maps_.size() > recentFileLimit)
    {
        maps_.resize(recentFileLimit);
    }
    saveRecentMaps(maps_, path_);
}

void RecentMapStore::refreshValidity()
{
    for (auto & map : maps_)
    {
        refreshEntryValidity(map);
    }
}

} // namespace uo_mule::gui
