#include "gui/providers/embedded_radarcol.h"
#include "gui/context/color_conversion.h"
#include "gui/documents/map_workspace.h"
#include "gui/providers/data_providers.h"
#include "uomul/radarcol/radarcol.h"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>

namespace uo_mule::gui
{
namespace
{

// Generated from radarcol.mul, SHA-256:
// 098f339b42f9cbaf298bea9cad8efb8bf51b71981e9e72cdb78e2c77868befde
// Embedded payload: first 0x8000 little-endian colors, SHA-256:
// 68ef6f81484f8d220b030228de073611c1042de7c9b1d6bab54feb94bcd0829e
constexpr std::array<uomul::RadarColor, uomul::radarColorCount> colors{
#include "gui/providers/embedded_radarcol_values.inc"
};

class EmbeddedRadarColorProvider final : public RadarColorProvider
{
public:
    [[nodiscard]] ProviderStatus status() const override
    {
        return {
            .state = ProviderAssetState::available,
            .source = ProviderSourceKind::embeddedDefault,
            .message = "Embedded Radarcol",
            .revision = 1,
        };
    }

    [[nodiscard]] ProviderStatus status(std::uint16_t id) const override
    {
        if (id >= uomul::radarMapColorCount)
        {
            return {
                .state = ProviderAssetState::invalid,
                .source = ProviderSourceKind::embeddedDefault,
                .message = "Map color ID is outside the radar table",
                .revision = 1,
            };
        }
        return status();
    }

    [[nodiscard]] std::optional<MapColor> mapColor(std::uint16_t id) const override
    {
        if (id >= uomul::radarMapColorCount)
        {
            return std::nullopt;
        }
        return radarColorToMapColor(colors.at(id));
    }
};

} // namespace

std::optional<uomul::RadarColor> embeddedRadarcolColor(std::uint32_t index) noexcept
{
    if (index >= colors.size())
    {
        return std::nullopt;
    }
    return colors.at(index);
}

std::unique_ptr<RadarColorProvider> makeEmbeddedRadarColorProvider()
{
    return std::make_unique<EmbeddedRadarColorProvider>();
}

} // namespace uo_mule::gui
