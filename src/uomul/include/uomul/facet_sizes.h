#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace uomul
{

struct FacetSize
{
    std::string_view name;
    const char * label;
    const char * mapLabel;
    std::uint32_t widthTiles;
    std::uint32_t heightTiles;

    bool operator==(const FacetSize & other) const
    {
        return name == other.name && std::string_view(label) == other.label && std::string_view(mapLabel) == other.mapLabel &&
               widthTiles == other.widthTiles && heightTiles == other.heightTiles;
    }
};

[[nodiscard]] std::uint64_t facetMapFileBytes(const FacetSize & facet) noexcept;

[[nodiscard]] std::vector<FacetSize> facetSizesForMapFileBytes(std::uintmax_t fileBytes);

/** @brief Returns the canonical facet presets used by the application and libraries. */
[[nodiscard]] std::span<const FacetSize> knownFacetSizes() noexcept;

/** @brief Looks up a facet preset by canonical name or supported alias. */
[[nodiscard]] std::optional<FacetSize> facetSize(std::string_view name);

} // namespace uomul
