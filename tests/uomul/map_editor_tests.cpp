#include "map_test_helpers.h"
#include "uomul/map.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>

namespace
{

using uomul::test::dimensions;
using uomul::test::readBytes;
using uomul::test::TestDirectory;
using uomul::test::writeFixture;

constexpr std::uint32_t blockGridSize = dimensions.widthTiles / uomul::landBlockDimension;
constexpr std::uint32_t testRegionEnd = 2;
constexpr std::uint32_t failedCallbackCoordinate = 8;
constexpr std::uint32_t fullMapRegionEnd = dimensions.widthTiles;
constexpr std::uint32_t firstTileCoordinate = testRegionEnd - 1;
constexpr std::uint32_t secondTileCoordinate = testRegionEnd;
constexpr std::uint16_t stagedTileId = 777;
constexpr std::uint16_t editedTileId = 778;
constexpr std::uint16_t stagedBlockTileId = 999;
constexpr std::int8_t stagedTileElevation = -12;

} // namespace

TEST_CASE("MapEditor_Should_PreserveHeadersAndPublishOnlyOnCommit")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);
    REQUIRE(editor->fill({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 2, .y = 2}},
                         {.id = stagedTileId, .z = stagedTileElevation}));
    REQUIRE_FALSE(std::filesystem::exists(output));
    REQUIRE(editor->commit());
    const auto original = readBytes(source);
    const auto modified = readBytes(output);
    REQUIRE(modified.size() == original.size());
    REQUIRE(modified.substr(0, 4) == original.substr(0, 4));
    const auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readLandTile({1, 1})->id == stagedTileId);
    REQUIRE(reader->readLandTile({2, 2})->id == 18);
}

TEST_CASE("MapEditor_Should_AbortWithoutPublishingAndKeepSourceUntouched")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    const auto original = readBytes(source);
    {
        auto editor = uomul::MapEditor::create(source, output, dimensions);
        REQUIRE(editor);
        REQUIRE(editor->writeLandTile({0, 0}, {.id = 900, .z = 3}));
    }
    REQUIRE_FALSE(std::filesystem::exists(output));
    REQUIRE(readBytes(source) == original);
}

TEST_CASE("MapEditor_Should_RejectInPlaceEditing")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    writeFixture(source);
    REQUIRE_FALSE(uomul::MapEditor::create(source, source, dimensions, {.overwriteOutput = true}));
}

TEST_CASE("MapEditor_Should_KeepFailedRegionCallbackAllOrNothing")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);
    const auto failed = editor->editLandTiles(
        {.min = {.x = 0, .y = 0}, .maxExclusive = {.x = fullMapRegionEnd, .y = fullMapRegionEnd}},
        [](uomul::TileCoord coordinate, const uomul::LandTile & tile) -> std::expected<uomul::LandTile, uomul::Error>
        {
            if (coordinate.x == failedCallbackCoordinate && coordinate.y == failedCallbackCoordinate)
            {
                return std::unexpected(
                    uomul::Error{.code = uomul::ErrorCode::io, .path = {}, .offset = 0, .message = "test failure"});
            }
            return uomul::LandTile{.id = stagedBlockTileId, .z = tile.z};
        });
    REQUIRE_FALSE(failed);
    REQUIRE(editor->commit());
    const auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readLandTile({.x = 0, .y = 0})->id == 0);
}

TEST_CASE("writeLandTile_Should_RejectCoordinates_If_TileOrBlockIsOutOfBounds")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);

    const auto invalidTile = editor->writeLandTile({.x = dimensions.widthTiles, .y = 0}, {.id = 1, .z = 0});
    REQUIRE_FALSE(invalidTile);
    REQUIRE(invalidTile.error().code == uomul::ErrorCode::invalidCoordinate);
    const auto invalidBlock = editor->writeLandBlock({.x = 0, .y = blockGridSize}, uomul::LandBlock{});
    REQUIRE_FALSE(invalidBlock);
    REQUIRE(invalidBlock.error().code == uomul::ErrorCode::invalidCoordinate);
}

TEST_CASE("editLandBlocks_Should_KeepStagedDataUnchanged_If_CallbackFails")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);

    const auto result = editor->editLandBlocks(
        {.min = {.x = 0, .y = 0}, .maxExclusive = {.x = testRegionEnd, .y = testRegionEnd}},
        [](uomul::BlockCoord coordinate, const uomul::LandBlock & current) -> std::expected<uomul::LandBlock, uomul::Error>
        {
            if (coordinate.x == 0 && coordinate.y == testRegionEnd - 1)
            {
                return std::unexpected(
                    uomul::Error{.code = uomul::ErrorCode::io, .path = {}, .offset = 0, .message = "callback failure"});
            }
            auto changed = current;
            changed.at(0).id = stagedBlockTileId;
            return changed;
        });
    REQUIRE_FALSE(result);
    REQUIRE(editor->commit());

    const auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readLandTile({.x = 0, .y = 0})->id == 0);
}

TEST_CASE("editLandTiles_Should_UseEarlierStagedValues_If_RegionIsEditedAgain")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);

    const uomul::TileRegion region{.min = {.x = 0, .y = 0}, .maxExclusive = {.x = testRegionEnd, .y = testRegionEnd}};
    REQUIRE(editor->fill(region, {.id = stagedTileId, .z = stagedTileElevation}));
    REQUIRE(editor->editLandTiles(
        region,
        [](uomul::TileCoord, const uomul::LandTile & current) -> std::expected<uomul::LandTile, uomul::Error>
        {
            if (current.id != stagedTileId)
            {
                return std::unexpected(uomul::Error{
                    .code = uomul::ErrorCode::io, .path = {}, .offset = 0, .message = "staged value was not visible"});
            }
            return uomul::LandTile{.id = editedTileId, .z = current.z};
        }));
    REQUIRE(editor->commit());

    const auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readLandTile({.x = firstTileCoordinate, .y = firstTileCoordinate}) ==
            uomul::LandTile{.id = editedTileId, .z = stagedTileElevation});
    REQUIRE(reader->readLandTile({.x = secondTileCoordinate, .y = secondTileCoordinate})->id == 18);
}
