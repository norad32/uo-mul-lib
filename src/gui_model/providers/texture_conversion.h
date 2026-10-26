#pragma once

#include "gui_model/providers/data_providers.h"
#include "uomul/texmaps/texmaps.h"
#include <expected>

namespace uo_mule::gui
{

[[nodiscard]] std::expected<uomul::TextureImage, uomul::Error> providerTextureToTextureImage(const ProviderTexture & provider);

} // namespace uo_mule::gui
