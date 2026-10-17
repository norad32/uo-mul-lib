#include "map_test_helpers.h"
#include "uomul/map.h"
#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <vector>

namespace
{

using uomul::test::dimensions;
using uomul::test::TestDirectory;
using uomul::test::writeFixture;

constexpr uomul::TileCoord changedTileCoordinate{.x = 9, .y = 9};
constexpr uomul::MapTile changedTile{.id = 1234, .z = -20};
constexpr uomul::Dimensions rectangularDimensions{.widthTiles = 24, .heightTiles = 16};
constexpr std::uint32_t rectangularFixtureBlockCount = 6;
constexpr std::uint16_t changedBlockTileId = 2468;
constexpr std::int8_t changedBlockTileZ = 37;
constexpr uomul::TileRegion tileRegion{.min = {.x = 1, .y = 2}, .maxExclusive = {.x = 3, .y = 4}};
constexpr uomul::BlockRegion blockRegion{.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 2, .y = 1}};
constexpr uomul::TileRegion fillRegion{.min = {.x = 1, .y = 1}, .maxExclusive = {.x = 3, .y = 3}};
constexpr uomul::TileRegion firstBlockRegion{.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 8, .y = 8}};
constexpr uomul::MapTile fillTile{.id = 765, .z = -30};
constexpr std::uint16_t changedDimensionTileId = 4321;
constexpr std::uint32_t maximumBlocksPerIoBatch = 4096;
constexpr std::uint32_t batchBoundaryBlockCount = maximumBlocksPerIoBatch + 1;
constexpr std::uint32_t batchBoundaryLastTileY = ((batchBoundaryBlockCount - 1) * uomul::mapBlockDimension) + 1;
constexpr uomul::Dimensions batchBoundaryDimensions{
    .widthTiles = uomul::mapBlockDimension,
    .heightTiles = batchBoundaryBlockCount * uomul::mapBlockDimension,
};

[[nodiscard]] uomul::MapBlock changedBlock()
{
    uomul::MapBlock block{};
    block.fill({.id = changedBlockTileId, .z = changedBlockTileZ});
    return block;
}

[[nodiscard]] std::expected<std::vector<uomul::MapTile>, uomul::Error> readTilesInRegion(const uomul::MapReader & reader,
                                                                                         uomul::TileRegion region)
{
    std::vector<uomul::MapTile> tiles;
    const auto result =
        reader.readTiles(region,
                         [&tiles](uomul::TileCoord, const uomul::MapTile & tile) -> std::expected<void, uomul::Error>
                         {
                             tiles.push_back(tile);
                             return {};
                         });
    if (!result)
    {
        return std::unexpected(result.error());
    }
    return tiles;
}

[[nodiscard]] std::expected<std::vector<uomul::MapBlock>, uomul::Error> readBlocksInRegion(const uomul::MapReader & reader,
                                                                                           uomul::BlockRegion region)
{
    std::vector<uomul::MapBlock> blocks;
    const auto result =
        reader.readBlocks(region,
                          [&blocks](uomul::BlockCoord, const uomul::MapBlock & block) -> std::expected<void, uomul::Error>
                          {
                              blocks.push_back(block);
                              return {};
                          });
    if (!result)
    {
        return std::unexpected(result.error());
    }
    return blocks;
}

[[nodiscard]] std::expected<std::vector<uomul::MapBlockBytes>, uomul::Error>
readRawBlocksInRegion(const uomul::MapReader & reader, uomul::BlockRegion region)
{
    std::vector<uomul::MapBlockBytes> blocks;
    const auto result = reader.readRawBlocks(
        region,
        [&blocks](uomul::BlockCoord, const uomul::MapBlockBytes & block) -> std::expected<void, uomul::Error>
        {
            blocks.push_back(block);
            return {};
        });
    if (!result)
    {
        return std::unexpected(result.error());
    }
    return blocks;
}

} // namespace

