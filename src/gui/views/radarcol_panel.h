#pragma once

#include "gui/assets/asset_window_contracts.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/documents/map_workspace.h"
#include "gui/documents/radarcol_document.h"
#include "gui/persistence/preferences.h"
#include "gui/profile/data_profile.h"
#include "uomul/radarcol/radarcol.h"
#include <array>
#include <cstddef>
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
    std::string error;
    assets::AssetWindowState assetView;
    std::unordered_map<DocumentId, assets::AssetWindowState> documentAssetViews;
};

void syncRadarcolPanel(RadarcolPanelState & state, const DataProfile & profile);
void applyFileBrowserResult(RadarcolPanelState & state, const FileBrowserResult & result);
void drawRadarcolPanel(RadarcolPanelState & state, bool & visible, IdFormat idFormat);
void drawRadarcolDocumentPanel(RadarcolPanelState & state,
                               RadarcolDocument & document,
                               bool & visible,
                               FileBrowserState & browser,
                               IdFormat idFormat,
                               const std::function<void()> & becameVisible = {});

} // namespace uo_mule::gui
