#include "core/facet_sizes.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/dialogs/file_selector_dialog.h"
#include "gui/dialogs/file_size.h"
#include "uomul/radarcol/radarcol.h"
#include <algorithm>
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

TEST_CASE("file browser identifies a valid radar color table")
{
    const auto path = std::filesystem::temp_directory_path() / "radarcol.mul";
    {
        std::ofstream output(path, std::ios::binary);
        for (std::uint32_t index = 0; index < uomul::radarColorCount; index++)
        {
            const std::uint16_t value = static_cast<std::uint16_t>(index & 0x7FFFU);
            output.put(static_cast<char>(value & 0xFFU));
            output.put(static_cast<char>(value >> 8U));
        }
    }

    const auto summary = inspectFileBrowserSelection(path);
    REQUIRE(std::any_of(summary.assetCandidates.begin(),
                        summary.assetCandidates.end(),
                        [](const auto & candidate) { return candidate.kind == MulAssetKind::radarcol && candidate.openable; }));

    std::error_code error;
    std::filesystem::remove(path, error);
}

TEST_CASE("file browser never treats an index file as an openable texture asset without its pair")
{
    const auto directory = std::filesystem::temp_directory_path();
    const auto indexPath = directory / "texidx.mul";
    {
        std::ofstream output(indexPath, std::ios::binary);
        output.write("\0\0\0\0\0\0\0\0\0\0\0\0", 12);
    }

    const auto summary = inspectFileBrowserSelection(indexPath);
    REQUIRE(std::any_of(summary.assetCandidates.begin(),
                        summary.assetCandidates.end(),
                        [](const auto & candidate)
                        { return candidate.kind == MulAssetKind::texmapsPair && !candidate.openable; }));

    std::error_code error;
    std::filesystem::remove(indexPath, error);
}

TEST_CASE("file selector rejects a direct texture index result")
{
    FileSelectorState state;
    FileBrowserResult result;
    result.target = FileBrowserTarget::texmapsIndex;
    result.path = "texidx.mul";
    result.assetCandidates.push_back(FileBrowserAssetCandidate{
        .kind = MulAssetKind::texmapsPair,
        .path = result.path,
        .companionPath = {},
        .label = "Texture pair",
        .facet = {},
        .reason = "A matching pair is required",
        .warning = {},
        .score = 90,
        .dimensions = std::nullopt,
        .validated = false,
        .openable = false,
    });
    openFileSelector(state, result);

    REQUIRE(state.visible);
    const auto selected = selectedFileSelectorResult(state);
    REQUIRE(selected.assetCandidate.has_value());
    REQUIRE_FALSE(selected.assetCandidate->openable);
}

TEST_CASE("file browser treats an empty texture index and data pair as one openable asset")
{
    const auto directory = std::filesystem::temp_directory_path();
    const auto indexPath = directory / "texidx.mul";
    const auto dataPath = directory / "texmaps.mul";
    {
        std::ofstream index(indexPath, std::ios::binary);
        std::ofstream data(dataPath, std::ios::binary);
    }

    const auto summary = inspectFileBrowserSelection(indexPath);
    const auto pair = std::find_if(summary.assetCandidates.begin(),
                                   summary.assetCandidates.end(),
                                   [](const auto & candidate) { return candidate.kind == MulAssetKind::texmapsPair; });
    REQUIRE(pair != summary.assetCandidates.end());
    REQUIRE(pair->openable);
    REQUIRE(pair->path == indexPath);
    REQUIRE(pair->companionPath == dataPath);

    std::error_code error;
    std::filesystem::remove(indexPath, error);
    std::filesystem::remove(dataPath, error);
}

TEST_CASE("generic open candidate selection preserves format and map dimensions")
{
    const auto directory = std::filesystem::temp_directory_path();
    const auto path = directory / "generic-map.mul";
    const auto dimensions = uo_mule::core::knownFacetSizes().front();
    std::error_code error;
    std::filesystem::remove(path, error);
    REQUIRE(uomul::MapEditor::createEmpty(
        path, uomul::Dimensions{.widthTiles = dimensions.widthTiles, .heightTiles = dimensions.heightTiles}));

    const auto summary = inspectFileBrowserSelection(path);
    FileBrowserResult result{
        .target = FileBrowserTarget::genericOpen,
        .path = path,
        .mapCandidates = summary.mapCandidates,
        .assetCandidates = summary.assetCandidates,
        .assetCandidate = std::nullopt,
    };
    FileSelectorState selector;
    openFileSelector(selector, result);
    const auto mapCandidate = std::find_if(selector.candidates.begin(),
                                           selector.candidates.end(),
                                           [](const auto & candidate) { return candidate.kind == MulAssetKind::map; });
    REQUIRE(mapCandidate != selector.candidates.end());
    selector.selectedCandidate = static_cast<std::size_t>(std::distance(selector.candidates.begin(), mapCandidate));
    const auto selected = selectedFileSelectorResult(selector);

    REQUIRE(selected.assetCandidate.has_value());
    REQUIRE(selected.assetCandidate->kind == MulAssetKind::map);
    REQUIRE(selected.assetCandidate->dimensions.has_value());
    REQUIRE(selected.assetCandidate->dimensions->widthTiles == dimensions.widthTiles);

    std::filesystem::remove(path, error);
}

