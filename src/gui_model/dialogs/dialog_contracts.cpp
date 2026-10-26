#include "gui_model/dialogs/dialog_contracts.h"

namespace uo_mule::gui
{

const char * dialogPopupId(DialogId id) noexcept
{
    switch (id)
    {
        case DialogId::mapWorkflow :
            return "###dialog.map-workflow";
        case DialogId::preferences :
            return "###dialog.preferences";
        case DialogId::fileBrowser :
            return "###dialog.file-browser";
        case DialogId::fileSelector :
            return "###dialog.file-selector";
        case DialogId::none :
            break;
    }
    return "###dialog.none";
}

DialogRequest makeDialogRequest(DialogId id)
{
    return {.id = id, .popupId = dialogPopupId(id), .replaceActive = false, .generation = 0};
}

} // namespace uo_mule::gui
