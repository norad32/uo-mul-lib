#pragma once

#include "gui_model/dialogs/dialog_contracts.h"
#include <cstdint>
#include <optional>

namespace uo_mule::gui
{

struct ActiveDialog
{
    DialogId id = DialogId::none;
    std::uint64_t generation = 0;
};

struct DialogHostState
{
    std::optional<ActiveDialog> active;
    std::optional<DialogResult> result;

    [[nodiscard]] bool request(DialogRequest request);
    void activate(DialogRequest request);
    [[nodiscard]] bool complete(DialogResult completedResult);
    void cancel();
    [[nodiscard]] std::optional<DialogResult> consumeResult();
    [[nodiscard]] bool owns(DialogId id) const noexcept;

    std::uint64_t nextGeneration = 1;
};

} // namespace uo_mule::gui
