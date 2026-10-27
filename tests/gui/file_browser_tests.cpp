#include "gui/dialogs/file_browser_dialog.h"
#include "gui/dialogs/file_selector_dialog.h"
#include "gui/presentation/file_size.h"
#include "gui_model/dialogs/dialog_contracts.h"
#include "gui_model/dialogs/file_browser_model.h"
#include "gui_model/dialogs/file_deduction.h"
#include "gui_model/profile/data_profile.h"
#include "uomul/facet_sizes.h"
#include "uomul/map/map.h"
#include "uomul/radarcol/radarcol.h"
#include "uomul/texmaps/texmaps.h"
#include "uomul/tiledata/tiledata.h"
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
#include <vector>

namespace uo_mule::gui
{
namespace
{

constexpr std::uint16_t radarValueMask = 0x7FFFU;
constexpr std::uint16_t byteMask = 0xFFU;
constexpr unsigned byteShift = 8U;
constexpr std::streamsize emptyTextureIndexBytes = 12;
constexpr int mapCandidateScore = 80;
constexpr int radarCandidateScore = 20;
constexpr std::uint16_t fixtureTextureValue = 0x1234U;

void createTexmapsFixture(const std::filesystem::path & indexPath, const std::filesystem::path & dataPath)
{
    auto editor = uomul::TexMapsEditor::createEmpty(indexPath, dataPath);
    REQUIRE(editor);
    const uomul::TextureImage image{
        .dimension = uomul::textureSmallDimension,
        .pixels = std::vector<std::uint16_t>(uomul::textureSmallPixelCount, fixtureTextureValue),
    };
    REQUIRE(editor->editTexture(0, image));
    REQUIRE(editor->commit());
}

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
    REQUIRE(summary.assetCandidates.size() == 2U);
    REQUIRE(std::ranges::any_of(summary.assetCandidates,
                                [](const auto & candidate)
                                { return candidate.kind == MulAssetKind::radarcol && candidate.openable; }));
    REQUIRE(std::ranges::any_of(summary.assetCandidates,
                                [](const auto & candidate)
                                { return candidate.kind == MulAssetKind::texmapsSet && !candidate.openable; }));

    FileSelectorState selector;
    openFileSelector(selector,
                     {
                         .target = FileBrowserTarget::openGeneric,
                         .path = indexPath,
                         .mapCandidates = {},
                         .assetCandidates = summary.assetCandidates,
                         .assetCandidate = std::nullopt,
                     });
    REQUIRE(std::ranges::any_of(selector.candidates,
                                [](const auto & candidate) { return candidate.kind == MulAssetKind::texmapsSet; }));

    std::error_code error;
    std::filesystem::remove(indexPath, error);
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
    REQUIRE(std::ranges::any_of(summary.assetCandidates,
                                [](const auto & candidate)
                                { return candidate.kind == MulAssetKind::texmapsSet && candidate.warning.empty(); }));

