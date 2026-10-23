#include "gui/dialogs/file_selector_dialog.h"
#include "gui/dialogs/dialog_presentation.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/frontend/theme.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <imgui.h>
#include <iterator>
#include <string>
#include <utility>

namespace uo_mule::gui
{

namespace
{

FileSelectorAction drawCandidate(FileSelectorState & state, std::size_t index)
{
    const auto & candidate = state.candidates.at(index);
    const bool selected = state.selectedCandidate == index;
    const std::string selectableLabel = candidate.label + "##candidate-" + std::to_string(index);
    ImGui::BeginDisabled(!candidate.openable);
    if (ImGui::Selectable(selectableLabel.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
    {
        state.selectedCandidate = index;
    }
    ImGui::EndDisabled();
    if (candidate.kind == MulAssetKind::map && candidate.dimensions)
    {
        ImGui::SameLine();
        ImGui::TextDisabled(
            "%s %u x %u", candidate.facet.c_str(), candidate.dimensions->widthTiles, candidate.dimensions->heightTiles);
    }
    else if (candidate.kind == MulAssetKind::radarcol || candidate.kind == MulAssetKind::texmapsPair)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%u entries", candidate.itemCount);
    }
    if (!candidate.warning.empty())
    {
        ImGui::Indent();
        ImGui::TextColored(theme::imguiColor(theme::warning), "%s", candidate.warning.c_str());
        ImGui::Unindent();
    }
    ImGui::Indent();
    ImGui::TextColored(candidate.openable ? theme::imguiColor(theme::success) : theme::imguiColor(theme::warning),
                       "%s",
                       candidate.openable ? "Openable" : "Requires selection");
    ImGui::Unindent();
    if (candidate.kind == MulAssetKind::texmapsPair && !candidate.openable && ImGui::SmallButton("Choose companion"))
    {
        return FileSelectorAction::chooseCompanion;
    }
    return FileSelectorAction::none;
}

} // namespace

void openFileSelector(FileSelectorState & state, FileBrowserResult result)
{
    state = {};
    state.visible = true;
    state.browserResult = std::move(result);
    state.candidates = state.browserResult.assetCandidates;
    if (state.browserResult.target == FileBrowserTarget::radarcolSource)
    {
        std::erase_if(state.candidates, [](const auto & candidate) { return candidate.kind != MulAssetKind::radarcol; });
    }
    else if (state.browserResult.target == FileBrowserTarget::texmapsIndex ||
             state.browserResult.target == FileBrowserTarget::texmapsData)
    {
        std::erase_if(state.candidates, [](const auto & candidate) { return candidate.kind != MulAssetKind::texmapsPair; });
    }
    sortFileBrowserCandidates(state.candidates);
    const auto firstOpenable =
        std::ranges::find_if(state.candidates, [](const auto & candidate) { return candidate.openable; });
    if (firstOpenable != state.candidates.end())
    {
        state.selectedCandidate = static_cast<std::size_t>(std::distance(state.candidates.begin(), firstOpenable));
    }
}

FileBrowserResult selectedFileSelectorResult(const FileSelectorState & state)
{
    auto result = state.browserResult;
    if (state.selectedCandidate >= state.candidates.size())
    {
        return result;
    }

    result.assetCandidate = state.candidates.at(state.selectedCandidate);
    if (result.assetCandidate->kind == MulAssetKind::texmapsPair)
    {
        result.path = result.assetCandidate->path;
    }
    return result;
}

bool applyFileSelectorCompanion(FileSelectorState & state, const std::filesystem::path & path)
{
    const auto selectedName = state.browserResult.path.filename().string();
    const auto lowerSelectedName = [&selectedName]
    {
        std::string value = selectedName;
        std::ranges::transform(
            value, value.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        return value;
    }();
    const bool selectedIsIndex = lowerSelectedName == "texidx.mul";
    const auto pair = selectedIsIndex ? inspectTexmapsPair(state.browserResult.path, path)
                                      : inspectTexmapsPair(path, state.browserResult.path);
    if (!pair.openable)
    {
        state.error = pair.reason;
        return false;
    }

    state.candidates = {pair};
    state.selectedCandidate = 0;
    state.error.clear();
    return true;
}

FileSelectorAction drawFileSelector(FileSelectorState & state)
{
    if (!state.visible)
    {
        return FileSelectorAction::none;
    }

    if (!beginDialog("Choose File Type###dialog.file-selector", nullptr, selectorDialogSpec()))
    {
        return FileSelectorAction::none;
    }

    if (state.candidates.empty())
    {
        ImGui::TextColored(theme::imguiColor(theme::warning), "No supported MUL type was detected.");
    }
    for (std::size_t index = 0; index < state.candidates.size(); index++)
    {
        if (drawCandidate(state, index) == FileSelectorAction::chooseCompanion)
        {
            ImGui::EndPopup();
            return FileSelectorAction::chooseCompanion;
        }
    }

    if (!state.error.empty())
    {
        ImGui::TextColored(theme::imguiColor(theme::warning), "%s", state.error.c_str());
    }
    const bool canAccept =
        state.selectedCandidate < state.candidates.size() && state.candidates.at(state.selectedCandidate).openable;
    beginDialogFooter({"Open", "Cancel"});
    if (dialogButton("Open", canAccept) || (canAccept && ImGui::IsKeyPressed(ImGuiKey_Enter)))
    {
        endDialogFooter();
        ImGui::EndPopup();
        return FileSelectorAction::accept;
    }
    ImGui::SameLine();
    if (dialogButton("Cancel"))
    {
        endDialogFooter();
        ImGui::EndPopup();
        return FileSelectorAction::cancel;
    }
    endDialogFooter();
    endDialog();
    return FileSelectorAction::none;
}

void cancelFileSelector(FileSelectorState & state)
{
    state = {};
}

} // namespace uo_mule::gui