TEST_CASE("readWriteDiffTile_Should_RoundTripEditedTileThroughGeneratedDiff")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto edited = directory.path() / "edited.mul";
    const auto diffDirectory = directory.path() / "diff";
    const auto applied = directory.path() / "applied.mul";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(source);

    auto editor = uomul::MapEditor::create(source, edited, dimensions);
    REQUIRE(editor);
    REQUIRE(editor->editTile(changedTileCoordinate, changedTile));
    REQUIRE(editor->commit());

    const auto editedReader = uomul::MapReader::open(edited, dimensions);
    REQUIRE(editedReader);
    REQUIRE(editedReader->readTile(changedTileCoordinate) == changedTile);

    const auto generated = uomul::MapDiff::generate(source, edited, diffDirectory, 0, dimensions);
    REQUIRE(generated);
    REQUIRE(generated->blocksChanged == 1);
    REQUIRE(generated->tilesChanged == 1);
    REQUIRE(uomul::MapDiff::apply(source, diffDirectory, applied, 0, dimensions));

    const auto appliedReader = uomul::MapReader::open(applied, dimensions);
    REQUIRE(appliedReader);
    REQUIRE(appliedReader->readTile(changedTileCoordinate) == changedTile);
}

TEST_CASE("readWriteDiffBlock_Should_RoundTripEditedBlockThroughGeneratedDiff")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto edited = directory.path() / "edited.mul";
    const auto diffDirectory = directory.path() / "diff";
    const auto applied = directory.path() / "applied.mul";
    constexpr uomul::BlockCoord changedCoordinate{.x = 2, .y = 1};
    const auto replacementBlock = changedBlock();
    std::filesystem::create_directories(diffDirectory);
    writeFixture(source, rectangularFixtureBlockCount);

    auto editor = uomul::MapEditor::create(source, edited, rectangularDimensions);
    REQUIRE(editor);
    REQUIRE(editor->editBlock(changedCoordinate, replacementBlock));
    REQUIRE(editor->commit());

    const auto editedReader = uomul::MapReader::open(edited, rectangularDimensions);
    REQUIRE(editedReader);
    REQUIRE(editedReader->readBlock(changedCoordinate) == replacementBlock);

    const auto generated = uomul::MapDiff::generate(source, edited, diffDirectory, 0, rectangularDimensions);
    REQUIRE(generated);
    REQUIRE(generated->blocksChanged == 1);
    REQUIRE(generated->tilesChanged == uomul::mapBlockTileCount);
    REQUIRE(uomul::MapDiff::apply(source, diffDirectory, applied, 0, rectangularDimensions));

    const auto appliedReader = uomul::MapReader::open(applied, rectangularDimensions);
    REQUIRE(appliedReader);
    REQUIRE(appliedReader->readBlock(changedCoordinate) == replacementBlock);
}

TEST_CASE("editTiles_Should_RoundTripEditedRegionThroughGeneratedDiff")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto edited = directory.path() / "edited.mul";
    const auto diffDirectory = directory.path() / "diff";
    const auto applied = directory.path() / "applied.mul";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(source);

    auto editor = uomul::MapEditor::create(source, edited, dimensions);
    REQUIRE(editor);
    REQUIRE(editor->editTiles(
        tileRegion,
        [](uomul::TileCoord coordinate, const uomul::MapTile &) -> std::expected<uomul::MapTile, uomul::Error>
        { return uomul::MapTile{.id = static_cast<std::uint16_t>(1234 + coordinate.x + coordinate.y), .z = -20}; }));
    REQUIRE(editor->commit());

    const auto editedReader = uomul::MapReader::open(edited, dimensions);
    REQUIRE(editedReader);
    const auto editedTiles = readTilesInRegion(*editedReader, tileRegion);
    REQUIRE(editedTiles);
    REQUIRE(editedTiles->size() == 4);
    REQUIRE(editedTiles->front() == uomul::MapTile{.id = 1237, .z = -20});
    REQUIRE(editedTiles->back() == uomul::MapTile{.id = 1239, .z = -20});

    const auto generated = uomul::MapDiff::generate(source, edited, diffDirectory, 0, dimensions);
    REQUIRE(generated);
    REQUIRE(generated->blocksChanged == 1);
    REQUIRE(generated->tilesChanged == editedTiles->size());
    REQUIRE(uomul::MapDiff::apply(source, diffDirectory, applied, 0, dimensions));

    const auto appliedReader = uomul::MapReader::open(applied, dimensions);
    REQUIRE(appliedReader);
    const auto appliedTiles = readTilesInRegion(*appliedReader, tileRegion);
    REQUIRE(appliedTiles);
    REQUIRE(*appliedTiles == *editedTiles);
}

