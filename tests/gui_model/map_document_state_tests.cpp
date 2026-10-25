#include "gui_model/documents/map_document_manager.h"
#include "gui_model/documents/map_document_state.h"
#include "gui_model/tools/tool_contracts.h"
#include "gui_model/world/map_selection.h"
#include "uomul/map/map.h"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace
{

using uo_mule::gui::FillMode;
using uo_mule::gui::MapDocumentManager;
using uo_mule::gui::MapDocumentState;
using uo_mule::gui::MapEditScope;
using uo_mule::gui::MapSelectionMode;
using uo_mule::gui::PendingChangesAction;
using uo_mule::gui::ReplaceMode;

constexpr uomul::Dimensions testDimensions{.widthTiles = 16, .heightTiles = 16};
constexpr uomul::MapTile initialTile{.id = 17, .z = -4};
constexpr double canvasOriginX = 10.0;
constexpr double canvasOriginY = 20.0;
constexpr double tilePixelSize = 10.0;
constexpr double panOffset = 5.0;
constexpr double screenTileX = 35.0;
constexpr double screenTileY = 55.0;
constexpr double outsideScreenCoordinate = -1.0;
constexpr double zeroOffset = 0.0;
constexpr double zeroTileSize = 0.0;
constexpr double mapRightEdgeScreenCoordinate =
    canvasOriginX + (static_cast<double>(testDimensions.widthTiles) * tilePixelSize);
constexpr std::uint32_t tileSelectionMaxY = 5;
constexpr std::uint16_t deterministicTileId = 1234;
constexpr std::uint16_t alternateTileId = 1235;
constexpr std::uint16_t selectionReplacementTileId = 99;
constexpr std::uint16_t selectedReplacementTileId = 23;
constexpr std::uint16_t rejectedReplacementTileId = 44;
constexpr std::int8_t filledElevation = 2;
constexpr std::int8_t blockElevation = 31;
constexpr std::uint64_t selectedTileCount = 4;
constexpr std::uint32_t expectedSelectedX = 2;
constexpr std::uint32_t expectedSelectedY = 3;
std::atomic<std::uint64_t> testDirectoryCounter = 0;

class TestDirectory
{
public:
    TestDirectory()
    {
        const auto sequence = testDirectoryCounter.fetch_add(1);
        path_ = std::filesystem::temp_directory_path() / ("map-document-state-tests-" + std::to_string(sequence));
        std::filesystem::create_directories(path_);
    }

    TestDirectory(const TestDirectory &) = delete;
    TestDirectory & operator=(const TestDirectory &) = delete;
    TestDirectory(TestDirectory &&) = delete;
    TestDirectory & operator=(TestDirectory &&) = delete;

    ~TestDirectory()
    {
        std::error_code ignoredError;
        std::filesystem::remove_all(path_, ignoredError);
    }

    [[nodiscard]] const std::filesystem::path & path() const { return path_; }

private:
    std::filesystem::path path_;
};

bool hasExpectedResidentState(const MapDocumentManager & manager)
{
    const auto selectedTile = manager.active().viewState().selectedTile;
    if (!selectedTile)
    {
        return false;
    }
    return selectedTile->x == expectedSelectedX && selectedTile->y == expectedSelectedY;
}

} // namespace

TEST_CASE("selectionInspectorTile_Should_UseTileOrContainingBlockOrigin")
{
    const uomul::TileCoord tile{.x = 19, .y = 10};
    const uomul::TileCoord tileSelection = uo_mule::gui::selectionInspectorTile(uo_mule::gui::MapSelectionMode::tiles, tile);
    REQUIRE(tileSelection.x == 19);
    REQUIRE(tileSelection.y == 10);
    const uomul::TileCoord blockSelection = uo_mule::gui::selectionInspectorTile(uo_mule::gui::MapSelectionMode::blocks, tile);
    REQUIRE(blockSelection.x == 16);
    REQUIRE(blockSelection.y == 8);
}

TEST_CASE("mapCoordinateHelpers_Should_ConvertScreenAndTileCoordinates")
{
    const auto block = uo_mule::gui::tileToBlock({.x = 15, .y = 8});
    REQUIRE(block.x == 1);
    REQUIRE(block.y == 1);
    const auto tile = uo_mule::gui::screenToTile(
        screenTileX, screenTileY, canvasOriginX, canvasOriginY, panOffset, panOffset, tilePixelSize, testDimensions);
    const auto convertedTile = tile.value_or(uomul::TileCoord{});
    REQUIRE(tile.has_value());
    REQUIRE(convertedTile.x == 2);
    REQUIRE(convertedTile.y == 3);
    REQUIRE_FALSE(uo_mule::gui::screenToTile(canvasOriginX + outsideScreenCoordinate,
                                             canvasOriginY + panOffset,
                                             canvasOriginX,
                                             canvasOriginY,
                                             zeroOffset,
                                             zeroOffset,
                                             tilePixelSize,
                                             testDimensions));
    REQUIRE_FALSE(uo_mule::gui::screenToTile(canvasOriginX + panOffset,
                                             canvasOriginY + outsideScreenCoordinate,
                                             canvasOriginX,
                                             canvasOriginY,
                                             zeroOffset,
                                             zeroOffset,
                                             tilePixelSize,
                                             testDimensions));
    REQUIRE_FALSE(uo_mule::gui::screenToTile(mapRightEdgeScreenCoordinate,
                                             canvasOriginY,
                                             canvasOriginX,
                                             canvasOriginY,
                                             zeroOffset,
                                             zeroOffset,
                                             tilePixelSize,
                                             testDimensions));
    REQUIRE_FALSE(uo_mule::gui::screenToTile(
        canvasOriginX, canvasOriginY, canvasOriginX, canvasOriginY, zeroOffset, zeroOffset, zeroTileSize, testDimensions));
}

