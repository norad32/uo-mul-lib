#include "gui/dialogs/dialog_host.h"
#include "gui/dialogs/dialog_contracts.h"
#include <optional>
#include <utility>

namespace uo_mule::gui
{

bool DialogHostState::request(DialogRequest request)
{
    if (active && active->id != request.id && !request.replaceActive)
    {
        return false;
    }
    activate(std::move(request));
    return true;
}

void DialogHostState::activate(DialogRequest request)
{
    request.popupId = dialogPopupId(request.id);
    if (active && active->id == request.id)
    {
        active->payload = std::move(request.payload);
        active->replaceActive = request.replaceActive;
        return;
    }
    active = std::move(request);
    result.reset();
}

void DialogHostState::complete(DialogResult completedResult)
{
    result = std::move(completedResult);
    active.reset();
}

void DialogHostState::cancel()
{
    if (active)
    {
        complete({.id = active->id, .action = DialogAction::cancel, .payload = {}});
    }
}

std::optional<DialogResult> DialogHostState::consumeResult()
{
    auto completed = std::move(result);
    result.reset();
    return completed;
}

bool DialogHostState::owns(DialogId id) const noexcept
{
    return active && active->id == id;
}

} // namespace uo_mule::gui
