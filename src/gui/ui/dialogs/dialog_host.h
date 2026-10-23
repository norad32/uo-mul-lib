#pragma once

#include "gui/ui/dialogs/dialog_contracts.h"
#include <optional>

namespace uo_mule::gui
{

struct DialogHostState
{
    std::optional<DialogRequest> active;
    std::optional<DialogResult> result;

    [[nodiscard]] bool request(DialogRequest request);
    void activate(DialogRequest request);
    void complete(DialogResult completedResult);
    void cancel();
    [[nodiscard]] std::optional<DialogResult> consumeResult();
    [[nodiscard]] bool owns(DialogId id) const noexcept;
};

} // namespace uo_mule::gui