TEST_CASE("mapSelectionHelpers_Should_CreateHalfOpenSingleAndReverseDragRegions")
{
    const auto singleTile = uo_mule::gui::tileSelectionFromDrag({.x = 4, .y = 5}, {.x = 4, .y = 5}, testDimensions);
    REQUIRE(singleTile.min.x == 4);
    REQUIRE(singleTile.min.y == 5);
    REQUIRE(singleTile.maxExclusive.x == 5);
    REQUIRE(singleTile.maxExclusive.y == 6);
    const auto reverseDrag = uo_mule::gui::tileSelectionFromDrag({.x = 9, .y = 12}, {.x = 3, .y = 5}, testDimensions);
    REQUIRE(reverseDrag.min.x == 3);
    REQUIRE(reverseDrag.min.y == 5);
    REQUIRE(reverseDrag.maxExclusive.x == 10);
    REQUIRE(reverseDrag.maxExclusive.y == 13);
    const auto edgeSelection = uo_mule::gui::tileSelectionFromDrag(
        {.x = 15, .y = 15}, {.x = std::numeric_limits<std::uint32_t>::max(), .y = 100}, testDimensions);
    REQUIRE(edgeSelection.min.x == 15);
    REQUIRE(edgeSelection.min.y == 15);
    REQUIRE(edgeSelection.maxExclusive.x == 16);
    REQUIRE(edgeSelection.maxExclusive.y == 16);
    const auto blocks = uo_mule::gui::blockSelectionFromDrag({.x = 7, .y = 8}, {.x = 8, .y = 15}, testDimensions);
    REQUIRE(blocks.min.x == 0);
    REQUIRE(blocks.min.y == 1);
    REQUIRE(blocks.maxExclusive.x == 2);
    REQUIRE(blocks.maxExclusive.y == 2);
    const auto clamped = uo_mule::gui::clampBlockRegion({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 9, .y = 9}},
                                                        {.widthTiles = 24, .heightTiles = 9});
    REQUIRE(clamped.min.x == 0);
    REQUIRE(clamped.min.y == 0);
    REQUIRE(clamped.maxExclusive.x == 3);
    REQUIRE(clamped.maxExclusive.y == 2);
}

TEST_CASE("mapColorHelpers_Should_BeDeterministicAndMapSignedElevationExtrema")
{
    REQUIRE(uo_mule::gui::tileIdColor(deterministicTileId) == uo_mule::gui::tileIdColor(deterministicTileId));
    REQUIRE_FALSE(uo_mule::gui::tileIdColor(deterministicTileId) == uo_mule::gui::tileIdColor(alternateTileId));
    REQUIRE(uo_mule::gui::zGrayscale(std::numeric_limits<std::int8_t>::min()) == 0);
    REQUIRE(uo_mule::gui::zGrayscale(0) == 128);
    REQUIRE(uo_mule::gui::zGrayscale(std::numeric_limits<std::int8_t>::max()) == 255);
    const uomul::MapTile tile{.id = deterministicTileId, .z = -12};
    REQUIRE(uo_mule::gui::tileDisplayColor(uo_mule::gui::MapDisplayMode::tileIds, tile) == uo_mule::gui::tileIdColor(tile.id));
    const uo_mule::gui::MapColor radarColor{.red = 12, .green = 34, .blue = 56};
    REQUIRE(uo_mule::gui::tileDisplayColor(uo_mule::gui::MapDisplayMode::tileIds, tile, radarColor) == radarColor);
    const auto zColor = uo_mule::gui::tileDisplayColor(uo_mule::gui::MapDisplayMode::z, tile, radarColor);
    REQUIRE(zColor.red == uo_mule::gui::zGrayscale(tile.z));
    REQUIRE(zColor.green == zColor.red);
    REQUIRE(zColor.blue == zColor.red);
    REQUIRE_FALSE(uo_mule::gui::tileDisplayColor(uo_mule::gui::MapDisplayMode::combined, tile) ==
                  uo_mule::gui::tileIdColor(tile.id));
}

