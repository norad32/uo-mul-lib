#include "gui/dialogs/file_browser_dialog.h"
#include "gui/common/file_size.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

namespace uo_mule::gui
{
namespace
{

TEST_CASE("file browser matches extensions case insensitively")
{
    REQUIRE(fileBrowserMatchesFilter("terrain.MUL", ".mul"));
    REQUIRE(fileBrowserMatchesFilter("terrain.mul", ".mul"));
    REQUIRE_FALSE(fileBrowserMatchesFilter("terrain.map", ".mul"));
    REQUIRE(fileBrowserMatchesFilter("terrain.mul", {}));
}

TEST_CASE("file browser formats file sizes")
{
    REQUIRE(formatFileSize(9999U) == "9999 B");
    REQUIRE(formatFileSize(10000U) == "10.0 kB");
    REQUIRE(formatFileSize(1234567U) == "1.23 MB");
}

TEST_CASE("file browser inspects generic file metadata")
{
    const auto path = std::filesystem::temp_directory_path() / "uo-mule-file-browser-summary.tmp";
    {
        std::ofstream output(path, std::ios::binary);
        output << "test";
    }

    const auto summary = inspectFileBrowserSelection(path);
    REQUIRE(summary.exists);
    REQUIRE(summary.regularFile);
    REQUIRE(summary.readable);
    REQUIRE(summary.size);
    REQUIRE(*summary.size == 4U);

    std::error_code error;
    std::filesystem::remove(path, error);
}

TEST_CASE("file browser save request preserves the initial filename")
{
    const auto path = std::filesystem::temp_directory_path() / "uo-mule-output.mul";
    FileBrowserState state;
    requestFileBrowser(state,
                       {.mode = FileBrowserMode::saveFile,
                        .target = FileBrowserTarget::newMapOutput,
                        .initialPath = path,
                        .extension = ".mul",
                        .title = "Save",});

    REQUIRE(state.visible);
    REQUIRE(state.directory == path.parent_path());
    REQUIRE(std::string(state.filename.data()) == path.filename().string());
    cancelFileBrowser(state);
    REQUIRE_FALSE(state.visible);
}

} // namespace
} // namespace uo_mule::gui
