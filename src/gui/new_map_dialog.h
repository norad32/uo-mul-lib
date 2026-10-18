#pragma once

#include "gui/map_workspace.h"
#include "gui/data_profile.h"
#include "uomul/map.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace uo_mule::gui
{

inline constexpr std::size_t mapDialogPathBufferSize = 1024;
inline constexpr std::uint32_t defaultMapDialogWidth = 6144;
inline constexpr std::uint32_t defaultMapDialogHeight = 4096;

enum class MapFileDialogMode : std::uint8_t
{
    none,
    openMap,
    newMap,
    saveAs,
    dirtyChanges,
    overwrite,
    error,
};

enum class PendingMapOperationKind : std::uint8_t
{
    none,
    openMap,
    newMap,
    close,
    quit,
};

enum class MapOverwriteKind : std::uint8_t
{
    none,
    newMap,
    save,
};

struct PendingMapOperation
{
    PendingMapOperationKind kind = PendingMapOperationKind::none;
    std::filesystem::path path;
    uomul::Dimensions dimensions{};
    uomul::MapTile initialTile{};
};

struct NewMapDialogState
{
    std::array<char, mapDialogPathBufferSize> openPath{};
    std::array<char, mapDialogPathBufferSize> newMapPath{};
    std::array<char, mapDialogPathBufferSize> savePath{};
    std::uint32_t openWidth = defaultMapDialogWidth;
    std::uint32_t openHeight = defaultMapDialogHeight;
    std::uint32_t newMapWidth = defaultMapDialogWidth;
    std::uint32_t newMapHeight = defaultMapDialogHeight;
    int openFacet = 1;
    int openMapRole = 0;
    bool openProfileInitialized = false;
    int newMapFacet = 1;
    int initialTileId = 0;
    int initialTileZ = 0;
    MapFileDialogMode mode = MapFileDialogMode::none;
    MapFileDialogMode activePopup = MapFileDialogMode::none;
    PendingMapOperation operation{};
    MapOverwriteKind overwriteKind = MapOverwriteKind::none;
    bool saveForPendingOperation = false;
    bool operationOverwriteOutput = false;
    bool shouldClose = false;
    std::string error;
};

void drawNewMapDialog(MapWorkspace & workspace,
                      const DataProfile & profile,
                      NewMapDialogState & state,
                      bool & shouldClose);

} // namespace uo_mule::gui