TEST_CASE("mapDocumentState_Should_OpenCreateHoverAndExposeSelectionState")
{
    const TestDirectory directory;
    const auto mapPath = directory.path() / "created.mul";
    MapDocumentState mapDocumentState;

    REQUIRE(mapDocumentState.createEmptyMap(
        mapPath, testDimensions, PendingChangesAction::cancel, std::nullopt, false, initialTile));
    REQUIRE(mapDocumentState.changes().reader.has_value());
    REQUIRE(mapDocumentState.changes().mapPath == mapPath);
    REQUIRE(mapDocumentState.changes().dimensions == testDimensions);
    REQUIRE(mapDocumentState.changes().reader->readTile({.x = 8, .y = 9}) == initialTile);
    const uomul::TileCoord hoveredCoordinate{.x = 8, .y = 9};
    REQUIRE(mapDocumentState.updateHover(hoveredCoordinate));
    REQUIRE(mapDocumentState.viewState().hoveredTile.has_value());
    const auto hoveredTile = mapDocumentState.viewState().hoveredTile.value_or(uomul::TileCoord{});
    REQUIRE(hoveredTile.x == hoveredCoordinate.x);
    REQUIRE(hoveredTile.y == hoveredCoordinate.y);
    REQUIRE(mapDocumentState.viewState().hoveredValue == initialTile);
    mapDocumentState.viewState().clearHover();
    REQUIRE_FALSE(mapDocumentState.viewState().hoveredTile);
    REQUIRE_FALSE(mapDocumentState.viewState().selectedTile);
    const uomul::TileCoord selectedCoordinate{.x = 3, .y = 4};
    REQUIRE(mapDocumentState.updateSelectedTile(selectedCoordinate));
    REQUIRE(mapDocumentState.viewState().selectedTile.has_value());
    const auto selectedTile = mapDocumentState.viewState().selectedTile.value_or(uomul::TileCoord{});
    REQUIRE(selectedTile.x == selectedCoordinate.x);
    REQUIRE(selectedTile.y == selectedCoordinate.y);
    REQUIRE(mapDocumentState.viewState().selectedValue == initialTile);
    mapDocumentState.viewState().clearHover();
    REQUIRE_FALSE(mapDocumentState.viewState().hoveredTile);
    REQUIRE(mapDocumentState.viewState().selectedTile.has_value());
    const auto remainingSelectedTile = mapDocumentState.viewState().selectedTile.value_or(uomul::TileCoord{});
    REQUIRE(remainingSelectedTile.x == selectedCoordinate.x);
    REQUIRE(remainingSelectedTile.y == selectedCoordinate.y);

    mapDocumentState.viewState().setTileSelection({.min = {.x = 2, .y = 3}, .maxExclusive = {.x = 4, .y = tileSelectionMaxY}},
                                                  mapDocumentState.changes().dimensions);
    REQUIRE(mapDocumentState.viewState().selectionMode == MapSelectionMode::tiles);
    const auto tileStats = mapDocumentState.selectionStats();
    REQUIRE(tileStats.has_value());
    REQUIRE(tileStats.has_value());
    const auto availableTileStats = tileStats.value_or(std::nullopt);
    REQUIRE(availableTileStats.has_value());
    REQUIRE(availableTileStats.value_or(uomul::MapStats{}).tileCount == selectedTileCount);

    mapDocumentState.viewState().setSelectionMode(MapSelectionMode::blocks, mapDocumentState.changes().dimensions);
    REQUIRE_FALSE(mapDocumentState.viewState().tileSelection);
    mapDocumentState.viewState().setBlockSelection({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 1, .y = 1}},
                                                   mapDocumentState.changes().dimensions);
    const auto blockStats = mapDocumentState.selectionStats();
    REQUIRE(blockStats.has_value());
    REQUIRE(blockStats.has_value());
    const auto availableBlockStats = blockStats.value_or(std::nullopt);
    REQUIRE(availableBlockStats.has_value());
    REQUIRE(availableBlockStats.value_or(uomul::MapStats{}).tileCount == uomul::mapBlockTileCount);

    REQUIRE_FALSE(mapDocumentState.updateHover({.x = 16, .y = 0}));
    REQUIRE_FALSE(mapDocumentState.viewState().hoveredTile);
    REQUIRE_FALSE(mapDocumentState.viewState().hoveredValue);
    REQUIRE(mapDocumentState.changes().error);
    mapDocumentState.viewState().clearHover();
    REQUIRE_FALSE(mapDocumentState.viewState().hoveredTile);
}

