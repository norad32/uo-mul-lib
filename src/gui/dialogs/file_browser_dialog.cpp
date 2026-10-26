#include "gui/dialogs/file_browser_dialog.h"
#include "core/app_info.h"
#include "gui/presentation/bounded_buffer.h"
#include "gui/presentation/dialog_presentation.h"
#include "gui/presentation/file_size.h"
#include "gui/presentation/iconography.h"
#include "gui/presentation/strings.h"
#include "gui/presentation/theme.h"
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
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <imgui.h>
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

constexpr float placesWidth = 160.0F;
constexpr float summaryWidth = 260.0F;
constexpr float listingHeight = 480.0F;
constexpr float dateColumnWidth = 150.0F;
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
constexpr std::array<const char *, 7> standardPlaceNames{
    "Desktop",
    "Documents",
    "Downloads",
    "Music",
    "Pictures",
    "Videos",
    "Root",
};

[[nodiscard]] std::filesystem::path recentFoldersPath()
{
    const std::filesystem::path directory = configDirectory();
    if (directory.empty())
    {
        return {};
    }
    return directory / std::string(core::applicationName()) / "file_browser_folders.conf";
}

void loadRecentFolders(FileBrowserState & state)
{
    const auto path = recentFoldersPath();
    if (path.empty())
    {
        return;
    }
    std::ifstream input(path);
    std::string value;
    while (std::getline(input, value) && state.recentFolders.size() < maxRecentLocations)
    {
        if (!value.empty())
        {
            state.recentFolders.emplace_back(value);
        }
    }
}

void saveRecentFolders(const FileBrowserState & state)
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
    for (const auto & folder : state.recentFolders)
    {
        output << folder.string() << '\n';
    }
}

[[nodiscard]] bool isDirectory(const std::filesystem::path & path)
{
    std::error_code error;
    return std::filesystem::is_directory(path, error) && !error;
}

[[nodiscard]] std::filesystem::path absoluteDirectory(const std::filesystem::path & path)
{
    std::error_code error;
    auto absolute = std::filesystem::absolute(path, error).lexically_normal();
    if (!error && isDirectory(absolute))
    {
        return absolute;
    }
    return std::filesystem::current_path(error);
}

void recordRecentFolder(FileBrowserState & state, const std::filesystem::path & folder);

void recordRecentFolder(FileBrowserState & state, const std::filesystem::path & folder)
{
    const auto normalized = absoluteDirectory(folder);
    const auto removed = std::ranges::remove(state.recentFolders, normalized);
    state.recentFolders.erase(removed.begin(), removed.end());
    state.recentFolders.insert(state.recentFolders.begin(), normalized);
    if (state.recentFolders.size() > maxRecentLocations)
    {
        state.recentFolders.resize(maxRecentLocations);
    }
    saveRecentFolders(state);
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

void updateSummary(FileBrowserState & state)
{
    if (state.selectedPath.empty() || !std::filesystem::exists(state.selectedPath))
    {
        state.summary = {};
        return;
    }
    state.summary = inspectFileBrowserSelection(state.selectedPath);
}

void navigate(FileBrowserState & state, const std::filesystem::path & path, bool addHistory)
{
    const auto directory = absoluteDirectory(path);
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error) || error)
    {
        state.error = error ? error.message() : "Path is not a directory";
        return;
    }

    state.directory = directory;
    state.selectedPath.clear();
    state.summary = {};
    state.error.clear();
    copyToBuffer(state.location, directory);

    if (addHistory)
    {
        if (!state.history.empty() && state.historyIndex + 1U < state.history.size())
        {
            state.history.erase(state.history.begin() + static_cast<std::ptrdiff_t>(state.historyIndex + 1U),
                                state.history.end());
        }
        if (state.history.empty() || state.history.back() != directory)
        {
            state.history.push_back(directory);
        }
        state.historyIndex = state.history.size() - 1U;
    }
    recordRecentFolder(state, directory);
}

[[nodiscard]] bool selectableEntry(const FileBrowserState & state, const std::filesystem::directory_entry & entry)
{
    std::error_code error;
    if (entry.is_directory(error))
    {
        return !error;
    }
    return !error && entry.is_regular_file(error) && fileBrowserMatchesFilter(entry.path(), state.request.extension);
}

[[nodiscard]] std::filesystem::path savePath(const FileBrowserState & state)
{
    std::filesystem::path value(state.filename.data());
    if (value.empty())
    {
        return {};
    }
    if (value.is_relative())
    {
        value = state.directory / value;
    }
    if (!state.request.extension.empty() && value.extension().empty())
    {
        value += state.request.extension;
    }
    return value.lexically_normal();
}

