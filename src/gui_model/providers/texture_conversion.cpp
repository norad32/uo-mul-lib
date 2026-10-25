#include "gui_model/providers/texture_conversion.h"
#include "uomul/radarcol/radarcol.h"
#include <cstddef>

namespace uo_mule::gui
{

std::expected<uomul::TextureImage, uomul::Error>
providerTextureToTextureImage(const ProviderTexture & provider)
{
    if (provider.width != provider.height ||
        (provider.width != uomul::textureSmallDimension && provider.width != uomul::textureLargeDimension))
    {
        return std::unexpected(uomul::Error{
            .code = uomul::ErrorCode::invalidDimensions,
            .path = {},
            .offset = 0,
            .message = "texture provider returned unsupported dimensions",
        });
    }
    const auto expectedPixels = static_cast<std::size_t>(provider.width) * provider.height;
    if (provider.pixels.size() != expectedPixels)
    {
        return std::unexpected(uomul::Error{
            .code = uomul::ErrorCode::invalidFormat,
            .path = {},
            .offset = 0,
            .message = "texture provider returned an invalid pixel count",
        });
    }
    uomul::TextureImage texture{
        .dimension = static_cast<uomul::TextureDimension>(provider.width),
        .pixels = {},
    };
    texture.pixels.reserve(provider.pixels.size());
    for (const auto & pixel : provider.pixels)
    {
        texture.pixels.push_back(uomul::RadarColorConverter::fromRgb888(pixel.red, pixel.green, pixel.blue));
    }
    return texture;
}

} // namespace uo_mule::gui