TEST_CASE("mapDocumentState_Should_ApplyLassoSelectionWithCombineModes")
{
    const TestDirectory directory;
    const auto mapPath = directory.path() / "lasso.mul";
    MapDocumentState mapDocumentState;
    REQUIRE(mapDocumentState.createEmptyMap(mapPath, testDimensions));

    const std::vector<uomul::TileCoord> firstLasso{
        {.x = 1, .y = 1},
        {.x = 4, .y = 1},
        {.x = 4, .y = 4},
        {.x = 1, .y = 4},
    };
    mapDocumentState.viewState().applyLassoSelection(
        firstLasso, uo_mule::gui::SelectionCombineMode::replace, mapDocumentState.changes().dimensions);
    REQUIRE(mapDocumentState.viewState().selectionMask.size() == 9);

    const std::vector<uomul::TileCoord> secondLasso{
        {.x = 3, .y = 3},
        {.x = 5, .y = 3},
        {.x = 5, .y = 5},
        {.x = 3, .y = 5},
    };
    mapDocumentState.viewState().applyLassoSelection(
        secondLasso, uo_mule::gui::SelectionCombineMode::add, mapDocumentState.changes().dimensions);
    REQUIRE(mapDocumentState.viewState().selectionMask.size() == 12);
    mapDocumentState.viewState().applyLassoSelection(
        secondLasso, uo_mule::gui::SelectionCombineMode::subtract, mapDocumentState.changes().dimensions);
    REQUIRE(mapDocumentState.viewState().selectionMask.size() == 8);
}

TEST_CASE("mapDocumentState_Should_ConvertBlockSelectionBackToClippedTiles")
{
    const TestDirectory directory;
    const auto mapPath = directory.path() / "selection-conversion.mul";
    REQUIRE(uomul::MapEditor::createEmpty(mapPath, {.widthTiles = 16, .heightTiles = 16}));

    MapDocumentState mapDocumentState;
    REQUIRE(mapDocumentState.openMap(mapPath, {.widthTiles = 16, .heightTiles = 16}));
    mapDocumentState.viewState().setBlockSelection({.min = {.x = 1, .y = 1}, .maxExclusive = {.x = 2, .y = 2}},
                                                   mapDocumentState.changes().dimensions);
    mapDocumentState.viewState().setSelectionMode(MapSelectionMode::tiles, mapDocumentState.changes().dimensions);

    REQUIRE(mapDocumentState.viewState().selectionMode == MapSelectionMode::tiles);
    REQUIRE(mapDocumentState.viewState().selectionMask.size() == 64);
    REQUIRE(mapDocumentState.viewState().selectionMask.contains(uomul::TileCoord{.x = 8, .y = 8}));
    REQUIRE(mapDocumentState.viewState().selectionMask.contains(uomul::TileCoord{.x = 15, .y = 15}));
}

TEST_CASE("mapDocumentState_Should_ExposeSparseSelectionBoundsAndStats")
{
    const TestDirectory directory;
    const auto mapPath = directory.path() / "selection-bounds.mul";
    REQUIRE(uomul::MapEditor::createEmpty(mapPath, testDimensions, initialTile));
    MapDocumentState mapDocumentState;
    REQUIRE(mapDocumentState.openMap(mapPath, testDimensions));
    mapDocumentState.viewState().applyLassoSelection({{.x = 1, .y = 1}, {.x = 4, .y = 1}, {.x = 4, .y = 4}, {.x = 1, .y = 4}},
                                                     uo_mule::gui::SelectionCombineMode::replace,
                                                     mapDocumentState.changes().dimensions);

    const auto bounds = mapDocumentState.viewState().selectionMask.tileBounds(mapDocumentState.changes().dimensions);
    REQUIRE(bounds.has_value());
    const auto boundsValue = bounds.value_or(uomul::TileRegion{});
    REQUIRE(boundsValue.min.x == 1);
    REQUIRE(boundsValue.min.y == 1);
    REQUIRE(boundsValue.maxExclusive.x == 4);
    REQUIRE(boundsValue.maxExclusive.y == 4);
    const auto stats = mapDocumentState.selectionStats();
    REQUIRE(stats.has_value());
    REQUIRE(stats->has_value());
    const auto statsValue = stats.value().value_or(uomul::MapStats{});
    REQUIRE(statsValue.tileCount == 9);
}

