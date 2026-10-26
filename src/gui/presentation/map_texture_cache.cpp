#include "gui/presentation/map_texture_cache.h"
#include "uomul/map/map.h"
#include <GL/gl.h>
#include <algorithm>
#include <cstdint>
#include <imgui.h>

namespace uo_mule::gui
{

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
        entry.rgbaPixels.push_back(255U);
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
        return IM_COL32(255, 255, 255, 255);
    }
    const double brightness = 0.5 + (static_cast<double>(zGrayscale(tile.z)) / 255.0);
    const auto channel = static_cast<std::uint8_t>(std::clamp(static_cast<int>(brightness * 255.0), 0, 255));
    return IM_COL32(channel, channel, channel, 255);
}

} // namespace uo_mule::gui