    std::error_code error;
    std::filesystem::remove(indexPath, error);
    std::filesystem::remove(dataPath, error);
}

TEST_CASE("fileBrowser_Should_DeduceTexmapsSetFromEitherTestFile")
{
    const auto directory = std::filesystem::temp_directory_path() / "uo-mule-either-texmaps-file";
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    error.clear();
    std::filesystem::create_directories(directory, error);
    REQUIRE_FALSE(error);
    const auto indexPath = directory / "texidx.mul";
    const auto dataPath = directory / "texmaps.mul";
    createTexmapsFixture(indexPath, dataPath);

    for (const auto & selectedPath : {indexPath, dataPath})
    {
        const auto summary = inspectFileBrowserSelection(selectedPath);
        const auto set = std::ranges::find_if(
            summary.assetCandidates, [](const auto & candidate) { return candidate.kind == MulAssetKind::texmapsSet; });

        REQUIRE(set != summary.assetCandidates.end());
        REQUIRE(set->openable);
        REQUIRE(set->path == indexPath);
        REQUIRE(set->companionPath == dataPath);
    }

    std::filesystem::remove_all(directory, error);
}

void verifyNumberedRadarcol(const std::filesystem::path & directory)
{
    const auto radarPath = directory / "radarcol12.mul";
    auto radarEditor = uomul::RadarColEditor::createEmpty(radarPath);
    REQUIRE(radarEditor);
    REQUIRE(radarEditor->commit());
    const auto radarSummary = inspectFileBrowserSelection(radarPath);
    const auto radar = std::ranges::find_if(radarSummary.assetCandidates,
                                            [](const auto & candidate) { return candidate.kind == MulAssetKind::radarcol; });
    REQUIRE(radar != radarSummary.assetCandidates.end());
    REQUIRE(radar->warning.empty());
    REQUIRE(selectConfidentFileBrowserCandidate(radarSummary.assetCandidates).has_value());
}

void verifyNumberedTiledata(const std::filesystem::path & directory)
{
    const auto tiledataPath = directory / "tiledata12.mul";
    auto tiledata = uomul::TiledataEditor::createEmpty(tiledataPath, uomul::TiledataLayout::extended64);
    REQUIRE(tiledata);
    REQUIRE(tiledata->commit());
    const auto tiledataSummary = inspectFileBrowserSelection(tiledataPath);
    const auto tiledataCandidate = std::ranges::find_if(
        tiledataSummary.assetCandidates, [](const auto & candidate) { return candidate.kind == MulAssetKind::tiledata; });
    REQUIRE(tiledataCandidate != tiledataSummary.assetCandidates.end());
    REQUIRE(tiledataCandidate->warning.empty());
}

void verifyNumberedTexmaps(const std::filesystem::path & directory)
{
    const auto indexPath = directory / "texidx12.mul";
    const auto dataPath = directory / "texmaps12.mul";
    createTexmapsFixture(indexPath, dataPath);
    const auto texmapsSummary = inspectFileBrowserSelection(indexPath);
    const auto texmaps = std::ranges::find_if(
        texmapsSummary.assetCandidates, [](const auto & candidate) { return candidate.kind == MulAssetKind::texmapsSet; });
    REQUIRE(texmaps != texmapsSummary.assetCandidates.end());
    REQUIRE(texmaps->openable);
    REQUIRE(selectConfidentFileBrowserCandidate(texmapsSummary.assetCandidates).has_value());
}

void verifyNumberedMap(const std::filesystem::path & directory)
{
    const auto mapPath = directory / "map12.mul";
    const auto facet = uomul::knownFacetSizes().front();
    REQUIRE(uomul::MapEditor::createEmpty(mapPath,
                                          uomul::Dimensions{.widthTiles = facet.widthTiles, .heightTiles = facet.heightTiles}));
    const auto mapSummary = inspectFileBrowserSelection(mapPath);
    const auto map = std::ranges::find_if(mapSummary.assetCandidates,
                                          [](const auto & candidate) { return candidate.kind == MulAssetKind::map; });
    REQUIRE(map != mapSummary.assetCandidates.end());
    REQUIRE(map->warning.empty());
}

TEST_CASE("fileDeduction_Should_RecognizeNumberedCanonicalNames")
{
    const auto directory = std::filesystem::temp_directory_path() / "uo-mule-numbered-canonical-files";
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    error.clear();
    std::filesystem::create_directories(directory, error);
    REQUIRE_FALSE(error);

    verifyNumberedRadarcol(directory);
    verifyNumberedTiledata(directory);
    verifyNumberedTexmaps(directory);
    verifyNumberedMap(directory);

    std::filesystem::remove_all(directory, error);
}

TEST_CASE("fileBrowser_Should_SuggestRenamedIndexWithoutPairingCanonicalFiles")
{
    const auto directory = std::filesystem::temp_directory_path();
    std::error_code error;
    const auto indexPath = directory / "xidxet.mul";
    const auto dataPath = directory / "texmaps.mul";
    std::filesystem::remove(indexPath, error);
    error.clear();
    std::filesystem::remove(dataPath, error);
    createTexmapsFixture(indexPath, dataPath);

    const auto summary = inspectFileBrowserSelection(indexPath);
    const auto set = std::ranges::find_if(summary.assetCandidates,
                                          [](const auto & candidate) { return candidate.kind == MulAssetKind::texmapsSet; });
    REQUIRE(set != summary.assetCandidates.end());
    REQUIRE_FALSE(set->openable);
    REQUIRE(set->path == indexPath);
    REQUIRE(set->companionPath.empty());
    REQUIRE(set->warning == "Matching idx/maps file missing");
    REQUIRE(set->filenameWarning == "Unexpected filename");

    std::filesystem::remove(indexPath, error);
    error.clear();
    std::filesystem::remove(dataPath, error);
}

TEST_CASE("fileBrowser_Should_SuggestTexmapsForRenamedIndexWithoutCompanion")
{
    const auto directory = std::filesystem::temp_directory_path() / "uo-mule-missing-texmaps-companion";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    REQUIRE_FALSE(error);

    const auto indexPath = directory / "xidxet.mul";
    const auto dataPath = directory / "texmaps.mul";
    createTexmapsFixture(indexPath, dataPath);
    std::filesystem::remove(dataPath, error);

    const auto summary = inspectFileBrowserSelection(indexPath);
    const auto set = std::ranges::find_if(summary.assetCandidates,
                                          [](const auto & candidate) { return candidate.kind == MulAssetKind::texmapsSet; });
    REQUIRE(set != summary.assetCandidates.end());
    REQUIRE_FALSE(set->openable);
    REQUIRE(set->warning == "Matching idx/maps file missing");
    REQUIRE(set->filenameWarning == "Unexpected filename");

    std::filesystem::remove_all(directory, error);
}

void verifyUnavailableNoncanonicalTexmaps(const std::filesystem::path & selectedPath)
{
    const auto summary = inspectFileBrowserSelection(selectedPath);
    const auto possibleTexmaps = std::ranges::find_if(
        summary.assetCandidates, [](const auto & candidate) { return candidate.kind == MulAssetKind::texmapsSet; });
    REQUIRE(possibleTexmaps != summary.assetCandidates.end());
    REQUIRE_FALSE(possibleTexmaps->openable);
    REQUIRE(possibleTexmaps->companionPath.empty());
}

TEST_CASE("fileDeduction_Should_ValidateNoncanonicalTexmapsFixturesAfterCompanionSelection")
{
    const auto directory = std::filesystem::temp_directory_path() / "uo-mule-noncanonical-texmaps-fixtures";
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    error.clear();
    std::filesystem::create_directories(directory, error);
    REQUIRE_FALSE(error);
    const auto indexPath = directory / "xdixet.mul";
    const auto dataPath = directory / "spamxet.mul";
    createTexmapsFixture(indexPath, dataPath);

    verifyUnavailableNoncanonicalTexmaps(indexPath);
    verifyUnavailableNoncanonicalTexmaps(dataPath);

    const auto fromIndex = inspectTexmapsCompanion(indexPath, dataPath);
    REQUIRE(fromIndex.openable);
    REQUIRE(fromIndex.path == indexPath);
    REQUIRE(fromIndex.companionPath == dataPath);

    const auto fromData = inspectTexmapsCompanion(dataPath, indexPath);
    REQUIRE(fromData.openable);
    REQUIRE(fromData.path == indexPath);
    REQUIRE(fromData.companionPath == dataPath);

    std::filesystem::remove_all(directory, error);
}

TEST_CASE("fileDeduction_Should_SelectOnlyConfidentCandidates")
{
    FileBrowserAssetCandidate radarcol{
        .kind = MulAssetKind::radarcol,
        .path = "radarcol.mul",
        .companionPath = {},
        .label = "Radarcol",
        .facet = {},
        .reason = {},
        .warning = "Unexpected filename",
        .score = 0,
        .itemCount = 0,
        .dimensions = std::nullopt,
        .validated = true,
        .openable = true,
        .filenameWarning = {},
    };
    REQUIRE_FALSE(selectConfidentFileBrowserCandidate({radarcol}).has_value());

    radarcol.warning.clear();
    const FileBrowserAssetCandidate texmaps{
        .kind = MulAssetKind::texmapsSet,
        .path = "texidx.mul",
        .companionPath = "texmaps.mul",
        .label = "Texmaps",
        .facet = {},
        .reason = {},
        .warning = {},
        .score = 0,
        .itemCount = 0,
        .dimensions = std::nullopt,
        .validated = true,
        .openable = true,
        .filenameWarning = {},
    };
    const auto selected = selectConfidentFileBrowserCandidate({radarcol, texmaps});
    const auto selectedCandidate = selected.value_or(FileBrowserAssetCandidate{});
    REQUIRE(selectedCandidate.kind == MulAssetKind::texmapsSet);

    const FileBrowserAssetCandidate tiledata{
        .kind = MulAssetKind::tiledata,
        .path = "tiledata.mul",
        .companionPath = {},
        .label = "Tiledata",
        .facet = {},
        .reason = {},
        .warning = {},
        .score = 0,
        .itemCount = 0,
        .dimensions = std::nullopt,
        .validated = true,
        .openable = true,
        .filenameWarning = {},
    };
    REQUIRE_FALSE(selectConfidentFileBrowserCandidate({radarcol, tiledata}).has_value());
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
        .target = FileBrowserTarget::openGeneric,
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
        .target = FileBrowserTarget::openGeneric,
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
                    .filenameWarning = {},
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
                    .filenameWarning = {},
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

TEST_CASE("genericOpen_Should_OfferTexmapsAndMapSetupForUnknownMul")
{
    const auto path = std::filesystem::temp_directory_path() / "unknown-generic.mul";
    {
        std::ofstream output(path, std::ios::binary);
        output << "not a supported MUL asset";
    }

    const auto summary = inspectFileBrowserSelection(path);
    REQUIRE(std::ranges::any_of(summary.assetCandidates,
                                [](const auto & candidate)
                                { return candidate.kind == MulAssetKind::texmapsSet && !candidate.openable; }));
    REQUIRE(std::ranges::any_of(summary.assetCandidates,
                                [](const auto & candidate)
                                { return candidate.kind == MulAssetKind::map && !candidate.openable; }));

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
                           .target = FileBrowserTarget::createMap,
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
                           .target = FileBrowserTarget::openGeneric,
                           .initialPath = directory,
                           .extension = ".mul",
                           .title = "Open",
                       });
    REQUIRE(state.directory == std::filesystem::absolute(directory).lexically_normal());
    cancelFileBrowser(state);

    requestFileBrowser(state,
                       {
                           .mode = FileBrowserMode::openFile,
                           .target = FileBrowserTarget::openGeneric,
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
                               .target = FileBrowserTarget::openGeneric,
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
