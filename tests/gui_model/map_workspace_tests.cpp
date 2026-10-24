#include "gui_model/tools/tool_contracts.h"
#include "gui_model/workspaces/map_workspace.h"
#include "gui_model/workspaces/map_workspace_manager.h"
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
using uo_mule::gui::MapEditScope;
using uo_mule::gui::MapSelectionMode;
using uo_mule::gui::MapWorkspace;
using uo_mule::gui::MapWorkspaceManager;
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
        path_ = std::filesystem::temp_directory_path() / ("map-workspace-tests-" + std::to_string(sequence));
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

bool hasExpectedResidentState(const MapWorkspaceManager & manager)
{
    const auto selectedTile = manager.selectedTile();
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

TEST_CASE("mapWorkspace_Should_OpenCreateHoverAndExposeSelectionState")
{
    const TestDirectory directory;
    const auto mapPath = directory.path() / "created.mul";
    MapWorkspace workspace;

    REQUIRE(workspace.createEmptyMap(mapPath, testDimensions, PendingChangesAction::cancel, std::nullopt, false, initialTile));
    REQUIRE(workspace.reader() != nullptr);
    REQUIRE(workspace.mapPath() == mapPath);
    REQUIRE(workspace.dimensions() == testDimensions);
    REQUIRE(workspace.reader()->readTile({.x = 8, .y = 9}) == initialTile);
    const uomul::TileCoord hoveredCoordinate{.x = 8, .y = 9};
    REQUIRE(workspace.updateHover(hoveredCoordinate));
    REQUIRE(workspace.hoveredTile().has_value());
    const auto hoveredTile = workspace.hoveredTile().value_or(uomul::TileCoord{});
    REQUIRE(hoveredTile.x == hoveredCoordinate.x);
    REQUIRE(hoveredTile.y == hoveredCoordinate.y);
    REQUIRE(workspace.hoveredValue() == initialTile);
    workspace.clearHover();
    REQUIRE_FALSE(workspace.hoveredTile());
    REQUIRE_FALSE(workspace.selectedTile());
    const uomul::TileCoord selectedCoordinate{.x = 3, .y = 4};
    REQUIRE(workspace.updateSelectedTile(selectedCoordinate));
    REQUIRE(workspace.selectedTile().has_value());
    const auto selectedTile = workspace.selectedTile().value_or(uomul::TileCoord{});
    REQUIRE(selectedTile.x == selectedCoordinate.x);
    REQUIRE(selectedTile.y == selectedCoordinate.y);
    REQUIRE(workspace.selectedValue() == initialTile);
    workspace.clearHover();
    REQUIRE_FALSE(workspace.hoveredTile());
    REQUIRE(workspace.selectedTile().has_value());
    const auto remainingSelectedTile = workspace.selectedTile().value_or(uomul::TileCoord{});
    REQUIRE(remainingSelectedTile.x == selectedCoordinate.x);
    REQUIRE(remainingSelectedTile.y == selectedCoordinate.y);

    workspace.setTileSelection({.min = {.x = 2, .y = 3}, .maxExclusive = {.x = 4, .y = tileSelectionMaxY}});
    REQUIRE(workspace.selectionMode() == MapSelectionMode::tiles);
    const auto tileStats = workspace.selectionStats();
    REQUIRE(tileStats.has_value());
    REQUIRE(tileStats.has_value());
    const auto availableTileStats = tileStats.value_or(std::nullopt);
    REQUIRE(availableTileStats.has_value());
    REQUIRE(availableTileStats.value_or(uomul::MapStats{}).tileCount == selectedTileCount);

    workspace.setSelectionMode(MapSelectionMode::blocks);
    REQUIRE_FALSE(workspace.tileSelection());
    workspace.setBlockSelection({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 1, .y = 1}});
    const auto blockStats = workspace.selectionStats();
    REQUIRE(blockStats.has_value());
    REQUIRE(blockStats.has_value());
    const auto availableBlockStats = blockStats.value_or(std::nullopt);
    REQUIRE(availableBlockStats.has_value());
    REQUIRE(availableBlockStats.value_or(uomul::MapStats{}).tileCount == uomul::mapBlockTileCount);

    REQUIRE_FALSE(workspace.updateHover({.x = 16, .y = 0}));
    REQUIRE_FALSE(workspace.hoveredTile());
    REQUIRE_FALSE(workspace.hoveredValue());
    REQUIRE(workspace.error());
    workspace.clearHover();
    REQUIRE_FALSE(workspace.hoveredTile());
}

