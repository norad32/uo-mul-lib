#include "gui/presentation/map_texture_cache.h"
#include "gui_model/documents/map_document_state.h"
#include "gui_model/documents/map_view_state.h"
#include "gui_model/providers/data_providers.h"
#include "gui_model/world/map_canvas_state.h"
#include "gui_model/world/map_color.h"
#include "uomul/map/map.h"
#include <GL/gl.h>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <imgui.h>
#include <unordered_map>

namespace uo_mule::gui
{

namespace
{

constexpr int colorChannelMaximum = 255;

} // namespace

MapCanvasCachedTexture makeMapTextureCacheEntry(const TextureProvider & textures, std::uint32_t id)
{
    MapCanvasCachedTexture entry;
    const auto texture = textures.texture(id);
    if (!texture || texture->width == 0U || texture->height == 0U || texture->pixels.empty())
    {
        return entry;
    }

    entry.width = texture->width;
    entry.height = texture->height;
    entry.pixels = texture->pixels;
    entry.rgbaPixels.reserve(entry.pixels.size() * 4U);
    for (const MapColor pixel : entry.pixels)
    {
        entry.rgbaPixels.push_back(pixel.red);
        entry.rgbaPixels.push_back(pixel.green);
        entry.rgbaPixels.push_back(pixel.blue);
        entry.rgbaPixels.push_back(static_cast<std::uint8_t>(colorChannelMaximum));
    }
    return entry;
}

MapCanvasCachedTexture & cachedMapTexture(std::unordered_map<std::uint32_t, MapCanvasCachedTexture> & cache,
                                          const TextureProvider & textures,
                                          std::uint32_t id)
{
    const auto existing = cache.find(id);
    if (existing != cache.end())
    {
        return existing->second;
    }
    return cache.emplace(id, makeMapTextureCacheEntry(textures, id)).first->second;
}

void uploadMapTexture(MapCanvasCachedTexture & texture)
{
    if (texture.pixels.empty())
    {
        return;
    }
    if (texture.gpuTexture == 0U)
    {
        GLuint handle = 0;
        glGenTextures(1, &handle);
        texture.gpuTexture = handle;
    }
    if (texture.gpuTexture == 0U)
    {
        return;
    }

    GLint previousTexture = 0;
    GLint previousUnpackAlignment = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
    glBindTexture(GL_TEXTURE_2D, texture.gpuTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 static_cast<GLsizei>(texture.width),
                 static_cast<GLsizei>(texture.height),
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 texture.rgbaPixels.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
}

void clearMapTextureCache(std::unordered_map<std::uint32_t, MapCanvasCachedTexture> & cache,
                          const std::function<void(std::uint32_t)> & releaseTexture)
{
    if (releaseTexture)
    {
        for (auto & [id, texture] : cache)
        {
            static_cast<void>(id);
            if (texture.gpuTexture != 0U)
            {
                releaseTexture(texture.gpuTexture);
            }
        }
    }
    cache.clear();
}

ImU32 mapTextureTint(MapDisplayMode mode, uomul::MapTile tile) noexcept
{
    if (mode != MapDisplayMode::combined)
    {
        return IM_COL32(colorChannelMaximum, colorChannelMaximum, colorChannelMaximum, colorChannelMaximum);
    }
    const double brightness = 0.5 + (static_cast<double>(zGrayscale(tile.z)) / static_cast<double>(colorChannelMaximum));
    const auto channel = static_cast<std::uint8_t>(
        std::clamp(static_cast<int>(brightness * static_cast<double>(colorChannelMaximum)), 0, colorChannelMaximum));
    return IM_COL32(channel, channel, channel, colorChannelMaximum);
}

} // namespace uo_mule::gui
