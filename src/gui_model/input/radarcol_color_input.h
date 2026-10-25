#pragma once

#include "uomul/radarcol/radarcol.h"
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace uo_mule::gui
{

struct RadarcolRgb888
{
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
};

[[nodiscard]] std::expected<std::uint8_t, std::string> parseRadarcolChannel(std::string_view input);
[[nodiscard]] std::expected<RadarcolRgb888, std::string>
parseRadarcolRgbChannels(std::string_view red, std::string_view green, std::string_view blue);
[[nodiscard]] std::expected<uomul::RadarColor, std::string> parseRadarcolRgbHex(std::string_view input);
[[nodiscard]] std::expected<uomul::RadarColor, std::string> parseRadarcolRaw(std::string_view input);
[[nodiscard]] RadarcolRgb888 radarcolRgb888(uomul::RadarColor color) noexcept;
[[nodiscard]] std::string formatRadarcolRgbHex(uomul::RadarColor color);
[[nodiscard]] std::string formatRadarcolRaw(uomul::RadarColor color);

} // namespace uo_mule::gui
