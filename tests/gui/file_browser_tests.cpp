#include "gui/dialogs/file_browser_dialog.h"
#include "gui/dialogs/file_selector_dialog.h"
#include "gui/presentation/file_size.h"
#include "gui_model/dialogs/dialog_contracts.h"
#include "gui_model/dialogs/file_browser_model.h"
#include "gui_model/profile/data_profile.h"
#include "uomul/facet_sizes.h"
#include "uomul/map/map.h"
#include "uomul/radarcol/radarcol.h"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <system_error>

namespace uo_mule::gui
{
namespace
{

constexpr std::uint16_t radarValueMask = 0x7FFFU;
constexpr std::uint16_t byteMask = 0xFFU;
constexpr unsigned byteShift = 8U;
constexpr std::streamsize emptyTextureIndexBytes = 12;
constexpr int directTextureScore = 90;
constexpr int mapCandidateScore = 80;
constexpr int radarCandidateScore = 20;

TEST_CASE("fileBrowser_Should_MatchExtensionsCaseInsensitively")
{
    REQUIRE(fileBrowserMatchesFilter("terrain.MUL", ".mul"));
    REQUIRE(fileBrowserMatchesFilter("terrain.mul", ".mul"));
    REQUIRE_FALSE(fileBrowserMatchesFilter("terrain.map", ".mul"));
    REQUIRE(fileBrowserMatchesFilter("texture.PNG", ".bmp,.png"));
    REQUIRE_FALSE(fileBrowserMatchesFilter("texture.jpg", ".bmp,.png"));
    REQUIRE(fileBrowserMatchesFilter("terrain.mul", {}));
}

TEST_CASE("fileBrowser_Should_FormatFileSizes")
{
    REQUIRE(formatFileSize(9999U) == "9999 B");
    REQUIRE(formatFileSize(10000U) == "10.0 kB");
    REQUIRE(formatFileSize(1234567U) == "1.23 MB");
}

TEST_CASE("fileBrowser_Should_InspectGenericFileMetadata")
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
    REQUIRE(summary.size.value_or(0U) == 4U);

