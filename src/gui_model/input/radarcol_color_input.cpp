#include "gui_model/input/radarcol_color_input.h"
#include "uomul/radarcol/radarcol.h"
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <system_error>

namespace uo_mule::gui
{
namespace
{

constexpr std::uint32_t rgbByteMaximum = 255U;
constexpr std::uint32_t rgbByteRounding = 15U;
constexpr std::size_t rgbHexTextLength = 7U;

[[nodiscard]] std::expected<std::uint32_t, std::string>
parseUnsigned(std::string_view input, int base, std::string_view description)
{
    if (input.empty())
    {
        return std::unexpected(std::string(description) + " cannot be empty");
    }

    std::uint32_t value = 0;
    const auto result = std::from_chars(input.data(), input.data() + input.size(), value, base);
    if (result.ec != std::errc{} || result.ptr != input.data() + input.size())
    {
        return std::unexpected(std::string(description) + " contains invalid characters");
    }
    return value;
}

[[nodiscard]] std::uint8_t channelByte(std::uint32_t channel) noexcept
{
    return static_cast<std::uint8_t>(((channel * rgbByteMaximum) + rgbByteRounding) / uomul::RadarColorConverter::channelMask);
}

} // namespace

std::expected<std::uint8_t, std::string> parseRadarcolChannel(std::string_view input)
{
    const auto value = parseUnsigned(input, 10, "RGB channel");
    if (!value)
    {
        return std::unexpected(value.error());
    }
    if (*value > rgbByteMaximum)
    {
        return std::unexpected("RGB channels must be between 0 and 255");
    }
    return static_cast<std::uint8_t>(*value);
}

std::expected<RadarcolRgb888, std::string>
parseRadarcolRgbChannels(std::string_view red, std::string_view green, std::string_view blue)
{
    const auto redValue = parseRadarcolChannel(red);
    const auto greenValue = parseRadarcolChannel(green);
    const auto blueValue = parseRadarcolChannel(blue);
    if (!redValue || !greenValue || !blueValue)
    {
        if (!redValue)
        {
            return std::unexpected(redValue.error());
        }
        if (!greenValue)
        {
            return std::unexpected(greenValue.error());
        }
        return std::unexpected(blueValue.error());
    }
    return RadarcolRgb888{.red = *redValue, .green = *greenValue, .blue = *blueValue};
}

std::expected<uomul::RadarColor, std::string> parseRadarcolRgbHex(std::string_view input)
{
    if (input.size() != rgbHexTextLength || input.front() != '#')
    {
        return std::unexpected("RGB hex must use #RRGGBB");
    }
    const auto value = parseUnsigned(input.substr(1U), 16, "RGB hex");
    if (!value)
    {
        return std::unexpected(value.error());
    }
    return uomul::RadarColorConverter::fromRgb24(*value);
}

std::expected<uomul::RadarColor, std::string> parseRadarcolRaw(std::string_view input)
{
    if (input.size() != 4U)
    {
        return std::unexpected("raw color must use four hexadecimal digits");
    }
    const auto value = parseUnsigned(input, 16, "raw color");
    if (!value)
    {
        return std::unexpected(value.error());
    }
    if (*value > uomul::RadarColorConverter::colorMask)
    {
        return std::unexpected("raw color must be between 0000 and 7FFF");
    }
    return static_cast<uomul::RadarColor>(*value);
}

RadarcolRgb888 radarcolRgb888(uomul::RadarColor color) noexcept
{
    const auto packed = static_cast<std::uint32_t>(color);
    return {
        .red = channelByte((packed >> uomul::RadarColorConverter::redShift) & uomul::RadarColorConverter::channelMask),
        .green = channelByte((packed >> uomul::RadarColorConverter::greenShift) & uomul::RadarColorConverter::channelMask),
        .blue = channelByte(packed & uomul::RadarColorConverter::channelMask),
    };
}

std::string formatRadarcolRgbHex(uomul::RadarColor color)
{
    const auto rgb = radarcolRgb888(color);
    return std::format("#{:02X}{:02X}{:02X}", rgb.red, rgb.green, rgb.blue);
}

std::string formatRadarcolRaw(uomul::RadarColor color)
{
    return std::format("{:04X}", static_cast<std::uint32_t>(color) & uomul::RadarColorConverter::colorMask);
}

} // namespace uo_mule::gui
