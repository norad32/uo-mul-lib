#pragma once

#include "gui/dialogs/file_browser_dialog.h"
#include "gui_model/documents/radarcol_document.h"
#include "gui_model/model/assets/asset_window_contracts.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/providers/data_providers.h"
#include "gui_model/workspaces/map_workspace.h"
#include "uomul/radarcol/radarcol.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace uo_mule::gui
{

inline constexpr std::size_t radarcolPathBufferSize = 1024;

enum class RadarcolColorPickerFormat : std::uint8_t
{
    rgb,
    rgbHex,
    raw,
};

struct RadarcolColorPickerState
{
    bool open = false;
    bool sourceMode = false;
    bool positionPending = true;
    DocumentId documentId = 0;
    std::vector<std::uint32_t> targetIndices;
    uomul::RadarColor value = 0;
    bool mixed = false;
    RadarcolColorPickerFormat format = RadarcolColorPickerFormat::rgb;
    std::array<char, 4> red{};
    std::array<char, 4> green{};
    std::array<char, 4> blue{};
    std::array<char, 8> rgbHex{};
    std::array<char, 5> raw{};
    std::string error;
};

struct RadarcolPanelState
{
    assets::AssetWindowState assetView;
    std::unordered_map<DocumentId, assets::AssetWindowState> documentAssetViews;
    std::function<void(const assets::AssetItemId &)> itemPicked;
    std::function<void(const std::string &)> statusMessage;
    std::function<std::optional<uomul::RadarColor>()> activeTileColor;
    std::function<void(const std::string &)> copyToClipboard;
    std::function<void(std::uint32_t)> useAsActiveTile;
    RadarcolColorPickerState colorPicker;
};

void openRadarcolColorPicker(RadarcolPanelState & state, RadarcolDocument & document, const assets::AssetSelection & selection);
[[nodiscard]] bool acceptRadarcolColorSource(RadarcolPanelState & state, uomul::RadarColor color);
void cancelRadarcolColorSource(RadarcolPanelState & state);
void drawRadarcolPanel(RadarcolPanelState & state, const RadarColorProvider & provider, bool & visible, IdFormat idFormat);
void drawRadarcolDocumentPanel(RadarcolPanelState & state,
                               RadarcolDocument & document,
                               bool & visible,
                               FileBrowserState & browser,
                               IdFormat idFormat,
                               std::string windowIdentity,
                               const std::function<void()> & becameVisible = {});

} // namespace uo_mule::gui