TEST_CASE("mapDocumentState_Should_StageEditsAndRequireExplicitDirtyResolution")
{
    const TestDirectory directory;
    const auto originalPath = directory.path() / "original.mul";
    const auto otherPath = directory.path() / "other.mul";
    const auto savedPath = directory.path() / "saved.mul";
    REQUIRE(uomul::MapEditor::createEmpty(originalPath, testDimensions, initialTile));
    REQUIRE(uomul::MapEditor::createEmpty(otherPath, testDimensions));

    MapDocumentState mapDocumentState;
    REQUIRE(mapDocumentState.openMap(originalPath, testDimensions));
    mapDocumentState.viewState().setTileSelection({.min = {.x = 1, .y = 2}, .maxExclusive = {.x = 3, .y = 4}},
                                                  mapDocumentState.changes().dimensions);
    REQUIRE(mapDocumentState.fill({.id = selectionReplacementTileId, .z = filledElevation}, MapEditScope::selection));
    REQUIRE(mapDocumentState.changes().dirty());
    REQUIRE_FALSE(mapDocumentState.openMap(otherPath, testDimensions));
    REQUIRE(mapDocumentState.changes().dirty());
    REQUIRE(mapDocumentState.changes().workingPath.has_value());
    const auto & stagedPath = mapDocumentState.changes().workingPath;
    REQUIRE(mapDocumentState.changes().mapPath == stagedPath.value_or(std::filesystem::path{}));
    REQUIRE(mapDocumentState.changes().reader->readTile({.x = 1, .y = 2}) ==
            uomul::MapTile{.id = selectionReplacementTileId, .z = filledElevation});
    REQUIRE(mapDocumentState.changes().reader->readTile({.x = 0, .y = 0}) == initialTile);

    REQUIRE_FALSE(mapDocumentState.openMap(otherPath, testDimensions, PendingChangesAction::cancel));
    REQUIRE(mapDocumentState.openMap(otherPath, testDimensions, PendingChangesAction::discard));
    REQUIRE_FALSE(mapDocumentState.changes().dirty());
    REQUIRE(mapDocumentState.changes().mapPath == otherPath);

    REQUIRE(mapDocumentState.openMap(originalPath, testDimensions));
    REQUIRE(mapDocumentState.replaceTileId(initialTile.id, 23, MapEditScope::wholeMap));
    REQUIRE(mapDocumentState.changes().reader->readTile({.x = 0, .y = 0}) ==
            uomul::MapTile{.id = selectedReplacementTileId, .z = initialTile.z});
    REQUIRE(mapDocumentState.replaceZ(initialTile.z, 12, MapEditScope::wholeMap));
    REQUIRE(mapDocumentState.changes().reader->readTile({.x = 0, .y = 0}) ==
            uomul::MapTile{.id = selectedReplacementTileId, .z = 12});
    REQUIRE(mapDocumentState.save(savedPath));
    REQUIRE_FALSE(mapDocumentState.changes().dirty());
    REQUIRE(mapDocumentState.changes().mapPath == savedPath);
    const auto savedReader = uomul::MapReader::open(savedPath, testDimensions);
    REQUIRE(savedReader);
    REQUIRE(savedReader->readTile({.x = 0, .y = 0}) == uomul::MapTile{.id = selectedReplacementTileId, .z = 12});
}

TEST_CASE("mapDocumentState_Should_RejectSelectionEditsWithoutSelectionAndDiscardBackToSource")
{
    const TestDirectory directory;
    const auto sourcePath = directory.path() / "source.mul";
    REQUIRE(uomul::MapEditor::createEmpty(sourcePath, testDimensions, initialTile));
    MapDocumentState mapDocumentState;
    REQUIRE(mapDocumentState.openMap(sourcePath, testDimensions));
    REQUIRE_FALSE(mapDocumentState.replaceTileId(initialTile.id, rejectedReplacementTileId, MapEditScope::selection));
    REQUIRE_FALSE(mapDocumentState.changes().dirty());
    mapDocumentState.viewState().setBlockSelection({.min = {.x = 1, .y = 1}, .maxExclusive = {.x = 2, .y = 2}},
                                                   mapDocumentState.changes().dimensions);
    REQUIRE(mapDocumentState.replaceZ(initialTile.z, blockElevation, MapEditScope::selection));
    REQUIRE(mapDocumentState.changes().reader->readTile({.x = 8, .y = 8}) ==
            uomul::MapTile{.id = initialTile.id, .z = blockElevation});
    REQUIRE(mapDocumentState.changes().workingPath.has_value());
    const auto workingPath = mapDocumentState.changes().workingPath.value_or(std::filesystem::path{});
    REQUIRE(mapDocumentState.discard());
    REQUIRE_FALSE(mapDocumentState.changes().dirty());
    REQUIRE_FALSE(std::filesystem::exists(workingPath));
    REQUIRE(mapDocumentState.changes().reader->readTile({.x = 8, .y = 8}) == initialTile);
    REQUIRE(mapDocumentState.closeMap());
    REQUIRE(!mapDocumentState.changes().reader.has_value());
    REQUIRE(mapDocumentState.changes().mapPath.empty());
}

