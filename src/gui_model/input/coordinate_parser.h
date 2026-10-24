#pragma once

#include "uomul/map/map.h"
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

[[nodiscard]] std::expected<ParsedCoordinate, std::string> parseCoordinate(std::string_view input, CoordinateKind kind);

} // namespace uo_mule::gui
