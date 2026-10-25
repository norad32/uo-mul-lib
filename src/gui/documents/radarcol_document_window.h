#pragma once

#include "gui_model/documents/map_document_state.h"
#include "gui_model/documents/radarcol_document.h"
#include "gui_model/model/assets/asset_window_contracts.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/providers/data_providers.h"
#include "uomul/radarcol/radarcol.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace uo_mule::gui
{

inline constexpr std::size_t radarcolPathBufferSize = 1024;
inline constexpr std::size_t radarcolPickerRgbBufferSize = 4;
inline constexpr std::size_t radarcolPickerHexBufferSize = 8;
inline constexpr std::size_t radarcolPickerRawBufferSize = 5;

enum class RadarcolBrowserKind : std::uint8_t
{
    map,
    statics,
};

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
    DocumentId documentId = 0;
    std::vector<std::uint32_t> targetIndices;
    uomul::RadarColor value = 0;
    bool mixed = false;
    RadarcolColorPickerFormat format = RadarcolColorPickerFormat::rgb;
    std::array<char, radarcolPickerRgbBufferSize> red{};
    std::array<char, radarcolPickerRgbBufferSize> green{};
    std::array<char, radarcolPickerRgbBufferSize> blue{};
    std::array<char, radarcolPickerHexBufferSize> rgbHex{};
    std::array<char, radarcolPickerRawBufferSize> raw{};
    std::string error;
};

struct RadarcolDocumentWindowState
{
    assets::AssetWindowState assetView;
    std::unordered_map<DocumentId, assets::AssetWindowState> documentAssetViews;
    std::unordered_map<DocumentId, RadarcolBrowserKind> documentBrowserKinds;
    RadarcolBrowserKind browserKind = RadarcolBrowserKind::map;
    std::function<void(const assets::AssetItemId &)> itemPicked;
    std::function<void(std::string_view)> hoveredContext;
    std::function<void(std::uint32_t, bool)> showInTiledata;
    std::function<void(std::uint32_t)> showInTexmaps;
    std::function<void()> clearStatus;
    std::function<std::optional<uomul::RadarColor>()> activeTileColor;
    std::function<void(const std::string &)> copyToClipboard;
    std::function<void(std::uint32_t)> useAsActiveTile;
    RadarcolColorPickerState colorPicker;
};

void openRadarcolColorPicker(RadarcolDocumentWindowState & state,
                             const RadarcolDocument & document,
                             const assets::AssetSelection & selection);
[[nodiscard]] bool acceptRadarcolColorSource(RadarcolDocumentWindowState & state, uomul::RadarColor color);
void cancelRadarcolColorSource(RadarcolDocumentWindowState & state);
void drawRadarcolPanel(RadarcolDocumentWindowState & state,
                       const RadarColorProvider & provider,
                       bool & visible,
                       IdFormat idFormat);
void drawRadarcolDocumentWindow(RadarcolDocumentWindowState & state,
                                RadarcolDocument & document,
                                bool & visible,
                                IdFormat idFormat,
                                std::string windowIdentity,
                                const std::function<void()> & becameVisible = {});

} // namespace uo_mule::gui