TEST_CASE("editBlocks_Should_RoundTripEditedRegionThroughGeneratedDiff")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto edited = directory.path() / "edited.mul";
    const auto diffDirectory = directory.path() / "diff";
    const auto applied = directory.path() / "applied.mul";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(source);

    auto editor = uomul::MapEditor::create(source, edited, dimensions);
    REQUIRE(editor);
    REQUIRE(editor->editBlocks(
        blockRegion,
        [](uomul::BlockCoord coordinate, const uomul::MapBlock & current) -> std::expected<uomul::MapBlock, uomul::Error>
        {
            auto replacement = current;
            replacement.at(0) = uomul::MapTile{.id = static_cast<std::uint16_t>(2468 + coordinate.x), .z = 37};
            return replacement;
        }));
    REQUIRE(editor->commit());

    const auto editedReader = uomul::MapReader::open(edited, dimensions);
    REQUIRE(editedReader);
    const auto editedBlocks = readBlocksInRegion(*editedReader, blockRegion);
    REQUIRE(editedBlocks);
    REQUIRE(editedBlocks->size() == 2);
    REQUIRE(editedBlocks->at(0).at(0) == uomul::MapTile{.id = 2468, .z = 37});
    REQUIRE(editedBlocks->at(1).at(0) == uomul::MapTile{.id = 2469, .z = 37});

    const auto generated = uomul::MapDiff::generate(source, edited, diffDirectory, 0, dimensions);
    REQUIRE(generated);
    REQUIRE(generated->blocksChanged == editedBlocks->size());
    REQUIRE(generated->tilesChanged == editedBlocks->size());
    REQUIRE(uomul::MapDiff::apply(source, diffDirectory, applied, 0, dimensions));

    const auto appliedReader = uomul::MapReader::open(applied, dimensions);
    REQUIRE(appliedReader);
    const auto appliedBlocks = readBlocksInRegion(*appliedReader, blockRegion);
    REQUIRE(appliedBlocks);
    REQUIRE(*appliedBlocks == *editedBlocks);
}

TEST_CASE("fill_Should_RoundTripFilledRegionThroughGeneratedDiff")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto edited = directory.path() / "edited.mul";
    const auto diffDirectory = directory.path() / "diff";
    const auto applied = directory.path() / "applied.mul";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(source);

    auto editor = uomul::MapEditor::create(source, edited, dimensions);
    REQUIRE(editor);
    REQUIRE(editor->fillTiles(fillRegion, fillTile));
    REQUIRE(editor->commit());

    const auto editedReader = uomul::MapReader::open(edited, dimensions);
    REQUIRE(editedReader);
    const auto editedTiles = readTilesInRegion(*editedReader, fillRegion);
    REQUIRE(editedTiles);
    REQUIRE(editedTiles->size() == 4);
    REQUIRE(std::ranges::all_of(*editedTiles, [](const uomul::MapTile & tile) { return tile == fillTile; }));

    const auto generated = uomul::MapDiff::generate(source, edited, diffDirectory, 0, dimensions);
    REQUIRE(generated);
    REQUIRE(generated->blocksChanged == 1);
    REQUIRE(generated->tilesChanged == editedTiles->size());
    REQUIRE(uomul::MapDiff::apply(source, diffDirectory, applied, 0, dimensions));

    const auto appliedReader = uomul::MapReader::open(applied, dimensions);
    REQUIRE(appliedReader);
    const auto appliedTiles = readTilesInRegion(*appliedReader, fillRegion);
    REQUIRE(appliedTiles);
    REQUIRE(*appliedTiles == *editedTiles);
}