TEST_CASE("generic open selector keeps multiple candidates available for explicit type choice")
{
    FileBrowserResult result{
        .target = FileBrowserTarget::genericOpen,
        .role = DataFileRole::radarcol,
        .path = "ambiguous.mul",
        .mapCandidates = {},
        .assetCandidates =
            {
                FileBrowserAssetCandidate{
                    .kind = MulAssetKind::map,
                    .path = "ambiguous.mul",
                    .companionPath = {},
                    .label = "Map",
                    .facet = {},
                    .reason = "Map candidate",
                    .warning = {},
                    .score = 80,
                    .dimensions = std::nullopt,
                    .validated = true,
                    .openable = true,
                },
                FileBrowserAssetCandidate{
                    .kind = MulAssetKind::radarcol,
                    .path = "ambiguous.mul",
                    .companionPath = {},
                    .label = "Radar colors",
                    .facet = {},
                    .reason = "Radarcol candidate",
                    .warning = {},
                    .score = 20,
                    .dimensions = std::nullopt,
                    .validated = true,
                    .openable = true,
                },
            },
        .assetCandidate = std::nullopt,
    };
    FileSelectorState selector;
    openFileSelector(selector, result);

    REQUIRE(selector.visible);
    REQUIRE(selector.candidates.size() == 2);
    REQUIRE(selector.candidates.front().kind == MulAssetKind::map);
    REQUIRE(selectedFileSelectorResult(selector).assetCandidate.has_value());
}

TEST_CASE("generic open exposes unsupported MUL as an explicit diagnostic")
{
    const auto path = std::filesystem::temp_directory_path() / "unknown-generic.mul";
    {
        std::ofstream output(path, std::ios::binary);
        output << "not a supported MUL asset";
    }

    const auto summary = inspectFileBrowserSelection(path);
    REQUIRE(summary.assetCandidates.empty());
    REQUIRE_FALSE(summary.error.empty());

    std::error_code error;
    std::filesystem::remove(path, error);
}

TEST_CASE("file browser save request preserves the initial filename")
{
    const auto path = std::filesystem::temp_directory_path() / "uo-mule-output.mul";
    FileBrowserState state;
    requestFileBrowser(state,
                       {
                           .mode = FileBrowserMode::saveFile,
                           .target = FileBrowserTarget::newMapOutput,
                           .initialPath = path,
                           .extension = ".mul",
                           .title = "Save",
                       });

    REQUIRE(state.visible);
    REQUIRE(state.directory == path.parent_path());
    REQUIRE(std::string(state.filename.data()) == path.filename().string());
    cancelFileBrowser(state);
    REQUIRE_FALSE(state.visible);
}

TEST_CASE("file browser reuses the last navigated folder in the same session")
{
    const auto directory = std::filesystem::temp_directory_path() / "uo-mule-file-browser-recent";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    REQUIRE_FALSE(error);

    FileBrowserState state;
    requestFileBrowser(state,
                       {
                           .mode = FileBrowserMode::openFile,
                           .target = FileBrowserTarget::genericOpen,
                           .initialPath = directory,
                           .extension = ".mul",
                           .title = "Open",
                       });
    REQUIRE(state.directory == std::filesystem::absolute(directory).lexically_normal());
    cancelFileBrowser(state);

    requestFileBrowser(state,
                       {
                           .mode = FileBrowserMode::openFile,
                           .target = FileBrowserTarget::genericOpen,
                           .initialPath = {},
                           .extension = ".mul",
                           .title = "Open",
                       });
    REQUIRE(state.directory == std::filesystem::absolute(directory).lexically_normal());

    cancelFileBrowser(state);
    std::filesystem::remove_all(directory, error);
}

TEST_CASE("file browser accumulates multiple recent folders")
{
    const auto root = std::filesystem::temp_directory_path() / "uo-mule-file-browser-recent-many";
    const auto first = root / "first";
    const auto second = root / "second";
    const auto third = root / "third";
    std::error_code error;
    std::filesystem::create_directories(first, error);
    std::filesystem::create_directories(second, error);
    std::filesystem::create_directories(third, error);
    REQUIRE_FALSE(error);

    FileBrowserState state;
    for (const auto & folder : {first, second, third})
    {
        requestFileBrowser(state,
                           {
                               .mode = FileBrowserMode::openFile,
                               .target = FileBrowserTarget::genericOpen,
                               .initialPath = folder,
                               .extension = ".mul",
                               .title = "Open",
                           });
        cancelFileBrowser(state);
    }

    REQUIRE(state.recentFolders.size() >= 3U);
    REQUIRE(state.recentFolders.at(0) == std::filesystem::absolute(third).lexically_normal());
    REQUIRE(state.recentFolders.at(1) == std::filesystem::absolute(second).lexically_normal());
    REQUIRE(state.recentFolders.at(2) == std::filesystem::absolute(first).lexically_normal());

    std::filesystem::remove_all(root, error);
}

} // namespace
} // namespace uo_mule::gui
