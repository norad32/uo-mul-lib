#pragma once

#include "gui_model/profile/data_profile.h"
#include "uomul/map/map.h"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace uo_mule::gui
{

inline constexpr std::size_t fileBrowserBufferSize = 1024;

enum class FileBrowserMode : std::uint8_t
{
    openFile,
    saveFile,
    chooseDirectory,
};

enum class FileBrowserTarget : std::uint8_t
{
    genericOpen,
    mapOpen,
    newMapOutput,
    mapSaveAs,
    dataSourceRoot,
    dataSearchRoot,
    dataOverride,
    radarcolSource,
    radarcolOutput,
    texmapsIndex,
    texmapsData,
    texmapsCompanion,
    newRadarcolOutput,
    newTexmapsData,
    imageOpen,
};

enum class MulAssetKind : std::uint8_t
{
    unknown,
    map,
    radarcol,
    texmapsPair,
    tiledata,
};

struct FileBrowserRequest
{
    FileBrowserMode mode = FileBrowserMode::openFile;
    FileBrowserTarget target = FileBrowserTarget::mapOpen;
    DataFileRole role = DataFileRole::radarcol;
    std::filesystem::path initialPath;
    std::string extension;
    std::string title;
};

struct FileBrowserMapCandidate
{
    std::string presetName;
    std::string facet;
    uomul::Dimensions dimensions{};
    std::uint64_t expectedBlocks = 0;
    std::uint64_t completeBlocks = 0;
    std::uint64_t missingBlocks = 0;
    std::uint64_t trailingBytes = 0;
    bool valid = false;
};

struct FileBrowserAssetCandidate
{
    // Candidate presentation follows one scheme: label, compact detail, warning, likelihood score.
    MulAssetKind kind = MulAssetKind::unknown;
    std::filesystem::path path;
    std::filesystem::path companionPath;
    std::string label;
    std::string facet;
    std::string reason;
    std::string warning;
    int score = 0;
    std::uint32_t itemCount = 0;
    std::optional<uomul::Dimensions> dimensions;
    bool validated = false;
    bool openable = false;
};

struct FileBrowserResult
{
    FileBrowserTarget target = FileBrowserTarget::mapOpen;
    DataFileRole role = DataFileRole::radarcol;
    std::filesystem::path path;
    std::vector<FileBrowserMapCandidate> mapCandidates;
    std::vector<FileBrowserAssetCandidate> assetCandidates;
    std::optional<FileBrowserAssetCandidate> assetCandidate;
};

enum class DialogId : std::uint8_t
{
    none,
    mapWorkflow,
    preferences,
    fileBrowser,
    fileSelector,
};

enum class DialogAction : std::uint8_t
{
    none,
    accept,
    apply,
    cancel,
    back,
    discard,
    overwrite,
    choosePath,
    chooseCompanion,
};

enum class MapDialogWorkflow : std::uint8_t
{
    open,
    create,
    saveAs,
    dirtyChanges,
    overwrite,
    error,
};

struct MapDialogRequest
{
    MapDialogWorkflow workflow = MapDialogWorkflow::open;
};

struct PreferencesDialogRequest
{
    bool focusProfile = false;
};

struct FileBrowserDialogRequest
{
    FileBrowserRequest request;
};

struct FileSelectorDialogRequest
{
    std::filesystem::path sourcePath;
};

using DialogRequestPayload = std::
    variant<std::monostate, MapDialogRequest, PreferencesDialogRequest, FileBrowserDialogRequest, FileSelectorDialogRequest>;

struct DialogRequest
{
    DialogId id = DialogId::none;
    std::string popupId;
    DialogRequestPayload payload;
    bool replaceActive = false;
    std::uint64_t generation = 0;
};

struct DialogPathResult
{
    std::filesystem::path path;
};

struct DialogErrorResult
{
    std::string message;
};

using DialogResultPayload = std::variant<std::monostate, FileBrowserResult, DialogPathResult, DialogErrorResult>;

struct DialogResult
{
    DialogId id = DialogId::none;
    DialogAction action = DialogAction::none;
    DialogResultPayload payload;
    std::uint64_t generation = 0;
};

[[nodiscard]] const char * dialogPopupId(DialogId id) noexcept;
[[nodiscard]] DialogRequest makeDialogRequest(DialogId id, DialogRequestPayload payload = {});

} // namespace uo_mule::gui