    std::error_code error;
    std::filesystem::remove(path, error);
}

TEST_CASE("fileBrowser_Should_IdentifyValidRadarColorTable")
{
    const auto path = std::filesystem::temp_directory_path() / "radarcol.mul";
    {
        std::ofstream output(path, std::ios::binary);
        for (std::uint32_t index = 0; index < uomul::radarColorCount; index++)
        {
            const std::uint16_t value = static_cast<std::uint16_t>(index & radarValueMask);
            output.put(static_cast<char>(value & byteMask));
            output.put(static_cast<char>(value >> byteShift));
        }
    }

    const auto summary = inspectFileBrowserSelection(path);
    REQUIRE(std::ranges::any_of(summary.assetCandidates,
                                [](const auto & candidate)
                                { return candidate.kind == MulAssetKind::radarcol && candidate.openable; }));

    std::error_code error;
    std::filesystem::remove(path, error);
}

TEST_CASE("fileBrowser_Should_NotTreatIndexFileAsOpenableTexmapsSet_If_SetIsMissing")
{
    const auto directory = std::filesystem::temp_directory_path();
    const auto indexPath = directory / "texidx.mul";
    {
        std::ofstream output(indexPath, std::ios::binary);
        output.write("\0\0\0\0\0\0\0\0\0\0\0\0", emptyTextureIndexBytes);
    }

    const auto summary = inspectFileBrowserSelection(indexPath);
    REQUIRE(std::ranges::any_of(summary.assetCandidates,
                                [](const auto & candidate)
                                { return candidate.kind == MulAssetKind::texmapsSet && !candidate.openable; }));

    std::error_code error;
    std::filesystem::remove(indexPath, error);
}

TEST_CASE("fileSelector_Should_RejectDirectTextureIndexResult")
{
    FileSelectorState state;
    FileBrowserResult result;
    result.target = FileBrowserTarget::texmapsIndex;
    result.path = "texidx.mul";
    result.assetCandidates.push_back(FileBrowserAssetCandidate{
        .kind = MulAssetKind::texmapsSet,
        .path = result.path,
        .companionPath = {},
        .label = "Textmaps",
        .facet = {},
        .reason = "A matching texmaps-set is required",
        .warning = {},
        .score = directTextureScore,
        .dimensions = std::nullopt,
        .validated = false,
        .openable = false,
    });
    openFileSelector(state, result);

    REQUIRE(state.visible);
    const auto selected = selectedFileSelectorResult(state);
    REQUIRE(selected.assetCandidate.has_value());
    REQUIRE_FALSE(selected.assetCandidate.value_or(FileBrowserAssetCandidate{}).openable);
}

TEST_CASE("fileBrowser_Should_TreatEmptyTexmapsSetAsOneOpenableAsset")
{
    const auto directory = std::filesystem::temp_directory_path();
    const auto indexPath = directory / "texidx.mul";
    const auto dataPath = directory / "texmaps.mul";
    {
        const std::ofstream index(indexPath, std::ios::binary);
        const std::ofstream data(dataPath, std::ios::binary);
        static_cast<void>(index);
        static_cast<void>(data);
    }

    const auto summary = inspectFileBrowserSelection(indexPath);
    const auto set = std::ranges::find_if(summary.assetCandidates,
                                          [](const auto & candidate) { return candidate.kind == MulAssetKind::texmapsSet; });
    REQUIRE(set != summary.assetCandidates.end());
    REQUIRE(set->openable);
    REQUIRE(set->path == indexPath);
    REQUIRE(set->companionPath == dataPath);

    std::error_code error;
    std::filesystem::remove(indexPath, error);
    std::filesystem::remove(dataPath, error);
}

TEST_CASE("genericOpenCandidateSelection_Should_PreserveFormatAndMapDimensions")
{
    const auto directory = std::filesystem::temp_directory_path();
    const auto path = directory / "generic-map.mul";
    const auto dimensions = uomul::knownFacetSizes().front();
    std::error_code error;
    std::filesystem::remove(path, error);
    REQUIRE(uomul::MapEditor::createEmpty(
        path, uomul::Dimensions{.widthTiles = dimensions.widthTiles, .heightTiles = dimensions.heightTiles}));

    const auto summary = inspectFileBrowserSelection(path);
    const FileBrowserResult result{
        .target = FileBrowserTarget::genericOpen,
        .path = path,
        .mapCandidates = summary.mapCandidates,
        .assetCandidates = summary.assetCandidates,
        .assetCandidate = std::nullopt,
    };
    FileSelectorState selector;
    openFileSelector(selector, result);
    const auto mapCandidate =
        std::ranges::find_if(selector.candidates, [](const auto & candidate) { return candidate.kind == MulAssetKind::map; });
    REQUIRE(mapCandidate != selector.candidates.end());
    selector.selectedCandidate = static_cast<std::size_t>(std::distance(selector.candidates.begin(), mapCandidate));
    const auto selected = selectedFileSelectorResult(selector);

    REQUIRE(selected.assetCandidate.has_value());
    const auto selectedCandidate = selected.assetCandidate.value_or(FileBrowserAssetCandidate{});
    REQUIRE(selectedCandidate.kind == MulAssetKind::map);
    REQUIRE(selectedCandidate.dimensions.has_value());
    REQUIRE(selectedCandidate.dimensions.value_or(uomul::Dimensions{}).widthTiles == dimensions.widthTiles);

    std::filesystem::remove(path, error);
}

TEST_CASE("genericOpenSelector_Should_KeepMultipleCandidatesAvailable_If_MultipleCandidatesExist")
{
    const FileBrowserResult result{
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
                    .score = mapCandidateScore,
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
                    .score = radarCandidateScore,
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

TEST_CASE("genericOpen_Should_ExposeUnsupportedMulAsExplicitDiagnostic")
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

TEST_CASE("fileBrowserSaveRequest_Should_PreserveInitialFilename")
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

TEST_CASE("fileBrowser_Should_ReuseLastNavigatedFolder_If_SameSession")
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

TEST_CASE("fileBrowser_Should_AccumulateMultipleRecentFolders")
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
