#pragma once

#include <cstdint>

namespace uo_mule::gui
{

struct NewFileDialogState;

enum class MapFacetDialogResult : std::uint8_t
{
    none,
    accept,
    cancel,
};

[[nodiscard]] MapFacetDialogResult drawMapFacetDialog(NewFileDialogState & state);

} // namespace uo_mule::gui
