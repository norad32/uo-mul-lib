#include "gui/dialogs/file_browser_dialog.h"
#include "core/facet_sizes.h"
#include "gui/common/file_size.h"
#include "gui/theme.h"
#include "uomul/map.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <imgui.h>
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

constexpr float browserWidth = 980.0F;
constexpr float browserHeight = 620.0F;
constexpr float placesWidth = 160.0F;
constexpr float summaryWidth = 260.0F;
constexpr float listingHeight = 480.0F;
constexpr float dateColumnWidth = 150.0F;
constexpr std::size_t dateBufferSize = 64;
constexpr std::size_t maxRecentLocations = 12;
constexpr ImGuiWindowFlags fileBrowserWindowFlags = static_cast<ImGuiWindowFlags>(
    static_cast<unsigned int>(ImGuiWindowFlags_NoCollapse) | static_cast<unsigned int>(ImGuiWindowFlags_NoResize) |
    static_cast<unsigned int>(ImGuiWindowFlags_NoScrollbar) | static_cast<unsigned int>(ImGuiWindowFlags_NoScrollWithMouse));
constexpr std::array<const char *, 7> standardPlaceNames{
    "Desktop",
    "Documents",
    "Downloads",
    "Music",
    "Pictures",
    "Videos",
    "Root",
};

void copyBuffer(std::array<char, fileBrowserBufferSize> & buffer, std::string_view value)
{
    const auto length = std::min(value.size(), buffer.size() - 1U);
    std::memcpy(buffer.data(), value.data(), length);
    buffer.at(length) = '\0';
}

void copyBuffer(std::array<char, fileBrowserBufferSize> & buffer, const std::filesystem::path & value)
{
    const std::string text = value.string();
    copyBuffer(buffer, std::string_view(text));
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
    copyBuffer(state.location, directory);

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
        copyBuffer(state.filename, path.filename());
    }
    updateSummary(state);
}

void complete(FileBrowserState & state)
{
    std::filesystem::path path;
    if (!validSelection(state, path))
    {
        state.error = state.request.mode == FileBrowserMode::saveFile ? "Choose a valid .mul output path"
                                                                      : "Choose a matching file or directory";
        return;
    }

    state.result = FileBrowserResult{
        .target = state.request.target,
        .role = state.request.role,
        .path = path,
        .mapCandidates = state.summary.mapCandidates,
    };
    state.visible = false;
}