TEST_CASE("replaceIds_Should_RoundTripUpdatedRegionThroughGeneratedDiff")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto edited = directory.path() / "edited.mul";
    const auto diffDirectory = directory.path() / "diff";
    const auto applied = directory.path() / "applied.mul";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(source);

    auto editor = uomul::MapEditor::create(source, edited, dimensions);
    REQUIRE(editor);
    REQUIRE(editor->replaceIdsTiles(firstBlockRegion, 0, 654));
    REQUIRE(editor->commit());

    const auto editedReader = uomul::MapReader::open(edited, dimensions);
    REQUIRE(editedReader);
    const auto editedTiles = readTilesInRegion(*editedReader, firstBlockRegion);
    REQUIRE(editedTiles);
    REQUIRE(editedTiles->at(0) == uomul::MapTile{.id = 654, .z = -1});
    REQUIRE(editedTiles->at(1).id != 654);

    const auto generated = uomul::MapDiff::generate(source, edited, diffDirectory, 0, dimensions);
    REQUIRE(generated);
    REQUIRE(generated->blocksChanged == 1);
    REQUIRE(generated->tilesChanged == 1);
    REQUIRE(uomul::MapDiff::apply(source, diffDirectory, applied, 0, dimensions));

    const auto appliedReader = uomul::MapReader::open(applied, dimensions);
    REQUIRE(appliedReader);
    const auto appliedTiles = readTilesInRegion(*appliedReader, firstBlockRegion);
    REQUIRE(appliedTiles);
    REQUIRE(*appliedTiles == *editedTiles);
}

TEST_CASE("replaceZ_Should_RoundTripUpdatedRegionThroughGeneratedDiff")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto edited = directory.path() / "edited.mul";
    const auto diffDirectory = directory.path() / "diff";
    const auto applied = directory.path() / "applied.mul";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(source);

    auto editor = uomul::MapEditor::create(source, edited, dimensions);
    REQUIRE(editor);
    REQUIRE(editor->replaceZTiles(firstBlockRegion, -1, -40));
    REQUIRE(editor->commit());

    const auto editedReader = uomul::MapReader::open(edited, dimensions);
    REQUIRE(editedReader);
    const auto editedTiles = readTilesInRegion(*editedReader, firstBlockRegion);
    REQUIRE(editedTiles);
    REQUIRE(editedTiles->at(0) == uomul::MapTile{.id = 0, .z = -40});
    REQUIRE(editedTiles->at(1).z != -40);

    const auto generated = uomul::MapDiff::generate(source, edited, diffDirectory, 0, dimensions);
    REQUIRE(generated);
    REQUIRE(generated->blocksChanged == 1);
    REQUIRE(generated->tilesChanged == 1);
    REQUIRE(uomul::MapDiff::apply(source, diffDirectory, applied, 0, dimensions));

    const auto appliedReader = uomul::MapReader::open(applied, dimensions);
    REQUIRE(appliedReader);
    const auto appliedTiles = readTilesInRegion(*appliedReader, firstBlockRegion);
    REQUIRE(appliedTiles);
    REQUIRE(*appliedTiles == *editedTiles);
}

TEST_CASE("blockBatchReading_Should_ReadPastMaximumBatchSize")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path, batchBoundaryBlockCount);
    const auto reader = uomul::MapReader::open(path, batchBoundaryDimensions);
    REQUIRE(reader);

    std::uint32_t callbackCount = 0;
    const auto result = reader->readRawBlocks(
        {.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 1, .y = batchBoundaryBlockCount}},
        [&callbackCount](uomul::BlockCoord, const uomul::MapBlockBytes &) -> std::expected<void, uomul::Error>
        {
            ++callbackCount;
            return {};
        });

    REQUIRE(result);
    REQUIRE(callbackCount == batchBoundaryBlockCount);
}

TEST_CASE("diffBatchReading_Should_ReadPastMaximumBatchSize")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto target = directory.path() / "target.mul";
    const auto diffDirectory = directory.path() / "diff";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(base, batchBoundaryBlockCount);

    auto editor = uomul::MapEditor::create(base, target, batchBoundaryDimensions);
    REQUIRE(editor);
    REQUIRE(editor->editTile({.x = 0, .y = batchBoundaryLastTileY}, {.id = 9876, .z = 42}));
    REQUIRE(editor->commit());

    const auto generated = uomul::MapDiff::generate(base, target, diffDirectory, 0, batchBoundaryDimensions);

    REQUIRE(generated);
    REQUIRE(generated->blocksChanged == 1);
    REQUIRE(generated->tilesChanged == 1);
}

