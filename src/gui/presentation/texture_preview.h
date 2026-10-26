#pragma once

#include "gui_model/world/map_color.h"
#include "uomul/map/map.h"
#include <cstdint>
#include <span>

namespace uo_mule::gui
{

struct TexturePreviewData
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::span<const MapColor> pixels;
};

void drawTexturePreview(TexturePreviewData texture, float size, float borderThickness = 1.0F);

} // namespace uo_mule::gui