[[nodiscard]] bool validSelection(const FileBrowserState & state, std::filesystem::path & result)
{
    if (state.request.mode == FileBrowserMode::chooseDirectory)
    {
        result = state.selectedPath.empty() ? state.directory : state.selectedPath;
        return isDirectory(result);
    }
    if (state.request.mode == FileBrowserMode::saveFile)
    {
        result = savePath(state);
        if (result.empty() || !fileBrowserMatchesFilter(result, state.request.extension))
        {
            return false;
        }
        std::error_code error;
        return std::filesystem::is_directory(result.parent_path(), error) && !error;
    }

    result = state.selectedPath;
    std::error_code error;
    return !result.empty() && std::filesystem::is_regular_file(result, error) && !error &&
           fileBrowserMatchesFilter(result, state.request.extension);
}

void selectPath(FileBrowserState & state, const std::filesystem::path & path)
{
    state.selectedPath = path;
    if (state.request.mode == FileBrowserMode::saveFile)
    {
        copyToBuffer(state.filename, path.filename());
    }
    updateSummary(state);
}

std::optional<DialogResult> complete(FileBrowserState & state, std::uint64_t generation)
{
    std::filesystem::path path;
    if (!validSelection(state, path))
    {
        state.error = state.request.mode == FileBrowserMode::saveFile ? "Choose a valid .mul output path"
                                                                      : "Choose a matching file or directory";
        return std::nullopt;
    }

    FileBrowserResult result{
        .target = state.request.target,
        .role = state.request.role,
        .path = path,
        .mapCandidates = state.summary.mapCandidates,
        .assetCandidates = state.summary.assetCandidates,
        .assetCandidate = std::nullopt,
    };
    recordRecentFolder(state, path.parent_path());
    state.visible = false;
    return DialogResult{
        .id = DialogId::fileBrowser,
        .action = DialogAction::accept,
        .payload = std::move(result),
        .generation = generation,
    };
}

