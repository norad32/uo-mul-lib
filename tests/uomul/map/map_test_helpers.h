#pragma once

#include "../test_helpers.h"
#include "uomul/map/map.h"
#include <cstdint>

namespace uomul::test
{

inline constexpr uomul::Dimensions dimensions{.widthTiles = 16, .heightTiles = 16};
inline constexpr std::uint32_t defaultFixtureBlockCount = 4;

void writeFixture(const std::filesystem::path & path, std::uint32_t blocks = defaultFixtureBlockCount);

} // namespace uomul::test
