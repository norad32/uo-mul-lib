#include "gui/dialogs/file_selector_dialog.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/presentation/dialog/dialog_presentation.h"
#include "gui/presentation/theme/theme.h"
#include "gui_model/dialogs/dialog_contracts.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <imgui.h>
#include <iterator>
#include <optional>
#include <string>
#include <utility>

namespace uo_mule::gui
{

namespace
{

bool drawCandidate(FileSelectorState & state, std::size_t index)
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
    return candidate.kind == MulAssetKind::texmapsPair && !candidate.openable && ImGui::SmallButton("Choose companion");
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

std::optional<DialogResult> drawFileSelector(FileSelectorState & state)
{
    if (!state.visible)
    {
        return std::nullopt;
    }

    if (!beginDialog("Choose File Type###dialog.file-selector", &state.visible, selectorDialogSpec()))
    {
        if (!state.visible)
        {
            return DialogResult{.id = DialogId::fileSelector, .action = DialogAction::cancel, .payload = {}};
        }
        return std::nullopt;
    }

    if (state.candidates.empty())
    {
        ImGui::TextColored(theme::imguiColor(theme::warning), "No supported MUL type was detected.");
    }
    for (std::size_t index = 0; index < state.candidates.size(); index++)
    {
        if (drawCandidate(state, index))
        {
            ImGui::EndPopup();
            return DialogResult{
                .id = DialogId::fileSelector,
                .action = DialogAction::chooseCompanion,
                .payload = state.browserResult,
            };
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
        return DialogResult{
            .id = DialogId::fileSelector,
            .action = DialogAction::accept,
            .payload = selectedFileSelectorResult(state),
        };
    }
    ImGui::SameLine();
    if (dialogButton("Cancel"))
    {
        endDialogFooter();
        ImGui::EndPopup();
        return DialogResult{.id = DialogId::fileSelector, .action = DialogAction::cancel, .payload = {}};
    }
    endDialogFooter();
    endDialog();
    if (!state.visible)
    {
        return DialogResult{.id = DialogId::fileSelector, .action = DialogAction::cancel, .payload = {}};
    }
    return std::nullopt;
}

void cancelFileSelector(FileSelectorState & state)
{
    state = {};
}

} // namespace uo_mule::gui
