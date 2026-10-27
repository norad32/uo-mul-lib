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
#include <cstddef>
#include <cstdint>
#include <cstdlib>
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
constexpr float newFolderInputWidth = 280.0F;
constexpr std::string_view createFolderPopupId = "dialog.file-browser.create-folder";
const std::array<const char *, 7> standardPlaceNames{
    strings::desktop(),
    strings::documents(),
    strings::downloads(),
    strings::music(),
    strings::pictures(),
    strings::videos(),
    strings::root(),
};

[[nodiscard]] std::string createFolderPopupLabel()
{
    return std::string(strings::createFolder()) + "###" + std::string(createFolderPopupId);
}

void updateSummary(FileBrowserState & state)
{
    if (state.selectedPath.empty())
    {
        state.summary = {};
        return;
    }
    state.summary = inspectFileBrowserSelection(state.selectedPath);
    if (!state.summary.exists)
    {
        state.summary = {};
    }
}

void navigate(FileBrowserState & state, const std::filesystem::path & path, bool addHistory)
{
    const auto directory = normalizeFileBrowserDirectory(path);
    if (directory.empty() || !isFileBrowserDirectory(directory))
    {
        state.error = strings::pathNotDirectory();
        return;
    }

    state.directory = directory;
    state.selectedPath.clear();
    state.summary = {};
    state.listingDirty = true;
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
    state.recentFoldersDirty = recordRecentFolder(state.recentFolders, directory) || state.recentFoldersDirty;
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
        return isFileBrowserDirectory(result.parent_path());
    }

    result = state.selectedPath;
    return !result.empty() && inspectFileBrowserEntry(result).regularFile &&
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
        state.error = state.request.mode == FileBrowserMode::saveFile ? strings::chooseValidMulOutputPath()
                                                                      : strings::chooseMatchingFileOrDirectory();
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
    state.recentFoldersDirty = recordRecentFolder(state.recentFolders, path.parent_path()) || state.recentFoldersDirty;
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
    dialogHeading(strings::places());

    const auto home = std::filesystem::path(std::getenv("HOME") == nullptr ? "" : std::getenv("HOME"));
    if (!home.empty() && ImGui::Selectable(strings::home(), state.directory == home))
    {
        navigate(state, home, true);
    }

    const std::array<std::filesystem::path, standardPlaceNames.size()> paths{
        home / strings::desktop(),
        home / strings::documents(),
        home / strings::downloads(),
        home / strings::music(),
        home / strings::pictures(),
        home / strings::videos(),
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

    dialogHeading(strings::recentLocations());
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
    dialogHeading(strings::fileSummary());
    ImGui::Separator();
    if (state.selectedPath.empty())
    {
        ImGui::TextWrapped("%s", strings::selectFileMetadata());
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
            dialogSubheading(strings::map());
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
                           candidate.valid ? strings::openable() : strings::invalidMapCandidate());
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
        if (!candidate.filenameWarning.empty())
        {
            ImGui::TextColored(theme::imguiColor(theme::warning), "%s", candidate.filenameWarning.c_str());
        }
        ImGui::TextColored(candidate.openable ? theme::imguiColor(theme::success) : theme::imguiColor(theme::warning),
                           "%s",
                           candidate.openable ? strings::openable() : strings::requiresSelection());
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
    const auto staleRecentFolder = [](const std::filesystem::path & folder)
    {
        const auto normalized = normalizeFileBrowserDirectory(folder);
        return normalized.empty() || !isFileBrowserDirectory(normalized);
    };
    const auto removedRecentFolders = std::ranges::remove_if(state.recentFolders, staleRecentFolder);
    if (removedRecentFolders.begin() != removedRecentFolders.end())
    {
        state.recentFolders.erase(removedRecentFolders.begin(), removedRecentFolders.end());
        state.recentFoldersDirty = true;
    }
    state.visible = true;
    state.request = std::move(request);
    std::filesystem::path initial = state.request.initialPath;
    if (initial.empty())
    {
        initial = state.recentFolders.empty() ? std::filesystem::current_path() : state.recentFolders.front();
    }
    const auto initialMetadata = inspectFileBrowserEntry(initial);
    const bool initialIsFile = initialMetadata.regularFile;
    const bool initialIsSavePath = state.request.mode == FileBrowserMode::saveFile && !initialMetadata.exists;
    const auto requestedDirectory = initialIsFile || initialIsSavePath ? initial.parent_path() : initial;
    const auto normalizedRequestedDirectory = normalizeFileBrowserDirectory(requestedDirectory);
    const bool requestedDirectoryIsValid =
        !normalizedRequestedDirectory.empty() && isFileBrowserDirectory(normalizedRequestedDirectory);
    auto initialDirectory = normalizedRequestedDirectory;
    if (!requestedDirectoryIsValid)
    {
        std::error_code error;
        initialDirectory = normalizeFileBrowserDirectory(std::filesystem::current_path(error));
    }
    state.directory = std::move(initialDirectory);
    if (initialIsSavePath)
    {
        copyToBuffer(state.filename, initial.filename());
    }
    if (!state.directory.empty())
    {
        navigate(state, state.directory, true);
    }
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
    if (drawBrowserToolbarButton(IconId::arrowLeft, strings::back(), "browser.back") && state.historyIndex > 0U)
    {
        --state.historyIndex;
        navigate(state, state.history.at(state.historyIndex), false);
    }
    ImGui::SameLine();
    if (drawBrowserToolbarButton(IconId::arrowRight, strings::forward(), "browser.forward") &&
        state.historyIndex + 1U < state.history.size())
    {
        state.historyIndex++;
        navigate(state, state.history.at(state.historyIndex), false);
    }
    ImGui::SameLine();
    if (drawBrowserToolbarButton(IconId::arrowUp, strings::up(), "browser.up"))
    {
        navigate(state, state.directory.parent_path(), true);
    }
    ImGui::SameLine();
    if (drawBrowserToolbarButton(IconId::refresh, strings::refresh(), "browser.refresh"))
    {
        navigate(state, state.directory, false);
    }
    ImGui::SameLine();
    const std::string hiddenLabel = iconFontAvailable() ? iconGlyph(IconId::eye) : strings::showHidden();
    ImGui::TextUnformatted(hiddenLabel.c_str());
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", strings::showHidden());
    }
    if (ImGui::IsItemEdited())
    {
        state.listingDirty = true;
    }
    ImGui::SameLine();
    ImGui::Checkbox("##browser.show-hidden", &state.showHidden);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", strings::showHidden());
    }

    ImGui::SameLine();
    if (drawBrowserToolbarButton(IconId::folderPlus, strings::createFolder(), "browser.create-folder"))
    {
        copyToBuffer(state.newFolderName, strings::newFolder());
        state.createFolderVisible = true;
        state.error.clear();
        const std::string popupLabel = createFolderPopupLabel();
        ImGui::OpenPopup(popupLabel.c_str());
    }

    ImGui::SetNextItemWidth(-1.0F);
    if (ImGui::InputText("##Location", state.location.data(), state.location.size(), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        navigate(state, state.location.data(), true);
    }
}