TEST_CASE("mapWorkspace_Should_ApplyLassoSelectionWithCombineModes")
{
    const TestDirectory directory;
    const auto mapPath = directory.path() / "lasso.mul";
    MapWorkspace workspace;
    REQUIRE(workspace.createEmptyMap(mapPath, testDimensions));

    const std::vector<uomul::TileCoord> firstLasso{
        {.x = 1, .y = 1},
        {.x = 4, .y = 1},
        {.x = 4, .y = 4},
        {.x = 1, .y = 4},
    };
    workspace.applyLassoSelection(firstLasso, uo_mule::gui::SelectionCombineMode::replace);
    REQUIRE(workspace.selectionMask().size() == 9);

    const std::vector<uomul::TileCoord> secondLasso{
        {.x = 3, .y = 3},
        {.x = 5, .y = 3},
        {.x = 5, .y = 5},
        {.x = 3, .y = 5},
    };
    workspace.applyLassoSelection(secondLasso, uo_mule::gui::SelectionCombineMode::add);
    REQUIRE(workspace.selectionMask().size() == 12);
    workspace.applyLassoSelection(secondLasso, uo_mule::gui::SelectionCombineMode::subtract);
    REQUIRE(workspace.selectionMask().size() == 8);
}

TEST_CASE("mapWorkspace_Should_ConvertBlockSelectionBackToClippedTiles")
{
    const TestDirectory directory;
    const auto mapPath = directory.path() / "selection-conversion.mul";
    REQUIRE(uomul::MapEditor::createEmpty(mapPath, {.widthTiles = 16, .heightTiles = 16}));

    MapWorkspace workspace;
    REQUIRE(workspace.openMap(mapPath, {.widthTiles = 16, .heightTiles = 16}));
    workspace.setBlockSelection({.min = {.x = 1, .y = 1}, .maxExclusive = {.x = 2, .y = 2}});
    workspace.setSelectionMode(MapSelectionMode::tiles);

    REQUIRE(workspace.selectionMode() == MapSelectionMode::tiles);
    REQUIRE(workspace.selectionMask().size() == 64);
    REQUIRE(workspace.selectionMask().contains(uomul::TileCoord{.x = 8, .y = 8}));
    REQUIRE(workspace.selectionMask().contains(uomul::TileCoord{.x = 15, .y = 15}));
}

TEST_CASE("mapWorkspace_Should_ExposeSparseSelectionBoundsAndStats")
{
    const TestDirectory directory;
    const auto mapPath = directory.path() / "selection-bounds.mul";
    REQUIRE(uomul::MapEditor::createEmpty(mapPath, testDimensions, initialTile));
    MapWorkspace workspace;
    REQUIRE(workspace.openMap(mapPath, testDimensions));
    workspace.applyLassoSelection({{.x = 1, .y = 1}, {.x = 4, .y = 1}, {.x = 4, .y = 4}, {.x = 1, .y = 4}},
                                  uo_mule::gui::SelectionCombineMode::replace);

    const auto bounds = workspace.selectionBounds();
    REQUIRE(bounds.has_value());
    const auto boundsValue = bounds.value_or(uomul::TileRegion{});
    REQUIRE(boundsValue.min.x == 1);
    REQUIRE(boundsValue.min.y == 1);
    REQUIRE(boundsValue.maxExclusive.x == 4);
    REQUIRE(boundsValue.maxExclusive.y == 4);
    const auto stats = workspace.selectionStats();
    REQUIRE(stats.has_value());
    REQUIRE(stats->has_value());
    const auto statsValue = stats.value().value_or(uomul::MapStats{});
    REQUIRE(statsValue.tileCount == 9);
}

