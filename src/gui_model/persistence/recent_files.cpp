#include "gui_model/persistence/recent_files.h"
#include "core/app_info.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/persistence/config_paths.h"
#include "uomul/map.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace uo_mule::gui
{
namespace
{

constexpr std::uint32_t recentFilesSchemaVersion = 0;

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

[[nodiscard]] std::string defaultDisplayName(const std::filesystem::path & path)
{
    const std::string name = path.stem().string();
    return name.empty() ? path.filename().string() : name;
}

[[nodiscard]] std::optional<DocumentKind> documentKindFromStableName(std::string_view name)
{
    if (name == "map")
    {
        return DocumentKind::map;
    }
    if (name == "radarcol")
    {
        return DocumentKind::radarcol;
    }
    if (name == "texmap_pair")
    {
        return DocumentKind::texmapPair;
    }
    return std::nullopt;
}

void refreshEntryValidity(RecentFile & file)
{
    std::error_code error;
    const bool pathIsFile = std::filesystem::is_regular_file(file.path, error);
    const bool companionIsFile = file.companionPath.empty() || std::filesystem::is_regular_file(file.companionPath, error);
    if (!pathIsFile || !companionIsFile)
    {
        file.failure = error ? error.message() : "File is missing";
    }
    else if (file.kind != DocumentKind::unavailable)
    {
        file.failure.clear();
    }
}

[[nodiscard]] std::optional<RecentFile> loadRecentEntry(std::ifstream & file, std::uint32_t version)
{
    RecentFile result;
    std::string mapPath;
    std::string companionPath;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool hasDimensions = false;
    file >> std::quoted(mapPath) >> std::quoted(result.displayName) >> hasDimensions;
    result.path = mapPath;
    if (hasDimensions)
    {
        file >> width >> height;
        result.dimensions = uomul::Dimensions{.widthTiles = width, .heightTiles = height};
    }
    file >> std::quoted(result.facet) >> std::quoted(result.failure);
    if (version == recentFilesSchemaVersion)
    {
        std::string kind;
        file >> std::quoted(kind) >> result.itemCount >> std::quoted(companionPath);
        if (const auto parsedKind = documentKindFromStableName(kind))
        {
            result.kind = *parsedKind;
        }
        else
        {
            result.kind = DocumentKind::unavailable;
            result.failure = "Unsupported document kind: " + kind;
        }
        result.companionPath = companionPath;
        file >> result.lastOpened >> result.lastModified;
    }
    else if (version == 4)
    {
        std::uint32_t kind = 0;
        file >> kind >> std::quoted(companionPath);
        if (kind <= static_cast<std::uint32_t>(DocumentKind::texmapPair))
        {
            result.kind = static_cast<DocumentKind>(kind);
        }
        result.companionPath = companionPath;
        file >> result.lastOpened >> result.lastModified;
    }
    else if (version == 3)
    {
        file >> result.lastOpened >> result.lastModified;
    }
    else if (version == 2)
    {
        file >> result.lastOpened;
    }
    if (!file || result.path.empty())
    {
        return std::nullopt;
    }
    if (result.displayName.empty())
    {
        result.displayName = defaultDisplayName(result.path);
    }
    if (result.lastModified == 0)
    {
        result.lastModified = fileTimestamp(result.path);
    }
    return result;
}

} // namespace

std::filesystem::path recentFilesPath()
{
    const auto config = configDirectory();
    return config.empty() ? std::filesystem::path{} : config / std::string(core::applicationName()) / "recent_files.conf";
}

std::vector<RecentFile> loadRecentFiles(const std::filesystem::path & path)
{
    std::vector<RecentFile> result;
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
        if (key != "map" && key != "file")
        {
            std::string ignored;
            std::getline(file, ignored);
            continue;
        }

        if (const auto map = loadRecentEntry(file, version))
        {
            result.push_back(*map);
        }
    }

    if (version != recentFilesSchemaVersion)
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

void saveRecentFiles(const std::vector<RecentFile> & files, const std::filesystem::path & path)
{
    if (path.empty())
    {
        return;
    }
    std::filesystem::create_directories(path.parent_path());
    const auto temporary = path.string() + ".tmp";
    std::ofstream file(temporary, std::ios::trunc);
    file << "schema_version " << recentFilesSchemaVersion << '\n';
    for (const auto & map : files)
    {
        file << "map " << std::quoted(map.path.string()) << ' ' << std::quoted(map.displayName) << ' '
             << static_cast<bool>(map.dimensions);
        if (map.dimensions)
        {
            file << ' ' << map.dimensions->widthTiles << ' ' << map.dimensions->heightTiles;
        }
        file << ' ' << std::quoted(map.facet) << ' ' << std::quoted(map.failure) << ' '
             << std::quoted(std::string(documentKindStableName(map.kind))) << ' ' << map.itemCount << ' '
             << std::quoted(map.companionPath.string()) << ' ' << map.lastOpened << ' ' << map.lastModified << '\n';
    }
    file.close();
    std::filesystem::rename(temporary, path);
}

RecentFileStore::RecentFileStore(std::filesystem::path path) : path_(path.empty() ? recentFilesPath() : std::move(path))
{
    files_ = loadRecentFiles(path_);
}

const std::vector<RecentFile> & RecentFileStore::files() const noexcept
{
    return files_;
}

void RecentFileStore::record(std::filesystem::path path,
                             std::optional<uomul::Dimensions> dimensions,
                             std::string facet,
                             DocumentKind kind,
                             std::filesystem::path companionPath,
                             std::uint32_t itemCount)
{
    if (path.empty())
    {
        return;
    }
    const auto existing = std::ranges::find(files_, path, &RecentFile::path);
    RecentFile entry{
        .path = std::move(path),
        .companionPath = std::move(companionPath),
        .displayName = {},
        .kind = kind,
        .itemCount = itemCount,
        .dimensions = dimensions,
        .facet = std::move(facet),
        .failure = {},
        .lastOpened = currentTimestamp(),
        .lastModified = fileTimestamp(entry.path),
    };
    entry.displayName = defaultDisplayName(entry.path);
    if (existing != files_.end())
    {
        entry.displayName = existing->displayName.empty() ? entry.displayName : existing->displayName;
        files_.erase(existing);
    }
    files_.insert(files_.begin(), std::move(entry));
    if (files_.size() > recentFileLimit)
    {
        files_.resize(recentFileLimit);
    }
    saveRecentFiles(files_, path_);
}

void RecentFileStore::refreshValidity()
{
    for (auto & file : files_)
    {
        refreshEntryValidity(file);
    }
}

} // namespace uo_mule::gui