TEST_CASE("mapDocumentState_Should_ApplyMaskedFillModesAndPreserveSparseTileSelection")
{
    const TestDirectory directory;
    const auto mapPath = directory.path() / "masked-fill.mul";
    REQUIRE(uomul::MapEditor::createEmpty(mapPath, testDimensions, initialTile));

    MapDocumentState mapDocumentState;
    REQUIRE(mapDocumentState.openMap(mapPath, testDimensions));
    mapDocumentState.viewState().setTileSelection({.min = {.x = 1, .y = 1}, .maxExclusive = {.x = 3, .y = 3}},
                                                  mapDocumentState.changes().dimensions);
    mapDocumentState.viewState().applyTileSelection({.min = {.x = 2, .y = 2}, .maxExclusive = {.x = 3, .y = 3}},
                                                    uo_mule::gui::SelectionCombineMode::subtract,
                                                    mapDocumentState.changes().dimensions);
    REQUIRE(mapDocumentState.viewState().selectionMask.size() == 3);

    const auto idFill = mapDocumentState.fillMasked({.id = 90, .z = 50}, FillMode::id);
    REQUIRE(idFill);
    REQUIRE(idFill->changedTiles == 3);
    REQUIRE(mapDocumentState.viewState().selectionMask.size() == 3);
    REQUIRE(mapDocumentState.viewState().selectionMask.contains(uomul::TileCoord{.x = 1, .y = 1}));
    REQUIRE(mapDocumentState.changes().reader->readTile({.x = 1, .y = 1}) == uomul::MapTile{.id = 90, .z = initialTile.z});
    REQUIRE(mapDocumentState.changes().reader->readTile({.x = 2, .y = 2}) == initialTile);

    const auto zFill = mapDocumentState.fillMasked({.id = 80, .z = 51}, FillMode::z);
    REQUIRE(zFill);
    REQUIRE(zFill->changedTiles == 3);
    REQUIRE(mapDocumentState.changes().reader->readTile({.x = 1, .y = 1}) == uomul::MapTile{.id = 90, .z = 51});

    const auto combinedFill = mapDocumentState.fillMasked({.id = 70, .z = 52}, FillMode::idAndZ);
    REQUIRE(combinedFill);
    REQUIRE(combinedFill->changedTiles == 3);
    REQUIRE(mapDocumentState.changes().reader->readTile({.x = 1, .y = 1}) == uomul::MapTile{.id = 70, .z = 52});
    REQUIRE(mapDocumentState.changes().reader->readTile({.x = 0, .y = 0}) == initialTile);
    REQUIRE(mapDocumentState.changes().dirty());
}

TEST_CASE("mapDocumentState_Should_ApplyMaskedReplaceModesToSparseTiles")
{
    const TestDirectory directory;
    const auto mapPath = directory.path() / "masked-replace.mul";
    REQUIRE(uomul::MapEditor::createEmpty(mapPath, testDimensions, initialTile));

    MapDocumentState mapDocumentState;
    REQUIRE(mapDocumentState.openMap(mapPath, testDimensions));
    mapDocumentState.viewState().setTileSelection({.min = {.x = 1, .y = 1}, .maxExclusive = {.x = 3, .y = 3}},
                                                  mapDocumentState.changes().dimensions);

    const auto idReplace = mapDocumentState.replaceMasked(ReplaceMode::id, initialTile, {.id = 30, .z = 99});
    REQUIRE(idReplace);
    REQUIRE(idReplace->changedTiles == 4);
    REQUIRE(mapDocumentState.changes().reader->readTile({.x = 1, .y = 1}) == uomul::MapTile{.id = 30, .z = initialTile.z});
    REQUIRE(mapDocumentState.changes().reader->readTile({.x = 0, .y = 0}) == initialTile);

    const auto zReplace = mapDocumentState.replaceMasked(ReplaceMode::z, {.id = 30, .z = initialTile.z}, {.id = 40, .z = 22});
    REQUIRE(zReplace);
    REQUIRE(zReplace->changedTiles == 4);
    REQUIRE(mapDocumentState.changes().reader->readTile({.x = 1, .y = 1}) == uomul::MapTile{.id = 30, .z = 22});

    const auto combinedReplace = mapDocumentState.replaceMasked(ReplaceMode::idAndZ, {.id = 30, .z = 22}, {.id = 40, .z = 23});
    REQUIRE(combinedReplace);
    REQUIRE(combinedReplace->changedTiles == 4);
    REQUIRE(mapDocumentState.changes().reader->readTile({.x = 1, .y = 1}) == uomul::MapTile{.id = 40, .z = 23});
    REQUIRE(mapDocumentState.changes().reader->readTile({.x = 0, .y = 0}) == initialTile);
}

TEST_CASE("mapDocumentState_Should_ApplyMaskedBlockEditsOnlyToSelectedBlocks")
{
    const TestDirectory directory;
    const auto mapPath = directory.path() / "masked-blocks.mul";
    REQUIRE(uomul::MapEditor::createEmpty(mapPath, testDimensions, initialTile));

    MapDocumentState mapDocumentState;
    REQUIRE(mapDocumentState.openMap(mapPath, testDimensions));
    mapDocumentState.viewState().setBlockSelection({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 1, .y = 1}},
                                                   mapDocumentState.changes().dimensions);
    mapDocumentState.viewState().applyBlockSelection({.min = {.x = 1, .y = 1}, .maxExclusive = {.x = 2, .y = 2}},
                                                     uo_mule::gui::SelectionCombineMode::add,
                                                     mapDocumentState.changes().dimensions);

    const auto result = mapDocumentState.fillMasked({.id = 61, .z = 62}, FillMode::idAndZ);
    REQUIRE(result);
    REQUIRE(result->changedTiles == 128);
    REQUIRE(mapDocumentState.viewState().selectionMask.size() == 2);
    REQUIRE(mapDocumentState.changes().reader->readTile({.x = 7, .y = 7}) == uomul::MapTile{.id = 61, .z = 62});
    REQUIRE(mapDocumentState.changes().reader->readTile({.x = 8, .y = 8}) == uomul::MapTile{.id = 61, .z = 62});
    REQUIRE(mapDocumentState.changes().reader->readTile({.x = 8, .y = 7}) == initialTile);
}

