#include "gui_model/dialogs/file_browser_model.h"
#include "core/app_info.h"
#include "gui_model/dialogs/file_deduction.h"
#include "gui_model/persistence/config_paths.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace uo_mule::gui
{

namespace
{

constexpr std::size_t dateBufferSize = 64;
constexpr std::size_t maxRecentLocations = 12;

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

[[nodiscard]] std::string lowercase(std::string value)
{
    std::ranges::transform(
        value, value.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
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
        return "Unknown";
    }
    return buffer.data();
}

} // namespace

std::filesystem::path normalizeFileBrowserDirectory(const std::filesystem::path & path)
{
    std::error_code error;
    auto absolute = std::filesystem::absolute(path, error).lexically_normal();
    if (!error && isDirectory(absolute))
    {
        return absolute;
    }
    return std::filesystem::current_path(error);
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
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error)
    {
        return;
    }
    std::ofstream output(path, std::ios::trunc);
    for (const auto & folder : folders)
    {
        output << folder.string() << '\n';
    }
}

void recordRecentFolder(std::vector<std::filesystem::path> & folders, const std::filesystem::path & folder)
{
    const auto normalized = normalizeFileBrowserDirectory(folder);
    const auto removed = std::ranges::remove(folders, normalized);
    folders.erase(removed.begin(), removed.end());
    folders.insert(folders.begin(), normalized);
    if (folders.size() > maxRecentLocations)
    {
        folders.resize(maxRecentLocations);
    }
}

bool fileBrowserMatchesFilter(const std::filesystem::path & path, std::string_view extension)
{
    if (extension.empty())
    {
        return true;
    }
    const auto actual = lowercase(path.extension().string());
    const auto filters = lowercase(std::string(extension));
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

FileBrowserSummary inspectFileBrowserSelection(const std::filesystem::path & path)
{
    FileBrowserSummary summary;
    summary.path = path;
    std::error_code error;
    summary.exists = std::filesystem::exists(path, error) && !error;
    summary.regularFile = summary.exists && std::filesystem::is_regular_file(path, error) && !error;
    if (!summary.exists)
    {
        summary.error = error ? error.message() : "File does not exist";
        return summary;
    }
    summary.size = std::filesystem::file_size(path, error);
    if (error)
    {
        summary.error = error.message();
    }
    const auto modified = std::filesystem::last_write_time(path, error);
    summary.modified = error ? "Unknown" : formatModifiedTime(modified);
    const std::ifstream readable(path);
    summary.readable = readable.good();

    if (!summary.regularFile || lowercase(path.extension().string()) != ".mul")
    {
        return summary;
    }

    deduceFileCandidates(summary, path);
    sortFileBrowserCandidates(summary.assetCandidates);
    if (summary.mapCandidates.empty() && summary.assetCandidates.empty())
    {
        summary.error = "No known facet dimensions match this MUL file; choose Custom dimensions";
    }
    return summary;
}

} // namespace uo_mule::gui