TEST_CASE("mapWorkspace_Should_StageEditsAndRequireExplicitDirtyResolution")
{
    const TestDirectory directory;
    const auto originalPath = directory.path() / "original.mul";
    const auto otherPath = directory.path() / "other.mul";
    const auto savedPath = directory.path() / "saved.mul";
    REQUIRE(uomul::MapEditor::createEmpty(originalPath, testDimensions, initialTile));
    REQUIRE(uomul::MapEditor::createEmpty(otherPath, testDimensions));

    MapWorkspace workspace;
    REQUIRE(workspace.openMap(originalPath, testDimensions));
    workspace.setTileSelection({.min = {.x = 1, .y = 2}, .maxExclusive = {.x = 3, .y = 4}});
    REQUIRE(workspace.fill({.id = selectionReplacementTileId, .z = filledElevation}, MapEditScope::selection));
    REQUIRE(workspace.dirty());
    REQUIRE_FALSE(workspace.openMap(otherPath, testDimensions));
    REQUIRE(workspace.dirty());
    REQUIRE(workspace.workingPath().has_value());
    const auto & stagedPath = workspace.workingPath();
    REQUIRE(workspace.mapPath() == stagedPath.value_or(std::filesystem::path{}));
    REQUIRE(workspace.reader()->readTile({.x = 1, .y = 2}) ==
            uomul::MapTile{.id = selectionReplacementTileId, .z = filledElevation});
    REQUIRE(workspace.reader()->readTile({.x = 0, .y = 0}) == initialTile);

    REQUIRE_FALSE(workspace.openMap(otherPath, testDimensions, PendingChangesAction::cancel));
    REQUIRE(workspace.openMap(otherPath, testDimensions, PendingChangesAction::discard));
    REQUIRE_FALSE(workspace.dirty());
    REQUIRE(workspace.mapPath() == otherPath);

    REQUIRE(workspace.openMap(originalPath, testDimensions));
    REQUIRE(workspace.replaceTileId(initialTile.id, 23, MapEditScope::wholeMap));
    REQUIRE(workspace.reader()->readTile({.x = 0, .y = 0}) ==
            uomul::MapTile{.id = selectedReplacementTileId, .z = initialTile.z});
    REQUIRE(workspace.replaceZ(initialTile.z, 12, MapEditScope::wholeMap));
    REQUIRE(workspace.reader()->readTile({.x = 0, .y = 0}) == uomul::MapTile{.id = selectedReplacementTileId, .z = 12});
    REQUIRE(workspace.save(savedPath));
    REQUIRE_FALSE(workspace.dirty());
    REQUIRE(workspace.mapPath() == savedPath);
    const auto savedReader = uomul::MapReader::open(savedPath, testDimensions);
    REQUIRE(savedReader);
    REQUIRE(savedReader->readTile({.x = 0, .y = 0}) == uomul::MapTile{.id = selectedReplacementTileId, .z = 12});
}

TEST_CASE("mapWorkspace_Should_RejectSelectionEditsWithoutSelectionAndDiscardBackToSource")
{
    const TestDirectory directory;
    const auto sourcePath = directory.path() / "source.mul";
    REQUIRE(uomul::MapEditor::createEmpty(sourcePath, testDimensions, initialTile));
    MapWorkspace workspace;
    REQUIRE(workspace.openMap(sourcePath, testDimensions));
    REQUIRE_FALSE(workspace.replaceTileId(initialTile.id, rejectedReplacementTileId, MapEditScope::selection));
    REQUIRE_FALSE(workspace.dirty());
    workspace.setBlockSelection({.min = {.x = 1, .y = 1}, .maxExclusive = {.x = 2, .y = 2}});
    REQUIRE(workspace.replaceZ(initialTile.z, blockElevation, MapEditScope::selection));
    REQUIRE(workspace.reader()->readTile({.x = 8, .y = 8}) == uomul::MapTile{.id = initialTile.id, .z = blockElevation});
    REQUIRE(workspace.workingPath().has_value());
    const auto workingPath = workspace.workingPath().value_or(std::filesystem::path{});
    REQUIRE(workspace.discard());
    REQUIRE_FALSE(workspace.dirty());
    REQUIRE_FALSE(std::filesystem::exists(workingPath));
    REQUIRE(workspace.reader()->readTile({.x = 8, .y = 8}) == initialTile);
    REQUIRE(workspace.closeMap());
    REQUIRE(workspace.reader() == nullptr);
    REQUIRE(workspace.mapPath().empty());
}