[[nodiscard]] bool createFolder(FileBrowserState & state)
{
    const std::filesystem::path name(state.newFolderName.data());
    if (name.empty() || name == "." || name == ".." || name.has_root_path() || name.has_parent_path())
    {
        state.error = strings::invalidFolderName();
        return false;
    }

    const std::filesystem::path path = state.directory / name;
    std::error_code error;
    if (!std::filesystem::create_directory(path, error) || error)
    {
        state.error = error ? error.message() : strings::folderAlreadyExists();
        return false;
    }

    navigate(state, state.directory, false);
    return true;
}

void drawCreateFolderPopup(FileBrowserState & state)
{
    const std::string popupLabel = createFolderPopupLabel();
    if (!ImGui::BeginPopupModal(popupLabel.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        state.createFolderVisible = false;
        state.error.clear();
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }

    ImGui::TextUnformatted(strings::folderName());
    ImGui::SetNextItemWidth(newFolderInputWidth);
    const bool submitted = ImGui::InputText(
        "##browser.new-folder", state.newFolderName.data(), state.newFolderName.size(), ImGuiInputTextFlags_EnterReturnsTrue);
    if (!state.error.empty())
    {
        ImGui::TextColored(theme::imguiColor(theme::warning), "%s", state.error.c_str());
    }

    const std::string createLabel = iconLabel(IconId::ok, strings::create());
    const std::string cancelLabel = iconLabel(IconId::cancel, strings::cancel());
    beginDialogFooter({createLabel.c_str(), cancelLabel.c_str()}, false);
    const bool createPressed = dialogButton(createLabel.c_str());
    if ((submitted || createPressed) && createFolder(state))
    {
        state.createFolderVisible = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (dialogButton(cancelLabel.c_str()))
    {
        state.createFolderVisible = false;
        state.error.clear();
        ImGui::CloseCurrentPopup();
    }
    endDialogFooter();
    ImGui::EndPopup();
}

void rebuildBrowserListing(FileBrowserState & state)
{
    state.listingRows.clear();
    state.listingScanCount++;
    std::error_code error;
    for (const auto & entry : std::filesystem::directory_iterator(state.directory, error))
    {
        const auto & path = entry.path();
        const auto name = path.filename().string();
        if (!state.showHidden && !name.empty() && name.front() == '.')
        {
            continue;
        }
        const auto metadata = inspectFileBrowserEntry(path);
        if (metadata.directory || (metadata.regularFile && fileBrowserMatchesFilter(path, state.request.extension)))
        {
            state.listingRows.push_back(FileBrowserRowView{.path = path, .metadata = metadata});
        }
    }

    std::ranges::sort(state.listingRows,
                      [](const FileBrowserRowView & left, const FileBrowserRowView & right)
                      {
                          const bool leftDirectory = left.metadata.directory;
                          const bool rightDirectory = right.metadata.directory;
                          if (leftDirectory != rightDirectory)
                          {
                              return leftDirectory > rightDirectory;
                          }
                          return normalizeFileBrowserText(left.path.filename().string()) <
                                 normalizeFileBrowserText(right.path.filename().string());
                      });
    state.listingDirty = false;
}

std::optional<DialogResult> drawBrowserEntry(FileBrowserState & state, const FileBrowserRowView & row, std::uint64_t generation)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    const bool selected = state.selectedPath == row.path;
    const bool clicked = ImGui::Selectable(row.path.filename().string().c_str(), selected, ImGuiSelectableFlags_SpanAllColumns);
    const bool keyboardActivated = (ImGui::IsItemFocused() || selected) && ImGui::IsKeyPressed(ImGuiKey_Enter, false);
    if (clicked)
    {
        selectPath(state, row.path);
    }
    if ((ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) || keyboardActivated)
    {
        if (row.metadata.directory)
        {
            navigate(state, row.path, true);
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
    const auto size = row.metadata.size ? formatFileSize(*row.metadata.size) : "";
    ImGui::TextUnformatted(size.c_str());
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(row.metadata.modified.c_str());
    return std::nullopt;
}

std::optional<DialogResult> drawBrowserEntries(FileBrowserState & state, std::uint64_t generation)
{
    if (!ImGui::BeginTable("Entries", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH))
    {
        return std::nullopt;
    }

    ImGui::TableSetupColumn(strings::name());
    ImGui::TableSetupColumn(strings::sizeLabel(), ImGuiTableColumnFlags_WidthFixed, 100.0F);
    ImGui::TableSetupColumn(strings::dateLabel(), ImGuiTableColumnFlags_WidthFixed, dateColumnWidth);
    ImGui::TableHeadersRow();
    if (state.listingDirty)
    {
        rebuildBrowserListing(state);
    }
    for (const auto & row : state.listingRows)
    {
        if (const auto result = drawBrowserEntry(state, row, generation))
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
        ImGui::TableSetupColumn(strings::places(), ImGuiTableColumnFlags_WidthFixed, placesWidth);
        ImGui::TableSetupColumn(strings::files(), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(strings::summary(), ImGuiTableColumnFlags_WidthFixed, summaryWidth);
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

    const char * confirmLabel = strings::selectFolder();
    if (state.request.mode == FileBrowserMode::openFile)
    {
        confirmLabel = strings::open();
    }
    else if (state.request.mode == FileBrowserMode::saveFile)
    {
        confirmLabel = strings::save();
    }
    const std::string cancelButtonLabel = iconLabel(IconId::cancel, strings::cancel());

    if (state.request.mode == FileBrowserMode::saveFile)
    {
        ImGui::Separator();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(strings::filename());
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

    const std::string title =
        state.request.title.empty()
            ? iconLabel(IconId::fileBrowserDialog, std::string(strings::openMulFile()) + "###dialog.file-browser")
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
    drawCreateFolderPopup(state);
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
