#pragma once

#include "gui_model/documents/tiledata_document.h"
#include "gui_model/model/assets/asset_window_contracts.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/providers/data_providers.h"
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace uo_mule::gui
{

enum class TiledataBrowserKind : std::uint8_t
{
    map,
    statics,
};

struct TiledataDocumentWindowState
{
    assets::AssetWindowState assetView;
    std::unordered_map<DocumentId, assets::AssetWindowState> documentAssetViews;
    TiledataBrowserKind browserKind = TiledataBrowserKind::map;
    std::unordered_map<std::uint32_t, ProviderTexture> texturePreviews;
    std::uint64_t texturePreviewRevision = 0;
    std::function<void(const assets::AssetItemId &)> itemPicked;
    std::function<bool(std::uint32_t, bool)> itemSourcePicked;
    std::function<void(std::string_view)> hoveredContext;
};

void drawTiledataDocumentWindow(TiledataDocumentWindowState & state,
                                TiledataDocument & document,
                                bool & visible,
                                IdFormat idFormat,
                                const TextureProvider & textures,
                                const RadarColorProvider & radarColors,
                                std::string windowIdentity,
                                const std::function<void()> & becameVisible = {});

} // namespace uo_mule::gui
