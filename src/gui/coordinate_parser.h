#pragma once

#include "uomul/map.h"
#include <expected>
#include <string>
#include <string_view>

namespace uo_mule::gui
{

enum class CoordinateKind : std::uint8_t
{
    tile,
    block,
};

struct ParsedCoordinate
{
    CoordinateKind kind = CoordinateKind::tile;
    uomul::TileCoord value{};
};

[[nodiscard]] std::expected<ParsedCoordinate, std::string> parseCoordinate(std::string_view input);

} // namespace uo_mule::gui
