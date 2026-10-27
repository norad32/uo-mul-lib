#include "gui/dialogs/file_browser_dialog.h"
#include "gui_model/dialogs/dialog_contracts.h"
#include "imgui_test_support.h"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <string>
#include <system_error>

using uo_mule::gui::drawFileBrowser;
using uo_mule::gui::FileBrowserMode;
using uo_mule::gui::FileBrowserRequest;
using uo_mule::gui::FileBrowserState;
using uo_mule::gui::FileBrowserTarget;
using uo_mule::gui::requestFileBrowser;
using uo_mule::gui::test::ImGuiFrame;

namespace
{

class TemporaryBrowserDirectory
{
public:
    TemporaryBrowserDirectory() : path_(std::filesystem::temp_directory_path() / "uo-mule-file-browser-dialog-tests")
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        std::filesystem::create_directories(path_, error);
        std::ofstream(path_ / "visible.mul") << "data";
        std::ofstream(path_ / ".hidden.mul") << "hidden";
    }

    ~TemporaryBrowserDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryBrowserDirectory(const TemporaryBrowserDirectory &) = delete;
    TemporaryBrowserDirectory & operator=(const TemporaryBrowserDirectory &) = delete;
    TemporaryBrowserDirectory(TemporaryBrowserDirectory &&) = delete;
    TemporaryBrowserDirectory & operator=(TemporaryBrowserDirectory &&) = delete;

    [[nodiscard]] const std::filesystem::path & path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("fileBrowserDialog_Should_RenderOpenFileListing")
{
    const TemporaryBrowserDirectory directory;
    FileBrowserState state;
    requestFileBrowser(state,
                       FileBrowserRequest{
                           .mode = FileBrowserMode::openFile,
                           .target = FileBrowserTarget::openGeneric,
                           .initialPath = directory.path(),
                           .extension = ".mul",
                           .title = "Open",
                       });

    const ImGuiFrame frame;
    REQUIRE_FALSE(drawFileBrowser(state, 7U).has_value());
    REQUIRE(state.visible);
    REQUIRE(state.selectedPath.empty());
}

TEST_CASE("fileBrowserDialog_Should_RenderSaveFileFooter")
{
    const TemporaryBrowserDirectory directory;
    FileBrowserState state;
    requestFileBrowser(state,
                       FileBrowserRequest{
                           .mode = FileBrowserMode::saveFile,
                           .target = FileBrowserTarget::createMap,
                           .initialPath = directory.path() / "created.mul",
                           .extension = ".mul",
                           .title = "Save",
                       });

    const ImGuiFrame frame;
    REQUIRE_FALSE(drawFileBrowser(state, 8U).has_value());
    REQUIRE(std::string(state.filename.data()) == "created.mul");
}

TEST_CASE("fileBrowserDialog_Should_RenderDirectoryFooterAndCreateFolderPopup")
{
    const TemporaryBrowserDirectory directory;
    FileBrowserState state;
    requestFileBrowser(state,
                       FileBrowserRequest{
                           .mode = FileBrowserMode::chooseDirectory,
                           .target = FileBrowserTarget::chooseDataSearchRoot,
                           .initialPath = directory.path(),
                           .extension = {},
                           .title = "Choose",
                       });
    state.createFolderVisible = true;
    constexpr std::size_t newFolderNameLength = 11U;
    std::copy_n("new-folder", newFolderNameLength, state.newFolderName.data());

    const ImGuiFrame frame;
    ImGui::OpenPopup("Create folder###dialog.file-browser.create-folder");
    REQUIRE_FALSE(drawFileBrowser(state, 9U).has_value());
    REQUIRE(state.createFolderVisible);
}

TEST_CASE("fileBrowserDialog_Should_ReturnNothingWhenHidden")
{
    FileBrowserState state;
    const ImGuiFrame frame;

    REQUIRE_FALSE(drawFileBrowser(state, 10U).has_value());
}
