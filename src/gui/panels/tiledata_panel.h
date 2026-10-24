#pragma once

#include "gui_model/documents/tiledata_document.h"
#include "gui_model/model/assets/asset_window_contracts.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/providers/data_providers.h"
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

namespace uo_mule::gui
{

enum class TiledataBrowserKind : std::uint8_t
{
    map,
    statics,
};

struct TiledataPanelState
{
    assets::AssetWindowState assetView;
    TiledataBrowserKind browserKind = TiledataBrowserKind::map;
    std::unordered_map<std::uint32_t, ProviderTexture> texturePreviews;
    std::uint64_t texturePreviewRevision = 0;
    std::function<void(const assets::AssetItemId &)> itemPicked;
};

void drawTiledataPanel(TiledataPanelState & state,
                       const TiledataProvider & provider,
                       const TextureProvider & textures,
                       const RadarColorProvider & radarColors,
                       bool & visible,
                       IdFormat idFormat);

void drawTiledataDocumentPanel(TiledataPanelState & state,
                               TiledataDocument & document,
                               const TextureProvider & textures,
                               const RadarColorProvider & radarColors,
                               bool & visible,
                               IdFormat idFormat,
                               std::string windowIdentity);

} // namespace uo_mule::gui
