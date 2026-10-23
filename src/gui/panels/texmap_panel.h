#pragma once

#include "gui_model/model/assets/asset_window_contracts.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/providers/data_providers.h"
#include "gui_model/world/map_color.h"
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace uo_mule::gui
{

struct PendingPreferenceChanges;

struct TexmapsTexturePreview
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<MapColor> pixels;
};

struct TexmapsPanelState
{
    std::unordered_map<std::uint32_t, TexmapsTexturePreview> texturePreviews;
    std::uint64_t texturePreviewRevision = 0;
    bool hideUnused = false;
    assets::AssetWindowState assetView;
};

void drawTexmapsPanel(TexmapsPanelState & state,
                      const TextureProvider & provider,
                      bool & visible,
                      const Preferences & preferences,
                      PendingPreferenceChanges & pendingChanges);

} // namespace uo_mule::gui
