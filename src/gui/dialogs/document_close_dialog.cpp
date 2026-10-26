#include "gui/dialogs/document_close_dialog.h"
#include "gui/presentation/dialog_presentation.h"
#include "gui/presentation/iconography.h"
#include "gui/presentation/strings.h"
#include "gui/presentation/theme.h"
#include <imgui.h>

namespace uo_mule::gui
{

void drawDocumentCloseDialog(DocumentCloseDialogState & state)
{
    if (!beginAutoSizeDialog("###dialog.pending-document-close", &state.visible, ImGuiWindowFlags_NoTitleBar))
    {
        return;
    }

    const std::string savePrompt = strings::formatSaveChangesPrompt(state.filename);
    ImGui::TextUnformatted(savePrompt.c_str());
    ImGui::TextColored(theme::imguiColor(theme::mutedText), "%s", strings::discardChangesWarning());

    const std::string discardLabel = iconLabel(IconId::deleteIcon, strings::discard());
    const std::string cancelLabel = iconLabel(IconId::cancel, strings::cancel());
    const std::string saveLabel = iconLabel(IconId::save, strings::save());
    beginDialogFooter({discardLabel.c_str(), cancelLabel.c_str(), saveLabel.c_str()});
    if (dialogButton(discardLabel.c_str()))
    {
        if (state.actionRequested)
        {
            state.actionRequested(PendingChangesAction::discard);
        }
    }
    ImGui::SameLine();

    if (dialogButton(cancelLabel.c_str()))
    {
        state.visible = false;
    }
    ImGui::SameLine();

    if (dialogButton(saveLabel.c_str()))
    {
        if (state.actionRequested)
        {
            state.actionRequested(PendingChangesAction::save);
        }
    }
    endDialogFooter();

    endDialog();
    if (!state.visible && state.cancelRequested)
    {
        state.cancelRequested();
    }
}

} // namespace uo_mule::gui
