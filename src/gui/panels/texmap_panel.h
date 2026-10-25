#pragma once

#include "gui_model/documents/texmaps_document.h"
#include "gui_model/model/assets/asset_window_contracts.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/providers/data_providers.h"
#include "gui_model/world/map_color.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>
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
    std::unordered_map<DocumentId, std::unordered_map<std::uint32_t, TexmapsTexturePreview>> documentTexturePreviews;
    std::unordered_map<DocumentId, std::uint64_t> documentTexturePreviewRevisions;
    std::uint64_t texturePreviewRevision = 0;
    bool hideUnused = false;
    assets::AssetWindowState assetView;
    std::unordered_map<DocumentId, assets::AssetWindowState> documentAssetViews;
    std::function<void(const assets::AssetItemId &)> itemPicked;
    std::function<bool(std::uint32_t)> itemSourcePicked;
    std::function<void(std::string_view)> hoveredContext;
};

void drawTexmapsPanel(TexmapsPanelState & state,
                      const TextureProvider & provider,
                      bool & visible,
                      const Preferences & preferences,
                      PendingPreferenceChanges & pendingChanges);

void drawTexmapsDocumentPanel(TexmapsPanelState & state,
                              TexmapsDocument & document,
                              bool & visible,
                              IdFormat idFormat,
                              std::string windowIdentity,
                              const std::function<void()> & becameVisible = {});

} // namespace uo_mule::gui
