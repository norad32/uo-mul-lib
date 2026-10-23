#pragma once

#include "gui/assets/asset_window_contracts.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/documents/map_workspace.h"
#include "gui/documents/radarcol_document.h"
#include "gui/persistence/preferences.h"
#include "gui/providers/data_providers.h"
#include "uomul/radarcol/radarcol.h"
#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>

namespace uo_mule::gui
{

inline constexpr std::size_t radarcolPathBufferSize = 1024;
struct RadarcolPanelState
{
    assets::AssetWindowState assetView;
    std::unordered_map<DocumentId, assets::AssetWindowState> documentAssetViews;
};

void drawRadarcolPanel(RadarcolPanelState & state, const RadarColorProvider & provider, bool & visible, IdFormat idFormat);
void drawRadarcolDocumentPanel(RadarcolPanelState & state,
                               RadarcolDocument & document,
                               bool & visible,
                               FileBrowserState & browser,
                               IdFormat idFormat,
                               std::string windowIdentity,
                               const std::function<void()> & becameVisible = {});

} // namespace uo_mule::gui