void drawPlaces(FileBrowserState & state)
{
    ImGui::BeginChild("Places",
                      ImVec2(placesWidth, listingHeight),
                      ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::TextUnformatted("Places");

    const auto home = std::filesystem::path(std::getenv("HOME") == nullptr ? "" : std::getenv("HOME"));
    if (!home.empty() && ImGui::Selectable("Home", state.directory == home))
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

    ImGui::SeparatorText("Recent Locations");
    std::vector<std::filesystem::path> recentLocations;
    for (std::size_t index = state.history.size(); index-- > 0U;)
    {
        if (recentLocations.size() >= maxRecentLocations)
        {
            break;
        }
        const auto & location = state.history.at(index);
        if (std::ranges::find(recentLocations, location) != recentLocations.end())
        {
            continue;
        }
        recentLocations.push_back(location);

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
    ImGui::TextUnformatted("File Summary");
    ImGui::Separator();
    if (state.selectedPath.empty())
    {
        ImGui::TextWrapped("Select a file to inspect its metadata.");
        ImGui::EndChild();
        return;
    }

    const auto & summary = state.summary;
    ImGui::TextWrapped("%s", summary.path.filename().string().c_str());
    ImGui::TextWrapped("%s", summary.path.string().c_str());
    if (summary.size)
    {
        const auto size = formatFileSize(*summary.size);
        ImGui::Text("Size: %s", size.c_str());
    }
    ImGui::Text("Modified: %s", summary.modified.c_str());
    ImGui::Text("Readable: %s", summary.readable ? "yes" : "no");
    if (!summary.error.empty())
    {
        ImGui::TextColored(theme::imguiColor(theme::amber), "%s", summary.error.c_str());
    }

    for (const auto & candidate : summary.mapCandidates)
    {
        ImGui::Separator();
        ImGui::Text("Facet: %s", candidate.facet.c_str());
        ImGui::Text("Dimensions: %u x %u", candidate.dimensions.widthTiles, candidate.dimensions.heightTiles);
        ImGui::Text("Blocks: %llu / %llu",
                    static_cast<unsigned long long>(candidate.completeBlocks),
                    static_cast<unsigned long long>(candidate.expectedBlocks));
        ImGui::Text("Missing: %llu", static_cast<unsigned long long>(candidate.missingBlocks));
        ImGui::Text("Trailing bytes: %llu", static_cast<unsigned long long>(candidate.trailingBytes));
        ImGui::TextColored(candidate.valid ? theme::imguiColor(theme::leafGreen) : theme::imguiColor(theme::coral),
                           "%s",
                           candidate.valid ? "Valid map candidate" : "Invalid map candidate");
    }
    ImGui::EndChild();
}

} // namespace

bool fileBrowserMatchesFilter(const std::filesystem::path & path, std::string_view extension)
{
    if (extension.empty())
    {
        return true;
    }
    return lowercase(path.extension().string()) == lowercase(std::string(extension));
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

    const auto exactFacets =
        summary.size ? uo_mule::core::facetSizesForMapFileBytes(*summary.size) : std::vector<uo_mule::core::FacetSize>{};
    for (const auto & facet : exactFacets)
    {
        const auto dimensions = uomul::Dimensions{.widthTiles = facet.widthTiles, .heightTiles = facet.heightTiles};
        auto reader = uomul::MapReader::open(path, dimensions);
        if (reader)
        {
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
        }
    }
    if (summary.mapCandidates.empty())
    {
        summary.error = "No known facet dimensions match this MUL file; choose Custom dimensions";
    }
    return summary;
}

void requestFileBrowser(FileBrowserState & state, FileBrowserRequest request)
{
    state = {};
    state.visible = true;
    state.request = std::move(request);
    std::filesystem::path initial = state.request.initialPath;
    if (initial.empty())
    {
        initial = std::filesystem::current_path();
    }
    std::error_code error;
    const bool initialIsFile = std::filesystem::is_regular_file(initial, error);
    const bool initialIsSavePath = state.request.mode == FileBrowserMode::saveFile && !std::filesystem::exists(initial, error);
    if (initialIsFile || initialIsSavePath)
    {
        state.directory = absoluteDirectory(initial.parent_path());
        if (state.request.mode == FileBrowserMode::saveFile)
        {
            copyBuffer(state.filename, initial.filename());
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
    state = {};
}

std::optional<FileBrowserResult> consumeFileBrowserResult(FileBrowserState & state)
{
    auto result = std::move(state.result);
    state.result.reset();
    return result;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void drawFileBrowser(FileBrowserState & state)
{
    if (!state.visible)
    {
        return;
    }

    const std::string title =
        state.request.title.empty() ? "Open File###dialog.file-browser" : state.request.title + "###dialog.file-browser";
    ImGui::OpenPopup(title.c_str());
    ImGui::SetNextWindowSize(ImVec2(browserWidth, browserHeight), ImGuiCond_FirstUseEver);
    if (!ImGui::BeginPopupModal(title.c_str(), &state.visible, fileBrowserWindowFlags))
    {
        return;
    }

    if (ImGui::Button("Back") && state.historyIndex > 0U)
    {
        --state.historyIndex;
        navigate(state, state.history.at(state.historyIndex), false);
    }
    ImGui::SameLine();
    if (ImGui::Button("Forward") && state.historyIndex + 1U < state.history.size())
    {
        state.historyIndex++;
        navigate(state, state.history.at(state.historyIndex), false);
    }
    ImGui::SameLine();
    if (ImGui::Button("Up"))
    {
        navigate(state, state.directory.parent_path(), true);
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh"))
    {
        navigate(state, state.directory, false);
    }
    ImGui::SameLine();
    ImGui::Checkbox("Hidden", &state.showHidden);

    ImGui::SetNextItemWidth(-1.0F);
    if (ImGui::InputText("##Location", state.location.data(), state.location.size(), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        navigate(state, state.location.data(), true);
    }

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
        if (ImGui::BeginTable("Entries", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH))
        {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 100.0F);
            ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, dateColumnWidth);
            ImGui::TableHeadersRow();

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
                                  return lowercase(left.path().filename().string()) <
                                         lowercase(right.path().filename().string());
                              });

            for (const auto & entry : entries)
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
                        complete(state);
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
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();
        ImGui::TableNextColumn();
        drawSummary(state);
        ImGui::EndTable();
    }

    if (!state.error.empty())
    {
        ImGui::SameLine();
        ImGui::TextColored(theme::imguiColor(theme::amber), "%s", state.error.c_str());
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

    const float buttonPadding = ImGui::GetStyle().FramePadding.x * 2.0F;
    const float buttonSpacing = ImGui::GetStyle().ItemSpacing.x;
    const float actionWidth = ImGui::CalcTextSize(confirmLabel).x + buttonPadding;
    const float cancelWidth = ImGui::CalcTextSize("Cancel").x + buttonPadding;
    ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - actionWidth - buttonSpacing - cancelWidth);
    if (ImGui::Button(confirmLabel))
    {
        complete(state);
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        cancelFileBrowser(state);
    }
    if (!state.visible)
    {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

} // namespace uo_mule::gui
