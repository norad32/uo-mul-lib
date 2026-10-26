#include "gui/dialogs/file_browser_dialog.h"
#include "gui/presentation/bounded_buffer.h"
#include "gui/presentation/dialog_presentation.h"
#include "gui/presentation/file_size.h"
#include "gui/presentation/iconography.h"
#include "gui/presentation/strings.h"
#include "gui/presentation/theme.h"
#include "gui_model/dialogs/dialog_contracts.h"
#include "gui_model/dialogs/dialog_host.h"
#include "gui_model/dialogs/file_browser_model.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <filesystem>
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

constexpr float placesWidth = 160.0F;
constexpr float summaryWidth = 260.0F;
constexpr float listingHeight = 480.0F;
constexpr float dateColumnWidth = 150.0F;
constexpr std::size_t dateBufferSize = 64;
constexpr std::array<const char *, 7> standardPlaceNames{
    "Desktop",
    "Documents",
    "Downloads",
    "Music",
    "Pictures",
    "Videos",
    "Root",
};

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
    const auto directory = normalizeFileBrowserDirectory(path);
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
    recordRecentFolder(state.recentFolders, directory);
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
        return isFileBrowserDirectory(result);
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
    recordRecentFolder(state.recentFolders, path.parent_path());
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
        if (isFileBrowserDirectory(paths.at(index)) &&
            ImGui::Selectable(standardPlaceNames.at(index), state.directory == paths.at(index)))
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

} // namespace

void requestFileBrowser(FileBrowserState & state, FileBrowserRequest request)
{
    auto * const dialogHost = state.dialogHost;
    const bool recentFoldersDirty = state.recentFoldersDirty;
    auto recentFolders = std::move(state.recentFolders);
    state = {};
    state.dialogHost = dialogHost;
    state.recentFoldersDirty = recentFoldersDirty;
    state.recentFolders = std::move(recentFolders);
    if (state.recentFolders.empty())
    {
        state.recentFolders = loadRecentFolders();
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
        state.directory = normalizeFileBrowserDirectory(initial.parent_path());
        if (state.request.mode == FileBrowserMode::saveFile)
        {
            copyToBuffer(state.filename, initial.filename());
        }
    }
    else
    {
        state.directory = normalizeFileBrowserDirectory(initial);
    }
    navigate(state, state.directory, true);
    if (!initial.empty() && initialIsFile)
    {
        selectPath(state, initial);
    }
    if (state.dialogHost != nullptr)
    {
        auto dialogRequest = makeDialogRequest(DialogId::fileBrowser);
        dialogRequest.replaceActive = true;
        static_cast<void>(state.dialogHost->request(std::move(dialogRequest)));
    }
}

void cancelFileBrowser(FileBrowserState & state)
{
    auto * const dialogHost = state.dialogHost;
    const bool recentFoldersDirty = state.recentFoldersDirty;
    auto recentFolders = std::move(state.recentFolders);
    state = {};
    state.dialogHost = dialogHost;
    state.recentFoldersDirty = recentFoldersDirty;
    state.recentFolders = std::move(recentFolders);
}

void persistFileBrowserRecentFolders(FileBrowserState & state)
{
    if (!state.recentFoldersDirty)
    {
        return;
    }
    saveRecentFolders(state.recentFolders);
    state.recentFoldersDirty = false;
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
        if (isFileBrowserDirectory(entry.path()))
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
