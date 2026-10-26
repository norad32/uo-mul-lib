#pragma once

#include "gui_model/providers/data_providers.h"
#include "gui_model/world/map_canvas_state.h"
#include <functional>
#include <imgui.h>
#include <unordered_map>

namespace uo_mule::gui
{

[[nodiscard]] MapCanvasCachedTexture makeMapTextureCacheEntry(const TextureProvider & textures, std::uint32_t id);

[[nodiscard]] MapCanvasCachedTexture & cachedMapTexture(std::unordered_map<std::uint32_t, MapCanvasCachedTexture> & cache,
                                                        const TextureProvider & textures,
                                                        std::uint32_t id);

void uploadMapTexture(MapCanvasCachedTexture & texture);
void clearMapTextureCache(std::unordered_map<std::uint32_t, MapCanvasCachedTexture> & cache,
                          const std::function<void(std::uint32_t)> & releaseTexture);

[[nodiscard]] ImU32 mapTextureTint(MapDisplayMode mode, uomul::MapTile tile) noexcept;

} // namespace uo_mule::gui