TEST_CASE("mapWorkspace_Should_ApplyMaskedFillModesAndPreserveSparseTileSelection")
{
    const TestDirectory directory;
    const auto mapPath = directory.path() / "masked-fill.mul";
    REQUIRE(uomul::MapEditor::createEmpty(mapPath, testDimensions, initialTile));

    MapWorkspace workspace;
    REQUIRE(workspace.openMap(mapPath, testDimensions));
    workspace.setTileSelection({.min = {.x = 1, .y = 1}, .maxExclusive = {.x = 3, .y = 3}});
    workspace.applyTileSelection({.min = {.x = 2, .y = 2}, .maxExclusive = {.x = 3, .y = 3}},
                                 uo_mule::gui::SelectionCombineMode::subtract);
    REQUIRE(workspace.selectionMask().size() == 3);

    const auto idFill = workspace.fillMasked({.id = 90, .z = 50}, FillMode::id);
    REQUIRE(idFill);
    REQUIRE(idFill->changedTiles == 3);
    REQUIRE(workspace.selectionMask().size() == 3);
    REQUIRE(workspace.selectionMask().contains(uomul::TileCoord{.x = 1, .y = 1}));
    REQUIRE(workspace.reader()->readTile({.x = 1, .y = 1}) == uomul::MapTile{.id = 90, .z = initialTile.z});
    REQUIRE(workspace.reader()->readTile({.x = 2, .y = 2}) == initialTile);

    const auto zFill = workspace.fillMasked({.id = 80, .z = 51}, FillMode::z);
    REQUIRE(zFill);
    REQUIRE(zFill->changedTiles == 3);
    REQUIRE(workspace.reader()->readTile({.x = 1, .y = 1}) == uomul::MapTile{.id = 90, .z = 51});

    const auto combinedFill = workspace.fillMasked({.id = 70, .z = 52}, FillMode::idAndZ);
    REQUIRE(combinedFill);
    REQUIRE(combinedFill->changedTiles == 3);
    REQUIRE(workspace.reader()->readTile({.x = 1, .y = 1}) == uomul::MapTile{.id = 70, .z = 52});
    REQUIRE(workspace.reader()->readTile({.x = 0, .y = 0}) == initialTile);
    REQUIRE(workspace.dirty());
}

TEST_CASE("mapWorkspace_Should_ApplyMaskedReplaceModesToSparseTiles")
{
    const TestDirectory directory;
    const auto mapPath = directory.path() / "masked-replace.mul";
    REQUIRE(uomul::MapEditor::createEmpty(mapPath, testDimensions, initialTile));

    MapWorkspace workspace;
    REQUIRE(workspace.openMap(mapPath, testDimensions));
    workspace.setTileSelection({.min = {.x = 1, .y = 1}, .maxExclusive = {.x = 3, .y = 3}});

    const auto idReplace = workspace.replaceMasked(ReplaceMode::id, initialTile, {.id = 30, .z = 99});
    REQUIRE(idReplace);
    REQUIRE(idReplace->changedTiles == 4);
    REQUIRE(workspace.reader()->readTile({.x = 1, .y = 1}) == uomul::MapTile{.id = 30, .z = initialTile.z});
    REQUIRE(workspace.reader()->readTile({.x = 0, .y = 0}) == initialTile);

    const auto zReplace = workspace.replaceMasked(ReplaceMode::z, {.id = 30, .z = initialTile.z}, {.id = 40, .z = 22});
    REQUIRE(zReplace);
    REQUIRE(zReplace->changedTiles == 4);
    REQUIRE(workspace.reader()->readTile({.x = 1, .y = 1}) == uomul::MapTile{.id = 30, .z = 22});

    const auto combinedReplace = workspace.replaceMasked(ReplaceMode::idAndZ, {.id = 30, .z = 22}, {.id = 40, .z = 23});
    REQUIRE(combinedReplace);
    REQUIRE(combinedReplace->changedTiles == 4);
    REQUIRE(workspace.reader()->readTile({.x = 1, .y = 1}) == uomul::MapTile{.id = 40, .z = 23});
    REQUIRE(workspace.reader()->readTile({.x = 0, .y = 0}) == initialTile);
}

