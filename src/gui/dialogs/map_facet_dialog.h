#pragma once

#include <cstdint>

namespace uo_mule::gui
{

struct NewMapDialogState;

enum class MapFacetDialogResult : std::uint8_t
{
    none,
    accept,
    cancel,
};

[[nodiscard]] MapFacetDialogResult drawMapFacetDialog(NewMapDialogState & state);

} // namespace uo_mule::gui
