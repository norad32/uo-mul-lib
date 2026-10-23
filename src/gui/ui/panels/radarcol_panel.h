#pragma once

#include "gui/documents/radarcol_document.h"
#include "gui/persistence/preferences.h"
#include "gui/providers/data_providers.h"
#include "gui/ui/dialogs/file_browser_dialog.h"
#include "gui/ui/widgets/asset_browser_contracts.h"
#include "gui/workspaces/map_workspace.h"
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