void drawPlaces(FileBrowserState & state)
{
    ImGui::BeginChild("Places",
                      ImVec2(placesWidth, listingHeight),
                      ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    dialogHeading("Places");

    const auto home = std::filesystem::path(std::getenv("HOME") == nullptr ? "" : std::getenv("HOME"));
    if (!home.empty() && ImGui::Selectable(strings::home(), state.directory == home))
    {
        navigate(state, home, true);
    }

    const std::array<std::filesystem::path, standardPlaceNames.size()> paths{
        home / "Desktop",
        home / "Documents",
        home / "Downloads",
        home / "Music",
        home / "Pictures",
        home / "Videos",
        "/",
    };
    for (std::size_t index = 0; index < standardPlaceNames.size(); index++)
    {
        if (isDirectory(paths.at(index)) && ImGui::Selectable(standardPlaceNames.at(index), state.directory == paths.at(index)))
        {
            navigate(state, paths.at(index), true);
        }
    }

    dialogSeparatorText("Recent Locations");
    for (const auto & location : state.recentFolders)
    {
        const auto filename = location.filename().string();
        const auto label = filename.empty() ? location.root_path().string() : filename;
        const auto selectableLabel = label + "##" + location.string();
        if (ImGui::Selectable(selectableLabel.c_str(), state.directory == location))
        {
            navigate(state, location, false);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", location.string().c_str());
        }
    }
    ImGui::EndChild();
}

void drawSummary(const FileBrowserState & state)
{
    ImGui::BeginChild("Summary",
                      ImVec2(summaryWidth, listingHeight),
                      ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    dialogHeading("File Summary");
    ImGui::Separator();
    if (state.selectedPath.empty())
    {
        ImGui::TextWrapped("Select a file to inspect its metadata.");
        ImGui::EndChild();
        return;
    }

    const auto & summary = state.summary;
    const std::string filename = summary.path.filename().string();
    const std::string fullPath = summary.path.string();
    ImGui::TextColored(theme::imguiColor(theme::subheading), "%s", filename.c_str());
    ImGui::TextColored(theme::imguiColor(theme::mutedText), "%s", fullPath.c_str());
    if (summary.size)
    {
        const auto size = formatFileSize(*summary.size);
        ImGui::Text(strings::sizeFormat(), size.c_str());
    }
    ImGui::Text(strings::modifiedFormat(), summary.modified.c_str());
    if (!summary.error.empty())
    {
        ImGui::TextColored(theme::imguiColor(theme::warning), "%s", summary.error.c_str());
    }

    bool firstMapCandidate = true;
    for (const auto & candidate : summary.mapCandidates)
    {
        ImGui::Separator();
        if (firstMapCandidate)
        {
            dialogSubheading("Map");
            firstMapCandidate = false;
        }
        ImGui::Text(strings::facetFormat(), candidate.facet.c_str());
        ImGui::Text(strings::dimensionsFormat(), candidate.dimensions.widthTiles, candidate.dimensions.heightTiles);
        ImGui::Text(strings::blocksFormat(),
                    static_cast<unsigned long long>(candidate.completeBlocks),
                    static_cast<unsigned long long>(candidate.expectedBlocks));
        ImGui::Text(strings::missingFormat(), static_cast<unsigned long long>(candidate.missingBlocks));
        ImGui::Text(strings::trailingBytesFormat(), static_cast<unsigned long long>(candidate.trailingBytes));
        ImGui::TextColored(candidate.valid ? theme::imguiColor(theme::success) : theme::imguiColor(theme::error),
                           "%s",
                           candidate.valid ? "Openable" : "Invalid map candidate");
    }
    const bool hasValidMapCandidate =
        std::ranges::any_of(summary.mapCandidates, [](const FileBrowserMapCandidate & candidate) { return candidate.valid; });
    for (const auto & candidate : summary.assetCandidates)
    {
        if (candidate.kind == MulAssetKind::map && hasValidMapCandidate)
        {
            continue;
        }
        ImGui::Separator();
        ImGui::TextColored(theme::imguiColor(theme::subheading), "%s", candidate.label.c_str());
        ImGui::TextWrapped("%s", candidate.reason.c_str());
        if (!candidate.warning.empty())
        {
            ImGui::TextColored(theme::imguiColor(theme::warning), "%s", candidate.warning.c_str());
        }
        ImGui::TextColored(candidate.openable ? theme::imguiColor(theme::success) : theme::imguiColor(theme::warning),
                           "%s",
                           candidate.openable ? "Openable" : "Requires selection");
    }
    ImGui::EndChild();
}

void sortAssetCandidates(std::vector<FileBrowserAssetCandidate> & candidates)
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

    std::string reason = std::to_string(reader->colorCount()) + " entries";
    std::string warning;
    int score = radarcolStructuralScore;
    if (namedRadarcol || classicRadarcolSize)
    {
        reason = std::to_string(reader->colorCount()) + " entries";
    }
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
        .reason = std::move(reason),
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

void sortFileBrowserCandidates(std::vector<FileBrowserAssetCandidate> & candidates)
{
    sortAssetCandidates(candidates);
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
    sortAssetCandidates(summary.assetCandidates);
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

void requestFileBrowser(FileBrowserState & state, FileBrowserRequest request)
{
    auto recentFolders = std::move(state.recentFolders);
    state = {};
    state.recentFolders = std::move(recentFolders);
    if (state.recentFolders.empty())
    {
        loadRecentFolders(state);
    }
    state.visible = true;
    state.request = std::move(request);
    std::filesystem::path initial = state.request.initialPath;
    if (initial.empty())
    {
        initial = state.recentFolders.empty() ? std::filesystem::current_path() : state.recentFolders.front();
    }
    std::error_code error;
    const bool initialIsFile = std::filesystem::is_regular_file(initial, error);
    const bool initialIsSavePath = state.request.mode == FileBrowserMode::saveFile && !std::filesystem::exists(initial, error);
    if (initialIsFile || initialIsSavePath)
    {
        state.directory = absoluteDirectory(initial.parent_path());
        if (state.request.mode == FileBrowserMode::saveFile)
        {
            copyToBuffer(state.filename, initial.filename());
        }
    }
    else
    {
        state.directory = absoluteDirectory(initial);
    }
    navigate(state, state.directory, true);
    if (!initial.empty() && initialIsFile)
    {
        selectPath(state, initial);
    }
}

void cancelFileBrowser(FileBrowserState & state)
{
    auto recentFolders = std::move(state.recentFolders);
    state = {};
    state.recentFolders = std::move(recentFolders);
}

namespace
{

[[nodiscard]] bool drawBrowserToolbarButton(IconId icon, std::string_view label, std::string_view stableId)
{
    std::string buttonLabel = iconFontAvailable() ? iconGlyph(icon) : std::string(label);
    buttonLabel += "##";
    buttonLabel += stableId;
    const bool clicked = ImGui::Button(buttonLabel.c_str());
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%.*s", static_cast<int>(label.size()), label.data());
    }
    return clicked;
}

void drawBrowserToolbar(FileBrowserState & state)
{
    if (drawBrowserToolbarButton(IconId::arrowLeft, "Back", "browser.back") && state.historyIndex > 0U)
    {
        --state.historyIndex;
        navigate(state, state.history.at(state.historyIndex), false);
    }
    ImGui::SameLine();
    if (drawBrowserToolbarButton(IconId::arrowRight, "Forward", "browser.forward") &&
        state.historyIndex + 1U < state.history.size())
    {
        state.historyIndex++;
        navigate(state, state.history.at(state.historyIndex), false);
    }
    ImGui::SameLine();
    if (drawBrowserToolbarButton(IconId::arrowUp, "Up", "browser.up"))
    {
        navigate(state, state.directory.parent_path(), true);
    }
    ImGui::SameLine();
    if (drawBrowserToolbarButton(IconId::refresh, "Refresh", "browser.refresh"))
    {
        navigate(state, state.directory, false);
    }
    ImGui::SameLine();
    const std::string hiddenLabel = iconFontAvailable() ? iconGlyph(IconId::eye) : "Show hidden";
    ImGui::TextUnformatted(hiddenLabel.c_str());
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", strings::showHidden());
    }
    ImGui::SameLine();
    ImGui::Checkbox("##browser.show-hidden", &state.showHidden);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", strings::showHidden());
    }

    ImGui::SetNextItemWidth(-1.0F);
    if (ImGui::InputText("##Location", state.location.data(), state.location.size(), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        navigate(state, state.location.data(), true);
    }
}

[[nodiscard]] std::vector<std::filesystem::directory_entry> browserEntries(const FileBrowserState & state)
{
    std::vector<std::filesystem::directory_entry> entries;
    std::error_code error;
    for (const auto & entry : std::filesystem::directory_iterator(state.directory, error))
    {
        const auto name = entry.path().filename().string();
        if (!state.showHidden && !name.empty() && name.front() == '.')
        {
            continue;
        }
        if (selectableEntry(state, entry))
        {
            entries.push_back(entry);
        }
    }

    std::ranges::sort(entries,
                      [](const auto & left, const auto & right)
                      {
                          std::error_code leftError;
                          std::error_code rightError;
                          const bool leftDirectory = left.is_directory(leftError);
                          const bool rightDirectory = right.is_directory(rightError);
                          if (leftDirectory != rightDirectory)
                          {
                              return leftDirectory > rightDirectory;
                          }
                          return lowercase(left.path().filename().string()) < lowercase(right.path().filename().string());
                      });
    return entries;
}

std::optional<DialogResult>
drawBrowserEntry(FileBrowserState & state, const std::filesystem::directory_entry & entry, std::uint64_t generation)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    const bool selected = state.selectedPath == entry.path();
    if (ImGui::Selectable(entry.path().filename().string().c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
    {
        selectPath(state, entry.path());
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        if (isDirectory(entry.path()))
        {
            navigate(state, entry.path(), true);
        }
        else if (state.request.mode == FileBrowserMode::openFile)
        {
            const auto result = complete(state, generation);
            if (result)
            {
                return result;
            }
        }
    }
    ImGui::TableNextColumn();
    std::error_code sizeError;
    const auto size = entry.is_regular_file(sizeError) ? formatFileSize(entry.file_size(sizeError)) : "";
    ImGui::TextUnformatted(size.c_str());
    ImGui::TableNextColumn();
    std::error_code timeError;
    const auto modified = entry.last_write_time(timeError);
    ImGui::TextUnformatted(timeError ? "Unknown" : formatModifiedTime(modified).c_str());
    return std::nullopt;
}

std::optional<DialogResult> drawBrowserEntries(FileBrowserState & state, std::uint64_t generation)
{
    if (!ImGui::BeginTable("Entries", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH))
    {
        return std::nullopt;
    }

    ImGui::TableSetupColumn("Name");
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 100.0F);
    ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, dateColumnWidth);
    ImGui::TableHeadersRow();
    for (const auto & entry : browserEntries(state))
    {
        if (const auto result = drawBrowserEntry(state, entry, generation))
        {
            ImGui::EndTable();
            return result;
        }
    }
    ImGui::EndTable();
    return std::nullopt;
}

std::optional<DialogResult> drawBrowserListing(FileBrowserState & state, std::uint64_t generation)
{
    if (ImGui::BeginTable("BrowserBody", 3, ImGuiTableFlags_SizingStretchProp, ImVec2(0.0F, listingHeight)))
    {
        ImGui::TableSetupColumn("Places", ImGuiTableColumnFlags_WidthFixed, placesWidth);
        ImGui::TableSetupColumn("Files", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Summary", ImGuiTableColumnFlags_WidthFixed, summaryWidth);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        drawPlaces(state);
        ImGui::TableNextColumn();
        ImGui::BeginChild("FileList", ImVec2(0.0F, listingHeight), ImGuiChildFlags_Borders, ImGuiWindowFlags_None);
        const auto result = drawBrowserEntries(state, generation);
        ImGui::EndChild();
        ImGui::TableNextColumn();
        drawSummary(state);
        ImGui::EndTable();
        return result;
    }
    return std::nullopt;
}

std::optional<DialogResult> drawBrowserFooter(FileBrowserState & state, std::uint64_t generation)
{
    if (!state.error.empty())
    {
        ImGui::SameLine();
        ImGui::TextColored(theme::imguiColor(theme::warning), "%s", state.error.c_str());
    }

    const char * confirmLabel = "Select Folder";
    if (state.request.mode == FileBrowserMode::openFile)
    {
        confirmLabel = "Open";
    }
    else if (state.request.mode == FileBrowserMode::saveFile)
    {
        confirmLabel = "Save";
    }
    const std::string cancelButtonLabel = iconLabel(IconId::cancel, "Cancel");

    if (state.request.mode == FileBrowserMode::saveFile)
    {
        ImGui::Separator();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Filename");
        ImGui::SameLine();

        const std::string confirmButtonLabel = iconLabel(IconId::save, confirmLabel);
        const float buttonWidth = ImGui::CalcTextSize(confirmButtonLabel.c_str()).x +
                                  (ImGui::GetStyle().FramePadding.x * 2.0F) + ImGui::GetStyle().ItemSpacing.x +
                                  ImGui::CalcTextSize(cancelButtonLabel.c_str()).x + (ImGui::GetStyle().FramePadding.x * 2.0F);
        const float inputWidth =
            std::max(120.0F, ImGui::GetContentRegionAvail().x - buttonWidth - ImGui::GetStyle().ItemSpacing.x);
        ImGui::SetNextItemWidth(inputWidth);
        ImGui::InputText("##Filename", state.filename.data(), state.filename.size());
        ImGui::SameLine();
        if (dialogButton(confirmButtonLabel.c_str()))
        {
            return complete(state, generation);
        }
        ImGui::SameLine();
        if (dialogButton(cancelButtonLabel.c_str()))
        {
            cancelFileBrowser(state);
            return DialogResult{
                .id = DialogId::fileBrowser,
                .action = DialogAction::cancel,
                .payload = {},
                .generation = generation,
            };
        }
    }
    else
    {
        const std::string confirmButtonLabel =
            state.request.mode == FileBrowserMode::openFile ? iconLabel(IconId::folder, confirmLabel) : confirmLabel;
        beginDialogFooter({confirmButtonLabel.c_str(), cancelButtonLabel.c_str()});
        std::optional<DialogResult> result;
        if (dialogButton(confirmButtonLabel.c_str()))
        {
            result = complete(state, generation);
        }
        else
        {
            ImGui::SameLine();
            if (dialogButton(cancelButtonLabel.c_str()))
            {
                cancelFileBrowser(state);
                result = DialogResult{
                    .id = DialogId::fileBrowser,
                    .action = DialogAction::cancel,
                    .payload = {},
                    .generation = generation,
                };
            }
        }
        endDialogFooter();
        if (result)
        {
            return result;
        }
    }
    if (!state.visible)
    {
        ImGui::CloseCurrentPopup();
    }
    return std::nullopt;
}

} // namespace

std::optional<DialogResult> drawFileBrowser(FileBrowserState & state, std::uint64_t generation)
{
    if (!state.visible)
    {
        return std::nullopt;
    }

    const std::string title = state.request.title.empty()
                                  ? iconLabel(IconId::fileBrowserDialog, "Open File###dialog.file-browser")
                                  : iconLabel(IconId::fileBrowserDialog, state.request.title + "###dialog.file-browser");
    if (!beginDialog(title.c_str(),
                     &state.visible,
                     browserDialogSpec(),
                     static_cast<ImGuiWindowFlags>(ImGuiWindowFlags_NoResize + ImGuiWindowFlags_NoScrollbar +
                                                   ImGuiWindowFlags_NoScrollWithMouse)))
    {
        return std::nullopt;
    }

    drawBrowserToolbar(state);
    auto result = drawBrowserListing(state, generation);
    if (!result)
    {
        result = drawBrowserFooter(state, generation);
    }
    endDialog();
    if (!result && !state.visible)
    {
        return DialogResult{
            .id = DialogId::fileBrowser,
            .action = DialogAction::cancel,
            .payload = {},
            .generation = generation,
        };
    }
    return result;
}

} // namespace uo_mule::gui