TEST_CASE("mapWorkspace_Should_ApplyMaskedBlockEditsOnlyToSelectedBlocks")
{
    const TestDirectory directory;
    const auto mapPath = directory.path() / "masked-blocks.mul";
    REQUIRE(uomul::MapEditor::createEmpty(mapPath, testDimensions, initialTile));

    MapWorkspace workspace;
    REQUIRE(workspace.openMap(mapPath, testDimensions));
    workspace.setBlockSelection({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 1, .y = 1}});
    workspace.applyBlockSelection({.min = {.x = 1, .y = 1}, .maxExclusive = {.x = 2, .y = 2}},
                                  uo_mule::gui::SelectionCombineMode::add);

    const auto result = workspace.fillMasked({.id = 61, .z = 62}, FillMode::idAndZ);
    REQUIRE(result);
    REQUIRE(result->changedTiles == 128);
    REQUIRE(workspace.selectionMask().size() == 2);
    REQUIRE(workspace.reader()->readTile({.x = 7, .y = 7}) == uomul::MapTile{.id = 61, .z = 62});
    REQUIRE(workspace.reader()->readTile({.x = 8, .y = 8}) == uomul::MapTile{.id = 61, .z = 62});
    REQUIRE(workspace.reader()->readTile({.x = 8, .y = 7}) == initialTile);
}

TEST_CASE("mapWorkspace_Should_RejectMaskedEditsWithoutSelectionAndKeepCleanState")
{
    const TestDirectory directory;
    const auto mapPath = directory.path() / "masked-no-selection.mul";
    REQUIRE(uomul::MapEditor::createEmpty(mapPath, testDimensions, initialTile));

    MapWorkspace workspace;
    REQUIRE(workspace.openMap(mapPath, testDimensions));
    const auto result = workspace.fillMasked({.id = 11, .z = 12}, FillMode::idAndZ);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().message == "selection scope requires a selection");
    REQUIRE_FALSE(workspace.dirty());
    REQUIRE(workspace.reader()->readTile({.x = 1, .y = 1}) == initialTile);
}

namespace
{

void prepareFuzzySelectionWorkspace(const std::filesystem::path & directory, MapWorkspace & workspace)
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
    REQUIRE(workspace.openMap(directory / "fuzzy-staged.mul", testDimensions));
}

} // namespace

TEST_CASE("mapWorkspace_ShouldFuzzySelectContiguousAndWholeMapWithAndPredicates")
{
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    const TestDirectory directory;
    MapWorkspace workspace;
    prepareFuzzySelectionWorkspace(directory.path(), workspace);
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

    REQUIRE(workspace.fuzzySelect(options, {.x = 1, .y = 1}, names) == 3);
    REQUIRE(workspace.selectionMask().size() == 3);
    REQUIRE(workspace.selectionMask().contains(uomul::TileCoord{.x = 1, .y = 1}));
    REQUIRE_FALSE(workspace.selectionMask().contains(uomul::TileCoord{.x = 10, .y = 10}));

    options.scope = uo_mule::gui::FuzzyScope::wholeMap;
    REQUIRE(workspace.fuzzySelect(options, {.x = 1, .y = 1}, names) == 4);
    REQUIRE(workspace.selectionMask().size() == 4);
    REQUIRE(workspace.selectionMask().contains(uomul::TileCoord{.x = 10, .y = 10}));
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
}

