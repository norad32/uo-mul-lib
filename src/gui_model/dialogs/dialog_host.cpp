#include "gui_model/dialogs/dialog_host.h"
#include "gui_model/dialogs/dialog_contracts.h"
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
        active->replaceActive = request.replaceActive;
        return;
    }
    request.generation = nextGeneration++;
    active = std::move(request);
    result.reset();
}

bool DialogHostState::complete(DialogResult completedResult)
{
    if (!active || completedResult.id != active->id || completedResult.generation != active->generation)
    {
        return false;
    }
    result = std::move(completedResult);
    active.reset();
    return true;
}

void DialogHostState::cancel()
{
    if (active)
    {
        static_cast<void>(
            complete({.id = active->id, .action = DialogAction::cancel, .payload = {}, .generation = active->generation}));
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
