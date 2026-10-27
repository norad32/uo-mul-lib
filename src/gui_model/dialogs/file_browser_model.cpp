#include "gui_model/dialogs/file_browser_model.h"
#include "core/app_info.h"
#include "gui_model/dialogs/file_deduction.h"
#include "gui_model/persistence/atomic_file.h"
#include "gui_model/persistence/config_paths.h"
#include "gui_model/persistence/schema_validation.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace uo_mule::gui
{

namespace
{

constexpr const char * unknownFileBrowserValue = "Unknown";
constexpr const char * fileDoesNotExistMessage = "File does not exist";
constexpr const char * noKnownFacetDimensionsMessage =
    "No known facet dimensions match this MUL file; choose Custom dimensions";

constexpr std::size_t dateBufferSize = 64;
constexpr std::size_t maxRecentLocations = 12;
constexpr std::uint32_t recentFoldersSchemaVersion = 0;

[[nodiscard]] std::filesystem::path recentFoldersPath()
{
    const std::filesystem::path directory = configDirectory();
    if (directory.empty())
    {
        return {};
    }
    return directory / std::string(core::applicationName()) / "file_browser_folders.conf";
}

[[nodiscard]] bool isDirectory(const std::filesystem::path & path)
{
    std::error_code error;
    return std::filesystem::is_directory(path, error) && !error;
}

[[nodiscard]] std::string formatModifiedTime(const std::filesystem::file_time_type & value)
{
    const auto systemTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        value - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    const std::time_t time = std::chrono::system_clock::to_time_t(systemTime);
    std::tm local{};
    if (const auto * converted = std::localtime(&time); converted != nullptr)
    {
        local = *converted;
    }

    std::array<char, dateBufferSize> buffer{};
    if (std::strftime(buffer.data(), buffer.size(), "%Y-%m-%d %H:%M:%S", &local) == 0)
    {
        return unknownFileBrowserValue;
    }
    return buffer.data();
}

} // namespace

std::string normalizeFileBrowserText(std::string value)
{
    std::ranges::transform(
        value, value.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

std::filesystem::path normalizeFileBrowserDirectory(const std::filesystem::path & path)
{
    std::error_code error;
    const auto absolute = std::filesystem::absolute(path, error);
    if (error)
    {
        return {};
    }
    return absolute.lexically_normal();
}

bool isFileBrowserDirectory(const std::filesystem::path & path)
{
    return isDirectory(path);
}

std::vector<std::filesystem::path> loadRecentFolders()
{
    std::vector<std::filesystem::path> folders;
    const auto path = recentFoldersPath();
    if (path.empty())
    {
        return folders;
    }
    std::ifstream input(path);
    std::string schemaLine;
    if (!std::getline(input, schemaLine) || !hasSchemaVersion(schemaLine, recentFoldersSchemaVersion))
    {
        return folders;
    }

    std::string value;
    while (std::getline(input, value) && folders.size() < maxRecentLocations)
    {
        if (!value.empty())
        {
            folders.emplace_back(value);
        }
    }
    return folders;
}

void saveRecentFolders(const std::vector<std::filesystem::path> & folders)
{
    const auto path = recentFoldersPath();
    if (path.empty())
    {
        return;
    }
    std::ostringstream contents;
    contents << "schema_version " << recentFoldersSchemaVersion << '\n';
    for (const auto & folder : folders)
    {
        contents << folder.string() << '\n';
    }
    writeFileAtomically(path, contents.str(), "recent folders");
}

bool recordRecentFolder(std::vector<std::filesystem::path> & folders, const std::filesystem::path & folder)
{
    const auto normalized = normalizeFileBrowserDirectory(folder);
    if (normalized.empty() || !isDirectory(normalized))
    {
        return false;
    }
    if (!folders.empty() && folders.front() == normalized)
    {
        return false;
    }
    const auto removed = std::ranges::remove(folders, normalized);
    folders.erase(removed.begin(), removed.end());
    folders.insert(folders.begin(), normalized);
    if (folders.size() > maxRecentLocations)
    {
        folders.resize(maxRecentLocations);
    }
    return true;
}

bool fileBrowserMatchesFilter(const std::filesystem::path & path, std::string_view extension)
{
    if (extension.empty())
    {
        return true;
    }
    const auto actual = normalizeFileBrowserText(path.extension().string());
    const auto filters = normalizeFileBrowserText(std::string(extension));
    std::size_t begin = 0;
    while (begin < filters.size())
    {
        const auto end = filters.find_first_of(",;", begin);
        const auto candidate = filters.substr(begin, end == std::string::npos ? end : end - begin);
        if (actual == candidate)
        {
            return true;
        }
        if (end == std::string::npos)
        {
            break;
        }
        begin = end + 1U;
    }
    return false;
}

FileBrowserEntryMetadata inspectFileBrowserEntry(const std::filesystem::path & path)
{
    FileBrowserEntryMetadata metadata;
    std::error_code error;
    metadata.exists = std::filesystem::exists(path, error) && !error;
    if (!metadata.exists)
    {
        metadata.modified = unknownFileBrowserValue;
        return metadata;
    }

    error.clear();
    metadata.directory = std::filesystem::is_directory(path, error) && !error;
    error.clear();
    metadata.regularFile = std::filesystem::is_regular_file(path, error) && !error;
    if (metadata.regularFile)
    {
        error.clear();
        const auto size = std::filesystem::file_size(path, error);
        if (!error)
        {
            metadata.size = size;
        }
    }

    error.clear();
    const auto modified = std::filesystem::last_write_time(path, error);
    metadata.modified = error ? unknownFileBrowserValue : formatModifiedTime(modified);
    return metadata;
}

FileBrowserSummary inspectFileBrowserSelection(const std::filesystem::path & path)
{
    FileBrowserSummary summary;
    summary.path = path;
    std::error_code error;
    summary.exists = std::filesystem::exists(path, error) && !error;
    summary.regularFile = summary.exists && std::filesystem::is_regular_file(path, error) && !error;
    if (!summary.exists)
    {
        summary.error = error ? error.message() : fileDoesNotExistMessage;
        return summary;
    }
    const auto metadata = inspectFileBrowserEntry(path);
    summary.size = metadata.size;
    summary.modified = metadata.modified;
    if (!summary.size)
    {
        error.clear();
        static_cast<void>(std::filesystem::file_size(path, error));
        if (error)
        {
            summary.error = error.message();
        }
    }
    const std::ifstream readable(path);
    summary.readable = readable.good();

    if (!summary.regularFile || normalizeFileBrowserText(path.extension().string()) != ".mul")
    {
        return summary;
    }

    deduceFileCandidates(summary, path);
    sortFileBrowserCandidates(summary.assetCandidates);
    if (summary.mapCandidates.empty() && summary.assetCandidates.empty())
    {
        summary.error = noKnownFacetDimensionsMessage;
    }
    return summary;
}

} // namespace uo_mule::gui
