#pragma once

#include "uomul/map.h"
#include "uomul/mul_common/mul_common.h"
#include <expected>

namespace uomul::detail
{

[[nodiscard]] std::expected<MapStats, Error> collectStats(const MapReader & reader, TileRegion region);

} // namespace uomul::detail
