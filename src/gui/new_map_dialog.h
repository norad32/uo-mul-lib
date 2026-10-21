#pragma once

#include "gui/map_workspace.h"
#include "gui/data_profile.h"
#include "gui/notification_state.h"
#include "gui/operation_controller.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "core/facet_sizes.h"
#include "uomul/map.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace uo_mule::gui
{

inline constexpr std::size_t mapDialogPathBufferSize = 1024;
inline const auto defaultMapDialogSize = uo_mule::core::knownFacetSizes().front();

enum class MapFileDialogMode : std::uint8_t
{
    none,
    openMap,
    selectMapFacet,
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
    OperationController * controller = nullptr;
    NotificationState * notifications = nullptr;
    OutputCollisionPolicy collisionPolicy = OutputCollisionPolicy::confirm;
    std::array<char, mapDialogPathBufferSize> openPath{};
    std::array<char, mapDialogPathBufferSize> newMapPath{};
    std::array<char, mapDialogPathBufferSize> savePath{};
    std::uint32_t openWidth = defaultMapDialogSize.widthTiles;
    std::uint32_t openHeight = defaultMapDialogSize.heightTiles;
    std::uint32_t newMapWidth = defaultMapDialogSize.widthTiles;
    std::uint32_t newMapHeight = defaultMapDialogSize.heightTiles;
    int openFacet = 1;
    bool openWithoutReview = false;
    std::vector<FileBrowserMapCandidate> openCandidates;
    FileBrowserState * browser = nullptr;
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
                      NewMapDialogState & state,
                      OperationController & controller,
                      OutputCollisionPolicy collisionPolicy,
                      NotificationState & notifications,
                      FileBrowserState & browser,
                      bool & shouldClose);

void cancelMapDialog(NewMapDialogState & state);
void applyFileBrowserResult(NewMapDialogState & state, const FileBrowserResult & result);

void requestCloseOperation(MapWorkspace & workspace,
                           NewMapDialogState & state,
                           OperationController & controller,
                           PendingMapOperationKind kind,
                           NotificationState & notifications);

} // namespace uo_mule::gui
