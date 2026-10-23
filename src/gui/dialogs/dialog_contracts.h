#pragma once

#include "gui/dialogs/file_browser_dialog.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <variant>

namespace uo_mule::gui
{

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
};

[[nodiscard]] const char * dialogPopupId(DialogId id) noexcept;
[[nodiscard]] DialogRequest makeDialogRequest(DialogId id, DialogRequestPayload payload = {});

} // namespace uo_mule::gui
