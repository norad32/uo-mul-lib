#pragma once

#include "uomul/map.h"
#include <expected>

namespace uomul::detail
{

[[nodiscard]] std::expected<MapStats, Error> collectStats(const MapReader & reader, TileRegion region);

} // namespace uomul::detail
