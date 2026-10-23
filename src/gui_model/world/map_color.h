#pragma once

#include <cstdint>

namespace uo_mule::gui
{

struct MapColor
{
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
    bool operator==(const MapColor &) const = default;
};

} // namespace uo_mule::gui
