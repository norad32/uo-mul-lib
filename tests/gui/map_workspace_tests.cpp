#include "gui/map_workspace.h"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>

namespace
{

using im_cpp::gui::MapEditScope;
using im_cpp::gui::MapSelectionMode;
using im_cpp::gui::MapWorkspace;
using im_cpp::gui::PendingChangesAction;

constexpr uomul::Dimensions testDimensions{.widthTiles = 16, .heightTiles = 16};
constexpr uomul::MapTile initialTile{.id = 17, .z = -4};
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

    ~TestDirectory()
    {
        std::error_code ignoredError;
        std::filesystem::remove_all(path_, ignoredError);
    }

    [[nodiscard]] const std::filesystem::path & path() const { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("mapCoordinateHelpers_Should_ConvertScreenAndTileCoordinates")
{
    const auto block = im_cpp::gui::tileToBlock({.x = 15, .y = 8});
    REQUIRE(block.x == 1);
    REQUIRE(block.y == 1);
    const auto tile = im_cpp::gui::screenToTile(35.0, 55.0, 10.0, 20.0, 5.0, 5.0, 10.0, testDimensions);
    REQUIRE(tile);
    REQUIRE(tile->x == 2);
    REQUIRE(tile->y == 3);
    REQUIRE_FALSE(im_cpp::gui::screenToTile(9.0, 25.0, 10.0, 20.0, 0.0, 0.0, 10.0, testDimensions));
    REQUIRE_FALSE(im_cpp::gui::screenToTile(170.0, 20.0, 10.0, 20.0, 0.0, 0.0, 10.0, testDimensions));
    REQUIRE_FALSE(im_cpp::gui::screenToTile(10.0, 20.0, 10.0, 20.0, 0.0, 0.0, 0.0, testDimensions));
}

TEST_CASE("mapSelectionHelpers_Should_CreateHalfOpenSingleAndReverseDragRegions")
{
    const auto singleTile = im_cpp::gui::tileSelectionFromDrag({.x = 4, .y = 5}, {.x = 4, .y = 5}, testDimensions);
    REQUIRE(singleTile.min.x == 4);
    REQUIRE(singleTile.min.y == 5);
    REQUIRE(singleTile.maxExclusive.x == 5);
    REQUIRE(singleTile.maxExclusive.y == 6);
    const auto reverseDrag = im_cpp::gui::tileSelectionFromDrag({.x = 9, .y = 12}, {.x = 3, .y = 5}, testDimensions);
    REQUIRE(reverseDrag.min.x == 3);
    REQUIRE(reverseDrag.min.y == 5);
    REQUIRE(reverseDrag.maxExclusive.x == 10);
    REQUIRE(reverseDrag.maxExclusive.y == 13);
    const auto edgeSelection = im_cpp::gui::tileSelectionFromDrag(
        {.x = 15, .y = 15}, {.x = std::numeric_limits<std::uint32_t>::max(), .y = 100}, testDimensions);
    REQUIRE(edgeSelection.min.x == 15);
    REQUIRE(edgeSelection.min.y == 15);
    REQUIRE(edgeSelection.maxExclusive.x == 16);
    REQUIRE(edgeSelection.maxExclusive.y == 16);
    const auto blocks = im_cpp::gui::blockSelectionFromDrag({.x = 7, .y = 8}, {.x = 8, .y = 15}, testDimensions);
    REQUIRE(blocks.min.x == 0);
    REQUIRE(blocks.min.y == 1);
    REQUIRE(blocks.maxExclusive.x == 2);
    REQUIRE(blocks.maxExclusive.y == 2);
    const auto clamped = im_cpp::gui::clampBlockRegion({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 9, .y = 9}},
                                                       {.widthTiles = 24, .heightTiles = 9});
    REQUIRE(clamped.min.x == 0);
    REQUIRE(clamped.min.y == 0);
    REQUIRE(clamped.maxExclusive.x == 3);
    REQUIRE(clamped.maxExclusive.y == 2);
}

TEST_CASE("mapColorHelpers_Should_BeDeterministicAndMapSignedElevationExtrema")
{
    REQUIRE(im_cpp::gui::tileIdColor(1234) == im_cpp::gui::tileIdColor(1234));
    REQUIRE_FALSE(im_cpp::gui::tileIdColor(1234) == im_cpp::gui::tileIdColor(1235));
    REQUIRE(im_cpp::gui::zGrayscale(std::numeric_limits<std::int8_t>::min()) == 0);
    REQUIRE(im_cpp::gui::zGrayscale(0) == 128);
    REQUIRE(im_cpp::gui::zGrayscale(std::numeric_limits<std::int8_t>::max()) == 255);
    const uomul::MapTile tile{.id = 1234, .z = -12};
    REQUIRE(im_cpp::gui::tileDisplayColor(im_cpp::gui::MapDisplayMode::tileIds, tile) == im_cpp::gui::tileIdColor(tile.id));
    const auto zColor = im_cpp::gui::tileDisplayColor(im_cpp::gui::MapDisplayMode::z, tile);
    REQUIRE(zColor.red == im_cpp::gui::zGrayscale(tile.z));
    REQUIRE(zColor.green == zColor.red);
    REQUIRE(zColor.blue == zColor.red);
    REQUIRE_FALSE(im_cpp::gui::tileDisplayColor(im_cpp::gui::MapDisplayMode::combined, tile) ==
                  im_cpp::gui::tileIdColor(tile.id));
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
    REQUIRE(workspace.updateHover({.x = 8, .y = 9}));
    REQUIRE(workspace.hoveredTile());
    REQUIRE(workspace.hoveredTile()->x == 8);
    REQUIRE(workspace.hoveredTile()->y == 9);
    REQUIRE(workspace.hoveredValue() == initialTile);
    REQUIRE(workspace.updateSelectedTile({.x = 3, .y = 4}));
    REQUIRE(workspace.selectedTile());
    REQUIRE(workspace.selectedTile()->x == 3);
    REQUIRE(workspace.selectedTile()->y == 4);
    REQUIRE(workspace.selectedValue() == initialTile);
    workspace.clearHover();
    REQUIRE_FALSE(workspace.hoveredTile());
    REQUIRE(workspace.selectedTile());

    workspace.setTileSelection({.min = {.x = 2, .y = 3}, .maxExclusive = {.x = 4, .y = 5}});
    REQUIRE(workspace.selectionMode() == MapSelectionMode::tiles);
    const auto tileStats = workspace.selectionStats();
    REQUIRE(tileStats);
    REQUIRE(tileStats->has_value());
    REQUIRE(tileStats->value().tileCount == 4);

    workspace.setSelectionMode(MapSelectionMode::blocks);
    REQUIRE_FALSE(workspace.tileSelection());
    workspace.setBlockSelection({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 1, .y = 1}});
    const auto blockStats = workspace.selectionStats();
    REQUIRE(blockStats);
    REQUIRE(blockStats->has_value());
    REQUIRE(blockStats->value().tileCount == uomul::mapBlockTileCount);