TEST_CASE("mapDocumentState_Should_RejectMaskedEditsWithoutSelectionAndKeepCleanState")
{
    const TestDirectory directory;
    const auto mapPath = directory.path() / "masked-no-selection.mul";
    REQUIRE(uomul::MapEditor::createEmpty(mapPath, testDimensions, initialTile));

    MapDocumentState mapDocumentState;
    REQUIRE(mapDocumentState.openMap(mapPath, testDimensions));
    const auto result = mapDocumentState.fillMasked({.id = 11, .z = 12}, FillMode::idAndZ);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().message == "selection scope requires a selection");
    REQUIRE_FALSE(mapDocumentState.changes().dirty());
    REQUIRE(mapDocumentState.changes().reader->readTile({.x = 1, .y = 1}) == initialTile);
}

namespace
{

void prepareFuzzySelectionState(const std::filesystem::path & directory, MapDocumentState & mapDocumentState)
{
    const auto mapPath = directory / "fuzzy.mul";
    REQUIRE(uomul::MapEditor::createEmpty(mapPath, testDimensions, initialTile));
    auto editor = uomul::MapEditor::create(mapPath, directory / "fuzzy-staged.mul", testDimensions);
    REQUIRE(editor);
    if (!editor)
    {
        return;
    }
    REQUIRE(editor->editTile({.x = 1, .y = 1}, {.id = 20, .z = 5}));
    REQUIRE(editor->editTile({.x = 2, .y = 1}, {.id = 20, .z = 5}));
    REQUIRE(editor->editTile({.x = 1, .y = 2}, {.id = 20, .z = 5}));
    REQUIRE(editor->editTile({.x = 10, .y = 10}, {.id = 20, .z = 5}));
    REQUIRE(editor->commit());
    REQUIRE(mapDocumentState.openMap(directory / "fuzzy-staged.mul", testDimensions));
}

} // namespace

TEST_CASE("mapDocumentState_Should_FuzzySelectContiguousAndWholeMapWithAndPredicates")
{
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    const TestDirectory directory;
    MapDocumentState mapDocumentState;
    prepareFuzzySelectionState(directory.path(), mapDocumentState);
    uo_mule::gui::FuzzySelectionOptions options{
        .scope = uo_mule::gui::FuzzyScope::contiguous,
        .idEnabled = true,
        .id = 20,
        .zEnabled = true,
        .z = 5,
        .nameEnabled = true,
        .name = "  Granite   Floor ",
    };
    const auto names = [](std::uint16_t id) -> std::optional<std::string>
    { return id == 20 ? std::optional<std::string>{"granite floor"} : std::optional<std::string>{"grass"}; };

    REQUIRE(mapDocumentState.fuzzySelect(options, {.x = 1, .y = 1}, names) == 3);
    REQUIRE(mapDocumentState.viewState().selectionMask.size() == 3);
    REQUIRE(mapDocumentState.viewState().selectionMask.contains(uomul::TileCoord{.x = 1, .y = 1}));
    REQUIRE_FALSE(mapDocumentState.viewState().selectionMask.contains(uomul::TileCoord{.x = 10, .y = 10}));

    options.scope = uo_mule::gui::FuzzyScope::wholeMap;
    REQUIRE(mapDocumentState.fuzzySelect(options, {.x = 1, .y = 1}, names) == 4);
    REQUIRE(mapDocumentState.viewState().selectionMask.size() == 4);
    REQUIRE(mapDocumentState.viewState().selectionMask.contains(uomul::TileCoord{.x = 10, .y = 10}));
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
}

TEST_CASE("mapDocumentState_Should_RejectUnavailableFuzzyNameAndBlockMode")
{
    const TestDirectory directory;
    const auto mapPath = directory.path() / "fuzzy-rejected.mul";
    REQUIRE(uomul::MapEditor::createEmpty(mapPath, testDimensions, initialTile));
    MapDocumentState mapDocumentState;
    REQUIRE(mapDocumentState.openMap(mapPath, testDimensions));
    const uo_mule::gui::FuzzySelectionOptions nameOptions{
        .nameEnabled = true,
        .name = "grass",
    };
    REQUIRE_FALSE(mapDocumentState.fuzzySelect(nameOptions, {.x = 0, .y = 0}));
    REQUIRE(mapDocumentState.changes().error);
    REQUIRE(mapDocumentState.changes().error.value_or(uomul::Error{}).message == "Tiledata name matching is unavailable");

    mapDocumentState.viewState().setBlockSelection({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 1, .y = 1}},
                                                   mapDocumentState.changes().dimensions);
    const uo_mule::gui::FuzzySelectionOptions idOptions{
        .idEnabled = true,
        .id = initialTile.id,
        .name = {},
    };
    REQUIRE_FALSE(mapDocumentState.fuzzySelect(idOptions, {.x = 0, .y = 0}));
    REQUIRE(mapDocumentState.changes().error);
    REQUIRE(mapDocumentState.changes().error.value_or(uomul::Error{}).message == "Fuzzy Select supports tiles only");
}

