#include "core/facet_sizes.h"
#include <array>
#include <cctype>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace uo_mule::core
{
namespace
{

constexpr std::array<FacetSize, 6> facetSizes{
    FacetSize{
        .name = "legacy",
        .label = "Legacy",
        .mapLabel = "Trammel/Felucca",
        .widthTiles = 6144,
        .heightTiles = 4096,
    },
    FacetSize{
        .name = "ml",
        .label = "ML",
        .mapLabel = "Trammel/Felucca (ML)",
        .widthTiles = 7168,
        .heightTiles = 4096,
    },
    FacetSize{
        .name = "ilshenar", .label = "Ilshenar", .mapLabel = "Ilshenar", .widthTiles = 2304, .heightTiles = 1600,
    },
    FacetSize{.name = "malas", .label = "Malas", .mapLabel = "Malas", .widthTiles = 2560, .heightTiles = 2048},
    FacetSize{
        .name = "tokuno", .label = "Tokuno", .mapLabel = "Tokuno", .widthTiles = 1448, .heightTiles = 1448,
    },
    FacetSize{
        .name = "ter-mur", .label = "Ter-Mur", .mapLabel = "Ter-Mur", .widthTiles = 1280, .heightTiles = 4096,
    },
};

[[nodiscard]] std::string normalize(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    for (const char character : value)
    {
        const auto unsignedCharacter = static_cast<unsigned char>(character);
        if (std::isalnum(unsignedCharacter) != 0)
        {
            result.push_back(static_cast<char>(std::tolower(unsignedCharacter)));
        }
    }
    return result;
}

[[nodiscard]] bool isMondainsLegacyAlias(const std::string & name) noexcept
{
    return name == "ml" || name == "mondainslegacy" || name == "mondainlegacy";
}

[[nodiscard]] bool isLegacyTrammelAlias(const std::string & name) noexcept
{
    return name == "legacy" || name == "legacytrammel" || name == "felucca" || name == "trammel";
}

} // namespace

std::span<const FacetSize> knownFacetSizes() noexcept
{
    return facetSizes;
}

std::uint64_t facetMapFileBytes(const FacetSize & facet) noexcept
{
    const auto blockWidth = static_cast<std::uint64_t>(facet.widthTiles) / mapBlockDimension;
    const auto blockHeight = static_cast<std::uint64_t>(facet.heightTiles) / mapBlockDimension;
    return blockWidth * blockHeight * mapBlockByteCount;
}

std::vector<FacetSize> facetSizesForMapFileBytes(std::uintmax_t fileBytes)
{
    std::vector<FacetSize> matches;
    for (const auto & facet : facetSizes)
    {
        if (fileBytes == facetMapFileBytes(facet))
        {
            matches.push_back(facet);
        }
    }
    return matches;
}

std::optional<FacetSize> facetSize(std::string_view name)
{
    const std::string normalized = normalize(name);
    if (isMondainsLegacyAlias(normalized))
    {
        return facetSizes.at(1);
    }
    if (isLegacyTrammelAlias(normalized))
    {
        return facetSizes.front();
    }

    for (const auto & facet : facetSizes)
    {
        if (normalized == normalize(facet.name))
        {
            return facet;
        }
    }
    return std::nullopt;
}

} // namespace uo_mule::core
