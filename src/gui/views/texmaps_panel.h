#pragma once

#include "gui/documents/map_workspace.h"
#include "gui/profile/data_profile.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/assets/asset_window_contracts.h"
#include "gui/persistence/preferences.h"
#include "uomul/texmaps/texmaps.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>

namespace uo_mule::gui
{

inline constexpr std::size_t texmapsPathBufferSize = 1024;
inline constexpr std::size_t texmapsPageTextureCount = 256;
inline constexpr std::uint32_t unloadedTexmapsPage = std::numeric_limits<std::uint32_t>::max();

struct TexmapsTexturePreview
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<MapColor> pixels;
};

struct TexmapsPanelState
{
    std::array<char, texmapsPathBufferSize> sourceIndexPath{};
    std::array<char, texmapsPathBufferSize> sourceDataPath{};
    std::optional<uomul::TexMapsReader> reader;
    std::unordered_map<std::uint32_t, TexmapsTexturePreview> texturePreviews;
    bool hideUnused = false;
    std::vector<TexmapsTexturePreview> pageTextures;
    std::uint32_t selectedTextureId = 0;
    std::string error;
    std::uint32_t page = 0;
    std::uint32_t pageCount = 0;
    std::uint32_t loadedPageIndex = unloadedTexmapsPage;
    bool previewVisible = false;
    assets::AssetWindowState assetView;
};

void syncTexmapsPanel(TexmapsPanelState & state, const DataProfile & profile);
void applyFileBrowserResult(TexmapsPanelState & state, const FileBrowserResult & result);
void drawTexmapsPanel(TexmapsPanelState & state, bool & visible, Preferences & preferences);

} // namespace uo_mule::gui
