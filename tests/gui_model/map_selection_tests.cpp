#include "gui_model/world/map_selection.h"
#include <catch2/catch_test_macros.hpp>

using uo_mule::gui::MapSelectionMask;
using uo_mule::gui::selectionCellHasExposedEdge;
using uo_mule::gui::ToolSelectionGranularity;

TEST_CASE("selectionMask_Should_ReplaceRegionsAtTheRequestedGranularity")
{
    MapSelectionMask mask;
    const uomul::Dimensions dimensions{.widthTiles = 10, .heightTiles = 9};

    mask.replaceTileRegion({.min = {.x = 2, .y = 3}, .maxExclusive = {.x = 4, .y = 5}}, dimensions);
    REQUIRE(mask.granularity() == ToolSelectionGranularity::tiles);
    REQUIRE(mask.size() == 4);
    REQUIRE(mask.contains(uomul::TileCoord{.x = 2, .y = 3}));
    REQUIRE_FALSE(mask.contains(uomul::TileCoord{.x = 4, .y = 3}));

    mask.replaceBlockRegion({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 2, .y = 2}}, dimensions);
    REQUIRE(mask.granularity() == ToolSelectionGranularity::blocks);
    REQUIRE(mask.size() == 4);
    REQUIRE(mask.contains(uomul::BlockCoord{.x = 1, .y = 1}));
}

TEST_CASE("selectionMask_Should_AddAndSubtractSparseSelections")
{
    MapSelectionMask base;
    MapSelectionMask addition;
    const uomul::Dimensions dimensions{.widthTiles = 8, .heightTiles = 8};
    base.replaceTileRegion({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 2, .y = 1}}, dimensions);
    addition.replaceTileRegion({.min = {.x = 2, .y = 0}, .maxExclusive = {.x = 3, .y = 1}}, dimensions);

    base.add(addition);
    REQUIRE(base.size() == 3);
    base.subtract(addition);
    REQUIRE(base.size() == 2);
    REQUIRE_FALSE(base.contains(uomul::TileCoord{.x = 2, .y = 0}));
}

TEST_CASE("selectionMask_Should_SelectAllAndInvertTiles")
{
    MapSelectionMask mask;
    const uomul::Dimensions dimensions{.widthTiles = 3, .heightTiles = 2};

    mask.selectAll(dimensions);
    REQUIRE(mask.size() == 6);
    mask.invert(dimensions);
    REQUIRE(mask.empty());
    mask.invert(dimensions);
    REQUIRE(mask.size() == 6);
}

TEST_CASE("selectionMask_Should_SelectAllAndInvertClippedBlocks")
{
    MapSelectionMask mask;
    const uomul::Dimensions dimensions{.widthTiles = 9, .heightTiles = 10};
    mask.setGranularity(ToolSelectionGranularity::blocks);

    mask.selectAll(dimensions);
    REQUIRE(mask.size() == 4);
    REQUIRE(mask.contains(uomul::BlockCoord{.x = 1, .y = 1}));
    mask.invert(dimensions);
    REQUIRE(mask.empty());
}

TEST_CASE("selectionMask_Should_ConvertTilesToCoveringBlocks")
{
    MapSelectionMask mask;
    const uomul::Dimensions dimensions{.widthTiles = 16, .heightTiles = 16};
    mask.replaceTileRegion({.min = {.x = 7, .y = 7}, .maxExclusive = {.x = 9, .y = 9}}, dimensions);

    mask.convertTilesToCoveringBlocks(dimensions);

    REQUIRE(mask.granularity() == ToolSelectionGranularity::blocks);
    REQUIRE(mask.size() == 4);
    REQUIRE(mask.contains(uomul::BlockCoord{.x = 0, .y = 0}));
    REQUIRE(mask.contains(uomul::BlockCoord{.x = 1, .y = 1}));
}

TEST_CASE("selectionMask_Should_IgnoreCombinesAtDifferentGranularities")
{
    MapSelectionMask tiles;
    MapSelectionMask blocks;
    const uomul::Dimensions dimensions{.widthTiles = 8, .heightTiles = 8};
    tiles.replaceTileRegion({.min = {}, .maxExclusive = {.x = 1, .y = 1}}, dimensions);
    blocks.setGranularity(ToolSelectionGranularity::blocks);
    blocks.replaceBlockRegion({.min = {}, .maxExclusive = {.x = 1, .y = 1}}, dimensions);

    tiles.add(blocks);
    REQUIRE(tiles.size() == 1);
    tiles.subtract(blocks);
    REQUIRE(tiles.size() == 1);
}

TEST_CASE("selectionMask_Should_RasterizeTileLassoByCellCenters")
{
    MapSelectionMask mask;
    const uomul::Dimensions dimensions{.widthTiles = 6, .heightTiles = 6};
    const std::vector<uomul::TileCoord> path{
        {.x = 1, .y = 1}, {.x = 4, .y = 1}, {.x = 4, .y = 4}, {.x = 1, .y = 4},
    };

    mask.replaceLasso(path, dimensions);

    REQUIRE(mask.size() == 9);
    REQUIRE(mask.contains(uomul::TileCoord{.x = 2, .y = 2}));
    REQUIRE_FALSE(mask.contains(uomul::TileCoord{.x = 0, .y = 0}));
}

TEST_CASE("selectionMask_Should_RasterizeBlockLassoByTouchedBlocks")
{
    MapSelectionMask mask;
    const uomul::Dimensions dimensions{.widthTiles = 16, .heightTiles = 16};
    mask.setGranularity(ToolSelectionGranularity::blocks);
    const std::vector<uomul::TileCoord> path{
        {.x = 7, .y = 1}, {.x = 9, .y = 1}, {.x = 9, .y = 9},
    };

    mask.replaceLasso(path, dimensions);

    REQUIRE(mask.size() == 3);
    REQUIRE(mask.contains(uomul::BlockCoord{.x = 0, .y = 0}));
    REQUIRE(mask.contains(uomul::BlockCoord{.x = 1, .y = 1}));
}

TEST_CASE("selectionMask_Should_ExposeOnlyOuterEdgesForAdjacentCells")
{
    MapSelectionMask mask;
    const uomul::Dimensions dimensions{.widthTiles = 4, .heightTiles = 4};
    mask.replaceTileRegion({.min = {.x = 1, .y = 1}, .maxExclusive = {.x = 3, .y = 3}}, dimensions);

    REQUIRE(selectionCellHasExposedEdge(mask, 1, 1, -1, 0, dimensions));
    REQUIRE(selectionCellHasExposedEdge(mask, 1, 1, 0, -1, dimensions));
    REQUIRE_FALSE(selectionCellHasExposedEdge(mask, 1, 1, 1, 0, dimensions));
    REQUIRE_FALSE(selectionCellHasExposedEdge(mask, 1, 1, 0, 1, dimensions));
}
