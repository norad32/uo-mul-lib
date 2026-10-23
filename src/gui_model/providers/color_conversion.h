#pragma once

#include "uomul/map.h"
#include "uomul/radarcol/radarcol.h"
#include <cstdint>

namespace uo_mule::gui
{

struct MapColor;

[[nodiscard]] MapColor radarColorToMapColor(uomul::RadarColor color) noexcept;
[[nodiscard]] MapColor texturePixelToMapColor(std::uint16_t pixel) noexcept;

} // namespace uo_mule::gui
