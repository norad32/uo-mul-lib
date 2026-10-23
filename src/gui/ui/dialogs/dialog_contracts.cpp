#include "gui/ui/dialogs/dialog_contracts.h"
#include <utility>

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

DialogRequest makeDialogRequest(DialogId id, DialogRequestPayload payload)
{
    return {.id = id, .popupId = dialogPopupId(id), .payload = std::move(payload), .replaceActive = false};
}

} // namespace uo_mule::gui
