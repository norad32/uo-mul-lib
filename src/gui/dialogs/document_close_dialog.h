#pragma once

#include "gui_model/documents/map_document_state.h"
#include <functional>
#include <string>

namespace uo_mule::gui
{

struct DocumentCloseDialogState
{
    bool visible = true;
    std::string filename;
    std::function<void(PendingChangesAction)> actionRequested;
    std::function<void()> cancelRequested;
};

void drawDocumentCloseDialog(DocumentCloseDialogState & state);

} // namespace uo_mule::gui
