#include "gui/context/color_conversion.h"
#include "gui/documents/map_workspace.h"
#include "uomul/radarcol/radarcol.h"
#include <cstdint>

namespace uo_mule::gui
{
namespace
{

constexpr std::uint32_t colorChannelMask = 0x1FU;
constexpr std::uint32_t colorRedShift = 10U;
constexpr std::uint32_t colorGreenShift = 5U;
constexpr std::uint32_t colorChannelMaximum = 255U;
constexpr std::uint32_t colorChannelValueMaximum = 31U;
[[nodiscard]] std::uint8_t expandFiveBitChannel(std::uint32_t channel) noexcept
{
    return static_cast<std::uint8_t>(((channel * colorChannelMaximum) + (colorChannelMask / 2U)) / colorChannelValueMaximum);
}

} // namespace

MapColor radarColorToMapColor(uomul::RadarColor color) noexcept
{
    const std::uint32_t packed = static_cast<std::uint32_t>(color);
    return {
        .red = expandFiveBitChannel((packed >> colorRedShift) & colorChannelMask),
        .green = expandFiveBitChannel((packed >> colorGreenShift) & colorChannelMask),
        .blue = expandFiveBitChannel(packed & colorChannelMask),
    };
}

MapColor texturePixelToMapColor(std::uint16_t pixel) noexcept
{
    const std::uint32_t packed = pixel;
    return {
        .red = expandFiveBitChannel((packed >> colorRedShift) & colorChannelMask),
        .green = expandFiveBitChannel((packed >> colorGreenShift) & colorChannelMask),
        .blue = expandFiveBitChannel(packed & colorChannelMask),
    };
}

} // namespace uo_mule::gui