    REQUIRE_FALSE(workspace.updateHover({.x = 16, .y = 0}));
    REQUIRE_FALSE(workspace.hoveredTile());
    REQUIRE_FALSE(workspace.hoveredValue());
    REQUIRE(workspace.error());
    workspace.clearHover();
    REQUIRE_FALSE(workspace.hoveredTile());
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
    REQUIRE_FALSE(workspace.fill({.id = 99, .z = 2}, MapEditScope::wholeMap));
    REQUIRE_FALSE(workspace.dirty());
    workspace.setEditMode(true);
    workspace.setTileSelection({.min = {.x = 1, .y = 2}, .maxExclusive = {.x = 3, .y = 4}});
    REQUIRE(workspace.fill({.id = 99, .z = 2}, MapEditScope::selection));
    REQUIRE(workspace.dirty());
    REQUIRE(workspace.workingPath());
    REQUIRE(workspace.mapPath() == *workspace.workingPath());
    REQUIRE(workspace.reader()->readTile({.x = 1, .y = 2}) == uomul::MapTile{.id = 99, .z = 2});
    REQUIRE(workspace.reader()->readTile({.x = 0, .y = 0}) == initialTile);

    REQUIRE_FALSE(workspace.openMap(otherPath, testDimensions));
    REQUIRE(workspace.mapPath() == *workspace.workingPath());
    REQUIRE_FALSE(workspace.openMap(otherPath, testDimensions, PendingChangesAction::cancel));
    REQUIRE(workspace.openMap(otherPath, testDimensions, PendingChangesAction::discard));
    REQUIRE_FALSE(workspace.dirty());
    REQUIRE(workspace.mapPath() == otherPath);

    REQUIRE(workspace.openMap(originalPath, testDimensions));
    workspace.setEditMode(true);
    REQUIRE(workspace.replaceTileId(initialTile.id, 23, MapEditScope::wholeMap));
    REQUIRE(workspace.reader()->readTile({.x = 0, .y = 0}) == uomul::MapTile{.id = 23, .z = initialTile.z});
    REQUIRE(workspace.replaceZ(initialTile.z, 12, MapEditScope::wholeMap));
    REQUIRE(workspace.reader()->readTile({.x = 0, .y = 0}) == uomul::MapTile{.id = 23, .z = 12});
    REQUIRE(workspace.save(savedPath));
    REQUIRE_FALSE(workspace.dirty());
    REQUIRE(workspace.mapPath() == savedPath);
    REQUIRE(uomul::MapReader::open(savedPath, testDimensions)->readTile({.x = 0, .y = 0}) == uomul::MapTile{.id = 23, .z = 12});
}

TEST_CASE("mapWorkspace_Should_RejectSelectionEditsWithoutSelectionAndDiscardBackToSource")
{
    const TestDirectory directory;
    const auto sourcePath = directory.path() / "source.mul";
    REQUIRE(uomul::MapEditor::createEmpty(sourcePath, testDimensions, initialTile));
    MapWorkspace workspace;
    REQUIRE(workspace.openMap(sourcePath, testDimensions));
    workspace.setEditMode(true);
    REQUIRE_FALSE(workspace.replaceTileId(initialTile.id, 44, MapEditScope::selection));
    REQUIRE_FALSE(workspace.dirty());
    workspace.setBlockSelection({.min = {.x = 1, .y = 1}, .maxExclusive = {.x = 2, .y = 2}});
    REQUIRE(workspace.replaceZ(initialTile.z, 31, MapEditScope::selection));
    REQUIRE(workspace.reader()->readTile({.x = 8, .y = 8}) == uomul::MapTile{.id = initialTile.id, .z = 31});
    const auto workingPath = *workspace.workingPath();
    REQUIRE(workspace.discard());
    REQUIRE_FALSE(workspace.dirty());
    REQUIRE_FALSE(std::filesystem::exists(workingPath));
    REQUIRE(workspace.reader()->readTile({.x = 8, .y = 8}) == initialTile);
    REQUIRE(workspace.closeMap());
    REQUIRE(workspace.reader() == nullptr);
    REQUIRE(workspace.mapPath().empty());
}