TEST_CASE("mapDocumentManager_Should_PreserveIndependentMapAndCanvasState")
{
    // These values describe the persisted view state used by this round-trip test.
    constexpr double expectedPan = 12.0;
    constexpr double expectedZoom = 18.0;
    constexpr double comparisonTolerance = 0.001;

    const TestDirectory directory;
    const auto firstPath = directory.path() / "first.mul";
    const auto secondPath = directory.path() / "second.mul";
    REQUIRE(uomul::MapEditor::createEmpty(firstPath, testDimensions, initialTile));
    REQUIRE(uomul::MapEditor::createEmpty(secondPath, testDimensions));

    MapDocumentManager manager;
    REQUIRE(manager.openMap(firstPath, testDimensions));
    manager.bindActiveDocument(1U);
    manager.active().viewState().displayMode = uo_mule::gui::MapDisplayMode::combined;
    manager.active().viewState().setTileSelection({.min = {.x = 1, .y = 2}, .maxExclusive = {.x = 3, .y = 4}},
                                                  manager.active().changes().dimensions);
    manager.canvasState().panX = expectedPan;
    manager.canvasState().zoom = expectedZoom;

    REQUIRE(manager.openMap(secondPath, testDimensions));
    manager.bindActiveDocument(2U);
    REQUIRE(manager.activateMap(1U, firstPath, testDimensions));
    REQUIRE(manager.fill({.id = selectionReplacementTileId, .z = filledElevation}, MapEditScope::selection));
    REQUIRE(manager.activateMap(2U, secondPath, testDimensions));
    REQUIRE(manager.activateMap(1U, firstPath, testDimensions));

    REQUIRE(manager.active().changes().originalPath == firstPath);
    REQUIRE(manager.active().changes().dirty());
    REQUIRE(manager.active().viewState().displayMode == uo_mule::gui::MapDisplayMode::combined);
    REQUIRE(manager.active().viewState().tileSelection.has_value());
    REQUIRE(std::abs(manager.canvasState().panX - expectedPan) < comparisonTolerance);
    REQUIRE(std::abs(manager.canvasState().zoom - expectedZoom) < comparisonTolerance);
}

TEST_CASE("mapDocumentManager_Should_InvalidateAllResidentCanvasCaches")
{
    const TestDirectory directory;
    const auto mapPath = directory.path() / "first.mul";
    REQUIRE(uomul::MapEditor::createEmpty(mapPath, testDimensions));
    MapDocumentManager manager;
    manager.canvasState().lodCacheValid = true;
    REQUIRE(manager.openMap(mapPath, testDimensions));
    manager.bindActiveDocument(1U);
    manager.canvasState().lodCacheValid = true;
    manager.canvasState().blockCache.push_back(
        {.coordinate = {.x = 1, .y = 1}, .block = std::nullopt, .error = std::nullopt, .lastUsed = 1});
    manager.canvasState().blockCacheIndex.emplace(
        0x0000000100000001ULL, 0U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    manager.canvasState().cacheClock = 2;
    manager.invalidateProviderCaches();

    REQUIRE_FALSE(manager.canvasState().lodCacheValid);
    REQUIRE(manager.canvasState().blockCache.empty());
    REQUIRE(manager.canvasState().blockCacheIndex.empty());
    REQUIRE(manager.canvasState().cacheClock == 0);
}

TEST_CASE("mapDocumentManager_Should_DeactivateMapWithoutDiscardingResidentState")
{
    // These values describe the resident view state restored after deactivation.
    constexpr double expectedPan = 24.0;
    constexpr double comparisonTolerance = 0.001;

    const TestDirectory directory;
    const auto mapPath = directory.path() / "resident.mul";
    REQUIRE(uomul::MapEditor::createEmpty(mapPath, testDimensions, initialTile));

    MapDocumentManager manager;
    REQUIRE(manager.openMap(mapPath, testDimensions));
    manager.bindActiveDocument(7U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    manager.canvasState().panX = expectedPan;
    REQUIRE(manager.updateSelectedTile({.x = 2, .y = 3}));

    manager.deactivateMap();

    REQUIRE_FALSE(manager.activeDocumentId());
    REQUIRE(!manager.active().changes().reader.has_value());
    REQUIRE(manager.activateMap(7U, mapPath, testDimensions));
    REQUIRE(manager.activeDocumentId() == 7U);
    REQUIRE(hasExpectedResidentState(manager));
    REQUIRE(std::abs(manager.canvasState().panX - expectedPan) < comparisonTolerance);
}
