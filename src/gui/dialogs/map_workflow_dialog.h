#pragma once

#include "gui/dialogs/file_browser_dialog.h"
#include "gui/workbench/notification_state.h"
#include "gui_model/documents/map_document_manager.h"
#include "gui_model/operations/operation_controller.h"
#include "gui_model/profile/data_profile.h"
#include "uomul/facet_sizes.h"
#include "uomul/map/map.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace uo_mule::gui
{

inline constexpr std::size_t mapDialogPathBufferSize = 1024;
inline const auto defaultMapDialogSize = uomul::knownFacetSizes().front();

enum class MapFileDialogMode : std::uint8_t
{
    none,
    openMap,
    selectMapFacet,
    newType,
    newMap,
    saveAs,
    dirtyChanges,
    overwrite,
    error,
};

enum class NewFileKind : std::uint8_t
{
    map,
    radarcol,
    texmapPair,
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

struct NewFileDialogState
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
    NewFileKind newFileKind = NewFileKind::map;
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

void drawNewFileDialog(MapDocumentManager & mapDocument,
                       NewFileDialogState & state,
                       OperationController & controller,
                       OutputCollisionPolicy collisionPolicy,
                       NotificationState & notifications,
                       FileBrowserState & browser,
                       bool & shouldClose);

void cancelFileDialog(NewFileDialogState & state);
void applyFileBrowserResult(NewFileDialogState & state, const FileBrowserResult & result);
void returnToFileBrowser(NewFileDialogState & state, FileBrowserState & browser);

void requestCloseOperation(MapDocumentManager & mapDocument,
                           NewFileDialogState & state,
                           OperationController & controller,
                           PendingMapOperationKind kind,
                           NotificationState & notifications);

} // namespace uo_mule::gui
