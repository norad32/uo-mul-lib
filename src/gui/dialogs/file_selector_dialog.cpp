#include "gui/dialogs/file_selector_dialog.h"
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

void sortCandidates(FileSelectorState & state)
{
    std::ranges::sort(state.candidates, [](const auto & left, const auto & right) {
        if (left.openable != right.openable)
        {
            return left.openable > right.openable;
        }
        return left.score > right.score;
    });
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
        std::erase_if(state.candidates, [](const auto & candidate) {
            return candidate.kind != MulAssetKind::radarcol;
        });
    }
    else if (state.browserResult.target == FileBrowserTarget::texmapsIndex ||
             state.browserResult.target == FileBrowserTarget::texmapsData)
    {
        std::erase_if(state.candidates, [](const auto & candidate) {
            return candidate.kind != MulAssetKind::texmapsPair;
        });
    }
    sortCandidates(state);
    const auto firstOpenable = std::ranges::find_if(state.candidates, [](const auto & candidate) {
        return candidate.openable;
    });
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
    const auto lowerSelectedName = [&selectedName] {
        std::string value = selectedName;
        std::ranges::transform(value, value.begin(), [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
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

    ImGui::OpenPopup("Select MUL File Type###dialog.file-selector");
    if (!ImGui::BeginPopupModal("Select MUL File Type###dialog.file-selector", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return FileSelectorAction::none;
    }

    ImGui::TextWrapped("Choose how to open this MUL file:");
    ImGui::TextWrapped("%s", state.browserResult.path.string().c_str());
    ImGui::Separator();
    if (state.candidates.empty())
    {
        ImGui::TextColored(theme::imguiColor(theme::amber), "No supported MUL type was detected.");
    }
    for (std::size_t index = 0; index < state.candidates.size(); index++)
    {
        const auto & candidate = state.candidates.at(index);
        ImGui::BeginDisabled(!candidate.openable);
        const bool selected = state.selectedCandidate == index;
        if (ImGui::Selectable(candidate.label.c_str(), selected))
        {
            state.selectedCandidate = index;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextWrapped("%s", candidate.reason.c_str());
        if (candidate.kind == MulAssetKind::texmapsPair && !candidate.openable)
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("Choose companion"))
            {
                ImGui::EndPopup();
                return FileSelectorAction::chooseCompanion;
            }
        }
    }

    if (!state.error.empty())
    {
        ImGui::TextColored(theme::imguiColor(theme::amber), "%s", state.error.c_str());
    }
    const bool canAccept = state.selectedCandidate < state.candidates.size() &&
                           state.candidates.at(state.selectedCandidate).openable;
    ImGui::BeginDisabled(!canAccept);
    if (ImGui::Button("Open") || (canAccept && ImGui::IsKeyPressed(ImGuiKey_Enter)))
    {
        ImGui::EndDisabled();
        ImGui::EndPopup();
        return FileSelectorAction::accept;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Choose Different File"))
    {
        ImGui::EndPopup();
        return FileSelectorAction::chooseDifferent;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        ImGui::EndPopup();
        return FileSelectorAction::cancel;
    }
    ImGui::EndPopup();
    return FileSelectorAction::none;
}

void cancelFileSelector(FileSelectorState & state)
{
    state = {};
}

} // namespace uo_mule::gui
