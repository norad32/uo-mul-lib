#pragma once

#include "gui_model/providers/data_providers.h"
#include "uomul/radarcol/radarcol.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

namespace uo_mule::gui
{

inline constexpr std::string_view embeddedRadarcolSourceFile = "radarcol.mul";
inline constexpr std::string_view embeddedRadarcolSourceSha256 =
    "098f339b42f9cbaf298bea9cad8efb8bf51b71981e9e72cdb78e2c77868befde";
inline constexpr std::string_view embeddedRadarcolPayloadSha256 =
    "68ef6f81484f8d220b030228de073611c1042de7c9b1d6bab54feb94bcd0829e";
inline constexpr std::uint32_t embeddedRadarcolSourceColorCount = 81884;

[[nodiscard]] constexpr std::uint32_t embeddedRadarcolColorCount() noexcept
{
    return uomul::radarColorCount;
}

[[nodiscard]] std::optional<uomul::RadarColor> embeddedRadarcolColor(std::uint32_t index) noexcept;
[[nodiscard]] std::unique_ptr<RadarColorProvider> makeEmbeddedRadarColorProvider();

} // namespace uo_mule::gui