TEST_CASE("readRawBlocks_Should_RoundTripRawRegionThroughGeneratedDiff")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto edited = directory.path() / "edited.mul";
    const auto diffDirectory = directory.path() / "diff";
    const auto applied = directory.path() / "applied.mul";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(source);

    auto editor = uomul::MapEditor::create(source, edited, dimensions);
    REQUIRE(editor);
    REQUIRE(editor->editTile({.x = 0, .y = 0}, changedTile));
    REQUIRE(editor->commit());
    REQUIRE(uomul::MapDiff::generate(source, edited, diffDirectory, 0, dimensions));
    REQUIRE(uomul::MapDiff::apply(source, diffDirectory, applied, 0, dimensions));

    const auto sourceReader = uomul::MapReader::open(source, dimensions);
    const auto editedReader = uomul::MapReader::open(edited, dimensions);
    const auto appliedReader = uomul::MapReader::open(applied, dimensions);
    REQUIRE(sourceReader);
    REQUIRE(editedReader);
    REQUIRE(appliedReader);
    const auto sourceBlocks = readRawBlocksInRegion(*sourceReader, blockRegion);
    const auto editedBlocks = readRawBlocksInRegion(*editedReader, blockRegion);
    const auto appliedBlocks = readRawBlocksInRegion(*appliedReader, blockRegion);
    REQUIRE(sourceBlocks);
    REQUIRE(editedBlocks);
    REQUIRE(appliedBlocks);
    REQUIRE(*appliedBlocks == *editedBlocks);
    REQUIRE(appliedBlocks->at(0) != sourceBlocks->at(0));
    REQUIRE(appliedBlocks->at(1) == sourceBlocks->at(1));
}

TEST_CASE("readWriteDiffDimensions_Should_RoundTripRectangularMapEdgeTile")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto edited = directory.path() / "edited.mul";
    const auto diffDirectory = directory.path() / "diff";
    const auto applied = directory.path() / "applied.mul";
    const uomul::TileCoord edgeCoordinate{
        .x = rectangularDimensions.widthTiles - 1,
        .y = rectangularDimensions.heightTiles - 1,
    };
    std::filesystem::create_directories(diffDirectory);
    writeFixture(source, rectangularFixtureBlockCount);

    const auto sourceReader = uomul::MapReader::open(source, rectangularDimensions);
    REQUIRE(sourceReader);
    REQUIRE(sourceReader->dimensions() == rectangularDimensions);
    REQUIRE(sourceReader->completeBlocks() == rectangularFixtureBlockCount);
    const auto outOfBoundsX = sourceReader->readTile({.x = rectangularDimensions.widthTiles, .y = 0});
    REQUIRE_FALSE(outOfBoundsX);
    REQUIRE(outOfBoundsX.error().code == uomul::ErrorCode::invalidCoordinate);
    const auto outOfBoundsY = sourceReader->readTile({.x = 0, .y = rectangularDimensions.heightTiles});
    REQUIRE_FALSE(outOfBoundsY);
    REQUIRE(outOfBoundsY.error().code == uomul::ErrorCode::invalidCoordinate);

    auto editor = uomul::MapEditor::create(source, edited, rectangularDimensions);
    REQUIRE(editor);
    REQUIRE(editor->editTile(edgeCoordinate, {.id = changedDimensionTileId, .z = 37}));
    REQUIRE(editor->commit());

    const auto generated = uomul::MapDiff::generate(source, edited, diffDirectory, 0, rectangularDimensions);
    REQUIRE(generated);
    REQUIRE(generated->blocksChanged == 1);
    REQUIRE(generated->tilesChanged == 1);
    REQUIRE(uomul::MapDiff::apply(source, diffDirectory, applied, 0, rectangularDimensions));

    const auto editedReader = uomul::MapReader::open(edited, rectangularDimensions);
    const auto appliedReader = uomul::MapReader::open(applied, rectangularDimensions);
    REQUIRE(editedReader);
    REQUIRE(appliedReader);
    REQUIRE(editedReader->dimensions() == rectangularDimensions);
    REQUIRE(appliedReader->dimensions() == rectangularDimensions);
    const auto editedEdgeTile = editedReader->readTile(edgeCoordinate);
    const auto appliedEdgeTile = appliedReader->readTile(edgeCoordinate);
    REQUIRE(editedEdgeTile);
    REQUIRE(appliedEdgeTile);
    REQUIRE(*appliedEdgeTile == *editedEdgeTile);
}
