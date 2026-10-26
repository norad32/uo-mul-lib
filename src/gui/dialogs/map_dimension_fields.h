#pragma once

#include "uomul/facet_sizes.h"
#include "uomul/map/map.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>

namespace uo_mule::gui
{

struct MapDimensionFieldLayout
{
    float fieldWidth = 0.0F;
    std::optional<float> labelWidth = std::nullopt;
};

[[nodiscard]] const uomul::FacetSize * mapFacetAt(std::size_t index) noexcept;
[[nodiscard]] const char * mapFacetLabel(std::size_t index, bool useMapLabel) noexcept;
[[nodiscard]] std::expected<uomul::Dimensions, std::string>
resolveMapDimensions(int preset, std::uint32_t width, std::uint32_t height);
[[nodiscard]] bool drawMapDimensionFields(int & preset,
                                          std::uint32_t & width,
                                          std::uint32_t & height,
                                          bool useMapLabel,
                                          MapDimensionFieldLayout layout);

} // namespace uo_mule::gui
