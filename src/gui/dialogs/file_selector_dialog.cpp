#include "gui/dialogs/file_selector_dialog.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/presentation/dialog_presentation.h"
#include "gui/presentation/iconography.h"
#include "gui/presentation/strings.h"
#include "gui/presentation/theme.h"
#include "gui_model/dialogs/dialog_contracts.h"
#include "gui_model/dialogs/dialog_host.h"
#include "gui_model/dialogs/file_deduction.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <imgui.h>
#include <iterator>
#include <optional>
#include <string>
#include <utility>

namespace uo_mule::gui
{

namespace
{

enum class CandidateAction : std::uint8_t
{
    none,
    chooseCompanion,
    openMapSetup,
};

CandidateAction drawCandidate(FileSelectorState & state, std::size_t index)
{
    const auto & candidate = state.candidates.at(index);
    const bool selected = state.selectedCandidate == index;
    const std::string selectableLabel = candidate.label + "##candidate-" + std::to_string(index);
    const bool setupCandidate = candidate.kind == MulAssetKind::map && !candidate.dimensions;
    ImGui::BeginDisabled(!candidate.openable && !setupCandidate);
    if (ImGui::Selectable(selectableLabel.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
    {
        state.selectedCandidate = index;
        if (setupCandidate)
        {
            ImGui::EndDisabled();
            return CandidateAction::openMapSetup;
        }
    }
    ImGui::EndDisabled();
    if (candidate.kind == MulAssetKind::map && candidate.dimensions)
    {
        ImGui::SameLine();
        ImGui::TextDisabled(
            "%s %u x %u", candidate.facet.c_str(), candidate.dimensions->widthTiles, candidate.dimensions->heightTiles);
    }
    else if (candidate.kind == MulAssetKind::radarcol || candidate.kind == MulAssetKind::texmapsSet)
    {
        ImGui::SameLine();
        ImGui::TextDisabled(strings::entriesFormat(), candidate.itemCount);
    }
    if (!candidate.warning.empty())
    {
        ImGui::Indent();
        ImGui::TextColored(theme::imguiColor(theme::warning), "%s", candidate.warning.c_str());
        ImGui::Unindent();
    }
    if (!candidate.filenameWarning.empty())
    {
        ImGui::Indent();
        ImGui::TextColored(theme::imguiColor(theme::warning), "%s", candidate.filenameWarning.c_str());
        ImGui::Unindent();
    }
    ImGui::Indent();
    ImGui::TextColored(candidate.openable ? theme::imguiColor(theme::success) : theme::imguiColor(theme::warning),
                       "%s",
                       candidate.openable ? "Openable" : "Requires selection");
    ImGui::Unindent();
    if (candidate.kind == MulAssetKind::texmapsSet && !candidate.openable && ImGui::SmallButton("Choose companion"))
    {
        return CandidateAction::chooseCompanion;
    }
    return CandidateAction::none;
}

} // namespace

void openFileSelector(FileSelectorState & state, FileBrowserResult result)
{
    auto * const dialogHost = state.dialogHost;
    state = {};
    state.dialogHost = dialogHost;
    state.visible = true;
    state.browserResult = std::move(result);
    state.candidates = state.browserResult.assetCandidates;
    sortFileBrowserCandidates(state.candidates);
    state.selectedCandidate = state.candidates.size();
    const auto firstOpenable =
        std::ranges::find_if(state.candidates, [](const auto & candidate) { return candidate.openable; });
    if (firstOpenable != state.candidates.end())
    {
        state.selectedCandidate = static_cast<std::size_t>(std::distance(state.candidates.begin(), firstOpenable));
    }
    activateFileSelector(state);
}

void activateFileSelector(const FileSelectorState & state)
{
    if (state.dialogHost == nullptr)
    {
        return;
    }
    auto request = makeDialogRequest(DialogId::fileSelector);
    request.replaceActive = true;
    static_cast<void>(state.dialogHost->request(std::move(request)));
}

FileBrowserResult selectedFileSelectorResult(const FileSelectorState & state)
{
    auto result = state.browserResult;
    if (state.selectedCandidate >= state.candidates.size())
    {
        return result;
    }

    result.assetCandidate = state.candidates.at(state.selectedCandidate);
    if (result.assetCandidate->kind == MulAssetKind::texmapsSet)
    {
        result.path = result.assetCandidate->path;
    }
    return result;
}

bool applyFileSelectorCompanion(FileSelectorState & state, const std::filesystem::path & path)
{
    const auto set = inspectTexmapsCompanion(state.browserResult.path, path);
    if (!set.openable)
    {
        state.error = set.reason;
        return false;
    }

    state.candidates = {set};
    state.selectedCandidate = 0;
    state.error.clear();
    return true;
}

std::optional<DialogResult> drawFileSelector(FileSelectorState & state, std::uint64_t generation)
{
    if (!state.visible)
    {
        return std::nullopt;
    }

    if (!beginAutoSizeDialog("Choose File Type###dialog.file-selector", &state.visible))
    {
        if (!state.visible)
        {
            return DialogResult{
                .id = DialogId::fileSelector,
                .action = DialogAction::cancel,
                .payload = {},
                .generation = generation,
            };
        }
        return std::nullopt;
    }

    if (state.candidates.empty())
    {
        ImGui::TextColored(theme::imguiColor(theme::warning), "No supported MUL type was detected.");
    }
    for (std::size_t index = 0; index < state.candidates.size(); index++)
    {
        const auto action = drawCandidate(state, index);
        if (action == CandidateAction::chooseCompanion || action == CandidateAction::openMapSetup)
        {
            endDialog();
            if (action == CandidateAction::openMapSetup)
            {
                return DialogResult{
                    .id = DialogId::fileSelector,
                    .action = DialogAction::accept,
                    .payload = selectedFileSelectorResult(state),
                    .generation = generation,
                };
            }
            return DialogResult{
                .id = DialogId::fileSelector,
                .action = DialogAction::chooseCompanion,
                .payload = state.browserResult,
                .generation = generation,
            };
        }
    }

    if (!state.error.empty())
    {
        ImGui::TextColored(theme::imguiColor(theme::warning), "%s", state.error.c_str());
    }
    const bool canAccept =
        state.selectedCandidate < state.candidates.size() && state.candidates.at(state.selectedCandidate).openable;
    beginDialogFooter({strings::ok(), strings::cancel()}, false);
    if (dialogButton(iconLabel(IconId::ok, strings::ok()).c_str(), canAccept) ||
        (canAccept && ImGui::IsKeyPressed(ImGuiKey_Enter)))
    {
        endDialogFooter();
        endDialog();
        return DialogResult{
            .id = DialogId::fileSelector,
            .action = DialogAction::accept,
            .payload = selectedFileSelectorResult(state),
            .generation = generation,
        };
    }
    ImGui::SameLine();
    if (dialogButton(iconLabel(IconId::cancel, strings::cancel()).c_str()))
    {
        endDialogFooter();
        endDialog();
        return DialogResult{
            .id = DialogId::fileSelector,
            .action = DialogAction::cancel,
            .payload = {},
            .generation = generation,
        };
    }
    endDialogFooter();
    endDialog();
    if (!state.visible)
    {
        return DialogResult{
            .id = DialogId::fileSelector,
            .action = DialogAction::cancel,
            .payload = {},
            .generation = generation,
        };
    }
    return std::nullopt;
}

void cancelFileSelector(FileSelectorState & state)
{
    auto * const dialogHost = state.dialogHost;
    state = {};
    state.dialogHost = dialogHost;
}

} // namespace uo_mule::gui