TEST_CASE("mapWorkspace_ShouldRejectUnavailableFuzzyNameAndBlockMode")
{
    const TestDirectory directory;
    const auto mapPath = directory.path() / "fuzzy-rejected.mul";
    REQUIRE(uomul::MapEditor::createEmpty(mapPath, testDimensions, initialTile));
    MapWorkspace workspace;
    REQUIRE(workspace.openMap(mapPath, testDimensions));
    const uo_mule::gui::FuzzySelectionOptions nameOptions{
        .nameEnabled = true,
        .name = "grass",
    };
    REQUIRE_FALSE(workspace.fuzzySelect(nameOptions, {.x = 0, .y = 0}));
    REQUIRE(workspace.error());
    REQUIRE(workspace.error().value_or(uomul::Error{}).message == "Tiledata name matching is unavailable");

    workspace.setBlockSelection({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 1, .y = 1}});
    const uo_mule::gui::FuzzySelectionOptions idOptions{
        .idEnabled = true,
        .id = initialTile.id,
        .name = {},
    };
    REQUIRE_FALSE(workspace.fuzzySelect(idOptions, {.x = 0, .y = 0}));
    REQUIRE(workspace.error());
    REQUIRE(workspace.error().value_or(uomul::Error{}).message == "Fuzzy Select supports tiles only");
}

TEST_CASE("mapWorkspaceManager_Should_PreserveIndependentMapAndCanvasState")
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

    MapWorkspaceManager manager;
    REQUIRE(manager.openMap(firstPath, testDimensions));
    manager.bindActiveDocument(1U);
    manager.setDisplayMode(uo_mule::gui::MapDisplayMode::combined);
    manager.setTileSelection({.min = {.x = 1, .y = 2}, .maxExclusive = {.x = 3, .y = 4}});
    manager.canvasState().panX = expectedPan;
    manager.canvasState().zoom = expectedZoom;

    REQUIRE(manager.openMap(secondPath, testDimensions));
    manager.bindActiveDocument(2U);
    REQUIRE(manager.activateMap(1U, firstPath, testDimensions));
    REQUIRE(manager.fill({.id = selectionReplacementTileId, .z = filledElevation}, MapEditScope::selection));
    REQUIRE(manager.activateMap(2U, secondPath, testDimensions));
    REQUIRE(manager.activateMap(1U, firstPath, testDimensions));

    REQUIRE(manager.originalPath() == firstPath);
    REQUIRE(manager.dirty());
    REQUIRE(manager.displayMode() == uo_mule::gui::MapDisplayMode::combined);
    REQUIRE(manager.tileSelection().has_value());
    REQUIRE(std::abs(manager.canvasState().panX - expectedPan) < comparisonTolerance);
    REQUIRE(std::abs(manager.canvasState().zoom - expectedZoom) < comparisonTolerance);
}

TEST_CASE("mapWorkspaceManager_Should_InvalidateAllResidentCanvasCaches")
{
    const TestDirectory directory;
    const auto mapPath = directory.path() / "first.mul";
    REQUIRE(uomul::MapEditor::createEmpty(mapPath, testDimensions));
    MapWorkspaceManager manager;
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

TEST_CASE("mapWorkspaceManager_Should_DeactivateMapWithoutDiscardingResidentState")
{
    // These values describe the resident view state restored after deactivation.
    constexpr double expectedPan = 24.0;
    constexpr double comparisonTolerance = 0.001;

    const TestDirectory directory;
    const auto mapPath = directory.path() / "resident.mul";
    REQUIRE(uomul::MapEditor::createEmpty(mapPath, testDimensions, initialTile));

    MapWorkspaceManager manager;
    REQUIRE(manager.openMap(mapPath, testDimensions));
    manager.bindActiveDocument(7U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    manager.canvasState().panX = expectedPan;
    REQUIRE(manager.updateSelectedTile({.x = 2, .y = 3}));

    manager.deactivateMap();

    REQUIRE_FALSE(manager.activeDocumentId());
    REQUIRE(manager.reader() == nullptr);
    REQUIRE(manager.activateMap(7U, mapPath, testDimensions));
    REQUIRE(manager.activeDocumentId() == 7U);
    REQUIRE(hasExpectedResidentState(manager));
    REQUIRE(std::abs(manager.canvasState().panX - expectedPan) < comparisonTolerance);
}
