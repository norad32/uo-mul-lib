#include "gui_model/dialogs/file_browser_model.h"
#include "core/app_info.h"
#include "gui_model/dialogs/dialog_contracts.h"
#include "gui_model/persistence/config_paths.h"
#include "uomul/facet_sizes.h"
#include "uomul/map/map.h"
#include "uomul/radarcol/radarcol.h"
#include "uomul/texmaps/texmaps.h"
#include "uomul/tiledata/tiledata.h"
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
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace uo_mule::gui
{

namespace
{

constexpr std::size_t dateBufferSize = 64;
constexpr std::size_t maxRecentLocations = 12;
constexpr int mapCandidateScore = 80;
constexpr int mapFilenameScore = 110;
constexpr int radarcolNamedScore = 100;
constexpr int radarcolFilenameScore = 80;
constexpr int radarcolClassicSizeScore = 60;
constexpr int radarcolStructuralScore = 20;
constexpr int texmapsPairScore = 90;
constexpr int tiledataNamedScore = 100;
constexpr int tiledataSizeScore = 60;
constexpr int filenameHintScoreBonus = 10;
constexpr std::uintmax_t classicRadarcolByteCount = static_cast<std::uintmax_t>(uomul::radarColorCount) * 2U;

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

[[nodiscard]] bool filenameContains(std::string_view filename, std::string_view token)
{
    return filename.contains(token);
}

[[nodiscard]] std::filesystem::path texmapsCompanionPath(const std::filesystem::path & path, bool selectedIndex)
{
    const std::string stem = path.stem().string();
    constexpr std::string_view indexPrefix = "texidx";
    constexpr std::string_view dataPrefix = "texmaps";
    const std::string_view selectedPrefix = selectedIndex ? indexPrefix : dataPrefix;
    const std::string suffix = stem.starts_with(selectedPrefix) ? stem.substr(selectedPrefix.size()) : "";
    const std::string companion = std::string(selectedIndex ? dataPrefix : indexPrefix) + suffix + path.extension().string();
    return path.parent_path() / companion;
}

void appendMapCandidates(FileBrowserSummary & summary, const std::filesystem::path & path)
{
    const auto exactFacets = summary.size ? uomul::facetSizesForMapFileBytes(*summary.size) : std::vector<uomul::FacetSize>{};
    for (const auto & facet : exactFacets)
    {
        const auto dimensions = uomul::Dimensions{.widthTiles = facet.widthTiles, .heightTiles = facet.heightTiles};
        auto reader = uomul::MapReader::open(path, dimensions);
        if (!reader)
        {
            continue;
        }

        summary.mapCandidates.push_back(FileBrowserMapCandidate{
            .presetName = std::string(facet.name),
            .facet = std::string(facet.mapLabel),
            .dimensions = dimensions,
            .expectedBlocks = reader->completeBlocks(),
            .completeBlocks = reader->completeBlocks(),
            .missingBlocks = reader->missingBlocks(),
            .trailingBytes = reader->ignoredTrailingBytes(),
            .valid = true,
        });
        summary.assetCandidates.push_back(FileBrowserAssetCandidate{
            .kind = MulAssetKind::map,
            .path = path,
            .companionPath = {},
            .label = "Map",
            .facet = std::string(facet.mapLabel),
            .reason = "Valid map layout for " + std::to_string(dimensions.widthTiles) + " x " +
                      std::to_string(dimensions.heightTiles) + " tiles",
            .warning = {},
            .score = filenameContains(lowercase(path.filename().string()), "map") ? mapFilenameScore : mapCandidateScore,
            .dimensions = dimensions,
            .validated = true,
            .openable = true,
        });
    }
}

void appendRadarcolCandidate(FileBrowserSummary & summary, const std::filesystem::path & path, std::string_view filename)
{
    if (!summary.readable)
    {
        return;
    }

    const bool namedRadarcol = filename == "radarcol.mul";
    const bool radarcolFilename = filenameContains(filename, "radarcol");
    const bool classicRadarcolSize = summary.size && *summary.size == classicRadarcolByteCount;
    const auto reader = uomul::RadarColReader::open(path);
    if (!reader)
    {
        return;
    }

    const std::string reason = std::to_string(reader->colorCount()) + " entries";
    std::string warning;
    int score = radarcolStructuralScore;
    if (namedRadarcol)
    {
        score = radarcolNamedScore;
    }
    else if (classicRadarcolSize)
    {
        score = radarcolClassicSizeScore;
    }
    else if (radarcolFilename)
    {
        score = radarcolFilenameScore;
    }
    else
    {
        warning = "Unexpected filename";
    }

    summary.assetCandidates.push_back(FileBrowserAssetCandidate{
        .kind = MulAssetKind::radarcol,
        .path = path,
        .companionPath = {},
        .label = "Radarcol",
        .facet = {},
        .reason = reason,
        .warning = std::move(warning),
        .score = score,
        .itemCount = reader->colorCount(),
        .dimensions = std::nullopt,
        .validated = true,
        .openable = true,
    });
}

void appendTexmapsCandidate(FileBrowserSummary & summary, const std::filesystem::path & path, std::string_view filename)
{
    const bool selectedIsIndex = filename.starts_with("texidx") && filename.ends_with(".mul");
    const bool selectedIsData = filename.starts_with("texmaps") && filename.ends_with(".mul");
    const bool filenameHint =
        filenameContains(filename, "tex") || filenameContains(filename, "maps") || filenameContains(filename, "idx");
    if (!selectedIsIndex && !selectedIsData && !filenameHint)
    {
        return;
    }
    const std::filesystem::path indexPath = selectedIsIndex ? path : texmapsCompanionPath(path, false);
    const std::filesystem::path dataPath = selectedIsData ? path : texmapsCompanionPath(path, true);
    auto pair = inspectTexmapsPair(indexPath, dataPath);
    std::error_code indexError;
    const bool hasIndex = std::filesystem::is_regular_file(indexPath, indexError);
    indexError.clear();
    const bool hasData = std::filesystem::is_regular_file(dataPath, indexError);
    if (!selectedIsIndex && !selectedIsData && !hasIndex && !hasData)
    {
        return;
    }

    pair.label = "Texmaps";
    pair.reason = pair.openable ? std::to_string(pair.itemCount) + " entries" : pair.reason;
    if (!selectedIsIndex && !selectedIsData)
    {
        pair.warning = "Unexpected filename";
    }
    else if (!hasIndex)
    {
        pair.warning = "texidx.mul file is not set";
    }
    else if (!hasData)
    {
        pair.warning = "texmaps.mul file is not set";
    }
    if (pair.openable)
    {
        summary.assetCandidates.push_back(pair);
        return;
    }

    summary.assetCandidates.push_back(FileBrowserAssetCandidate{
        .kind = MulAssetKind::texmapsPair,
        .path = path,
        .companionPath = {},
        .label = "Texmaps",
        .facet = {},
        .reason = "A matching Texmaps file set must be set",
        .warning = std::move(pair.warning),
        .score = filenameHint ? texmapsPairScore + filenameHintScoreBonus : texmapsPairScore,
        .dimensions = std::nullopt,
        .validated = false,
        .openable = false,
    });
}

void appendTiledataCandidate(FileBrowserSummary & summary, const std::filesystem::path & path, std::string_view filename)
{
    if (!summary.readable || !summary.size)
    {
        return;
    }
    const auto extended = uomul::TiledataReader::open(path, uomul::TiledataLayout::extended64);
    const auto legacy = uomul::TiledataReader::open(path, uomul::TiledataLayout::legacy32);
    if (!extended && !legacy)
    {
        return;
    }
    const auto layout = extended ? uomul::TiledataLayout::extended64 : uomul::TiledataLayout::legacy32;
    const bool named = filename == "tiledata.mul";
    summary.assetCandidates.push_back(FileBrowserAssetCandidate{
        .kind = MulAssetKind::tiledata,
        .path = path,
        .companionPath = {},
        .label = "Tiledata",
        .facet = {},
        .reason = layout == uomul::TiledataLayout::extended64 ? "Extended64 MapTileRecord and StaticTileRecord catalog"
                                                              : "Legacy32 MapTileRecord and StaticTileRecord catalog",
        .warning = named ? std::string{} : "Unexpected filename",
        .score = named ? tiledataNamedScore : tiledataSizeScore,
        .itemCount = uomul::tiledataMapTileCount + uomul::tiledataStaticTileCount,
        .dimensions = std::nullopt,
        .validated = true,
        .openable = true,
    });
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

void sortFileBrowserCandidates(std::vector<FileBrowserAssetCandidate> & candidates)
{
    std::ranges::stable_sort(candidates,
                             [](const auto & left, const auto & right)
                             {
                                 const bool leftHasWarning = !left.warning.empty();
                                 const bool rightHasWarning = !right.warning.empty();
                                 if (leftHasWarning != rightHasWarning)
                                 {
                                     return !leftHasWarning;
                                 }
                                 if (left.score != right.score)
                                 {
                                     return left.score > right.score;
                                 }
                                 return left.openable > right.openable;
                             });
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

    const auto filename = lowercase(path.filename().string());
    appendMapCandidates(summary, path);
    appendRadarcolCandidate(summary, path, filename);
    appendTexmapsCandidate(summary, path, filename);
    appendTiledataCandidate(summary, path, filename);
    sortFileBrowserCandidates(summary.assetCandidates);
    if (summary.mapCandidates.empty() && summary.assetCandidates.empty())
    {
        summary.error = "No known facet dimensions match this MUL file; choose Custom dimensions";
    }
    return summary;
}

FileBrowserAssetCandidate inspectTexmapsPair(const std::filesystem::path & indexPath, const std::filesystem::path & dataPath)
{
    const auto reader = uomul::TexMapsReader::open(indexPath, dataPath);
    std::string reason;
    if (reader)
    {
        reason = std::to_string(reader->textureCount()) + " entries";
    }
    else if (reader.error().message.empty())
    {
        reason = "Invalid Texmaps file set";
    }
    else
    {
        reason = reader.error().message;
    }
    return FileBrowserAssetCandidate{
        .kind = MulAssetKind::texmapsPair,
        .path = indexPath,
        .companionPath = dataPath,
        .label = "Texture pair",
        .facet = {},
        .reason = std::move(reason),
        .warning = {},
        .score = texmapsPairScore,
        .itemCount = reader ? reader->textureCount() : 0U,
        .dimensions = std::nullopt,
        .validated = reader.has_value(),
        .openable = reader.has_value(),
    };
}

} // namespace uo_mule::gui
