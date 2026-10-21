#pragma once

#include "gui/documents/map_workspace.h"
#include "gui/documents/radarcol_document.h"
#include "gui/profile/data_profile.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/assets/asset_window_contracts.h"
#include "uomul/radarcol/radarcol.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

namespace uo_mule::gui
{

inline constexpr std::size_t radarcolPathBufferSize = 1024;
struct RadarcolPanelState
{
    std::array<char, radarcolPathBufferSize> sourcePath{};
    std::optional<uomul::RadarColReader> reader;
    std::array<MapColor, uomul::radarMapColorCount> mapColorCache{};
    std::array<std::uint8_t, uomul::radarMapColorCount> mapColorCacheStatus{};
    std::uint64_t colorRevision = 0;
    std::string error;
    assets::AssetWindowState assetView;
    std::unordered_map<DocumentId, assets::AssetWindowState> documentAssetViews;
};

[[nodiscard]] std::optional<MapColor> radarMapDisplayColor(RadarcolPanelState & state, std::uint16_t id);
void syncRadarcolPanel(RadarcolPanelState & state, const DataProfile & profile);
void applyFileBrowserResult(RadarcolPanelState & state, const FileBrowserResult & result);
void drawRadarcolPanel(RadarcolPanelState & state,
                       bool & visible,
                       FileBrowserState & browser,
                       bool readOnly = false);
void drawRadarcolDocumentPanel(RadarcolPanelState & state,
                               RadarcolDocument & document,
                               bool & visible,
                               FileBrowserState & browser,
                               const std::function<void()> & becameVisible = {});

} // namespace uo_mule::gui
