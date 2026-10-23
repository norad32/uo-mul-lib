#include "../test_helpers.h"
#include "map_test_helpers.h"
#include "uomul/map/map.h"
#include "uomul/mul_common/mul_common.h"
#include <array>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace
{

using uomul::test::dimensions;
using uomul::test::TestDirectory;
using uomul::test::writeFixture;

constexpr std::uint32_t blockGridSize = dimensions.widthTiles / uomul::mapBlockDimension;
constexpr std::uint32_t invalidTileDimension = uomul::mapBlockDimension - 1;
constexpr std::uint32_t edgeStartTile = uomul::mapBlockDimension - 1;
constexpr std::uint32_t edgeRegionWidth = dimensions.widthTiles - edgeStartTile;
constexpr std::uint32_t callbackFailureInvocation = 3;
constexpr std::uint32_t testRegionEnd = 2;
constexpr std::size_t landHeaderByteCount = 4;
constexpr uomul::Dimensions partialEdgeDimensions{.widthTiles = 17, .heightTiles = 9};
constexpr uomul::Dimensions truncatedMapDimensions{.widthTiles = 32, .heightTiles = 16};
constexpr std::uint32_t partialEdgeFixtureBlockCount = 6;
constexpr std::uint32_t extraCompleteBlockFixtureCount = 5;
constexpr double expectedAverageElevation = 31.484375;
constexpr double expectedMedianElevation = 31.5;
constexpr double expectedSingleTilePercentage = 100.0 / (dimensions.widthTiles * dimensions.heightTiles);

void writeTruncatedEdgeFixture(const std::filesystem::path & path)
{
    writeFixture(path, partialEdgeFixtureBlockCount);
    std::ofstream output(path, std::ios::binary | std::ios::app);
    output.put('x');
    output.put('y');
}

[[nodiscard]] std::expected<std::vector<uomul::TileCoord>, uomul::Error> readTilesInRegion(const uomul::MapReader & reader,
                                                                                           uomul::TileRegion region)
{
    std::vector<uomul::TileCoord> coordinates;
    const auto result = reader.readTiles(
        region,
        [&coordinates](uomul::TileCoord coordinate, const uomul::MapTile &) -> std::expected<void, uomul::Error>
        {
            coordinates.push_back(coordinate);
            return {};
        });
    if (!result)
    {
        return std::unexpected(result.error());
    }
    return coordinates;
}

[[nodiscard]] bool blockCoordinatesAreInReadOrder(const std::vector<uomul::BlockCoord> & coordinates)
{
    return coordinates.size() == 4 && coordinates.at(0).x == 0 && coordinates.at(0).y == 0 && coordinates.at(1).x == 0 &&
           coordinates.at(1).y == 1 && coordinates.at(2).x == 1 && coordinates.at(2).y == 0 && coordinates.at(3).x == 1 &&
           coordinates.at(3).y == 1;
}

template <typename Result>
void requireInvalidCoordinate(const Result & result)
{
    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::invalidCoordinate);
}

} // namespace

TEST_CASE("open_Should_ExposeMapDimensionsAndFileCounts_If_FileMatchesDimensions")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);

    const auto reader = uomul::MapReader::open(path, dimensions);

    REQUIRE(reader);
    REQUIRE(reader->dimensions() == dimensions);
    REQUIRE(reader->completeBlocks() == 4);
    REQUIRE(reader->ignoredTrailingBytes() == 0);
    REQUIRE(reader->missingBlocks() == 0);
}

TEST_CASE("open_Should_ReturnIoError_If_MapFileDoesNotExist")
{
    const TestDirectory directory;

    const auto reader = uomul::MapReader::open(directory.path() / "missing.mul", dimensions);

    REQUIRE_FALSE(reader);
    REQUIRE(reader.error().code == uomul::ErrorCode::io);
}

TEST_CASE("open_Should_RejectDimensions_If_ZeroOrNotMultiplesOfEight")
{
    const TestDirectory directory;
    const std::array<uomul::Dimensions, 5> invalidDimensions{
        uomul::Dimensions{.widthTiles = 0, .heightTiles = dimensions.heightTiles},
        uomul::Dimensions{.widthTiles = dimensions.widthTiles, .heightTiles = 0},
        uomul::Dimensions{.widthTiles = invalidTileDimension, .heightTiles = dimensions.heightTiles},
        uomul::Dimensions{.widthTiles = dimensions.widthTiles, .heightTiles = invalidTileDimension},
        uomul::Dimensions{.widthTiles = UINT32_MAX - 7, .heightTiles = UINT32_MAX - 7},
    };

    for (const auto dimensionsToCheck : invalidDimensions)
    {
        const auto reader = uomul::MapReader::open(directory.path() / "map.mul", dimensionsToCheck);
        REQUIRE_FALSE(reader);
        REQUIRE(reader.error().code == uomul::ErrorCode::invalidDimensions);
    }
}

TEST_CASE("open_Should_RejectPartialEdgeDimensions_If_DimensionsAreNotBlockAligned")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);

    const auto reader = uomul::MapReader::open(path, partialEdgeDimensions);

    REQUIRE_FALSE(reader);
    REQUIRE(reader.error().code == uomul::ErrorCode::invalidDimensions);
}

TEST_CASE("open_Should_RejectIncompleteMap_If_RecoveryIsDisabled")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path, 2);

    const auto reader = uomul::MapReader::open(path, dimensions);

    REQUIRE_FALSE(reader);
    REQUIRE(reader.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("open_Should_ExposeCompletePrefixAndMissingBlocks_If_RecoveryIsEnabled")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path, 2);

    const auto reader = uomul::MapReader::open(path, dimensions, {.recoverTruncatedMap = true});

    REQUIRE(reader);
    REQUIRE(reader->completeBlocks() == 2);
    REQUIRE(reader->missingBlocks() == 2);
    REQUIRE(reader->readBlock({.x = 0, .y = 1}));
    const auto missing = reader->readBlock({.x = 1, .y = 0});
    REQUIRE_FALSE(missing);
    REQUIRE(missing.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("open_Should_ReportIgnoredTrailingBytes_If_RecoveryIsEnabled")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path, 3);
    {
        std::ofstream output(path, std::ios::binary | std::ios::app);
        output.put('x');
    }

    REQUIRE_FALSE(uomul::MapReader::open(path, dimensions));
    const auto reader = uomul::MapReader::open(path, dimensions, {.recoverTruncatedMap = true});

    REQUIRE(reader);
    REQUIRE(reader->completeBlocks() == 3);
    REQUIRE(reader->ignoredTrailingBytes() == 1);
    REQUIRE(reader->missingBlocks() == 1);
}

TEST_CASE("open_Should_RejectPartialBlock_If_TrailingBytesExceedExpectedSize")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    {
        std::ofstream output(path, std::ios::binary | std::ios::app);
        output.put('x');
    }

    const auto reader = uomul::MapReader::open(path, dimensions, {.recoverTruncatedMap = true});

    REQUIRE_FALSE(reader);
    REQUIRE(reader.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("open_Should_RejectExtraCompleteBlocks_If_RecoveryIsEnabled")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path, extraCompleteBlockFixtureCount);

    const auto reader = uomul::MapReader::open(path, dimensions, {.recoverTruncatedMap = true});

    REQUIRE_FALSE(reader);
    REQUIRE(reader.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("open_Should_ReportMissingBlocksAndTrailingBytes_If_RecoveredMapIsTruncated")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeTruncatedEdgeFixture(path);

    const auto reader = uomul::MapReader::open(path, truncatedMapDimensions, {.recoverTruncatedMap = true});

    REQUIRE(reader);
    REQUIRE(reader->completeBlocks() == partialEdgeFixtureBlockCount);
    REQUIRE(reader->missingBlocks() == 2);
    REQUIRE(reader->ignoredTrailingBytes() == 2);
    REQUIRE(reader->readBlock({.x = 2, .y = 1}));
    REQUIRE_FALSE(reader->readBlock({.x = 3, .y = 0}));
}

TEST_CASE("moveConstructor_Should_KeepReaderUsable_If_ReaderIsMoved")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    auto opened = uomul::MapReader::open(path, dimensions);
    REQUIRE(opened);

    const uomul::MapReader reader(std::move(*opened));

    REQUIRE(reader.dimensions() == dimensions);
    REQUIRE(reader.readTile({0, 0})->id == 0);
}

TEST_CASE("moveAssignment_Should_ReplaceReaderState_If_AnotherReaderIsMovedIntoIt")
{
    const TestDirectory directory;
    const auto firstPath = directory.path() / "first.mul";
    const auto secondPath = directory.path() / "second.mul";
    writeFixture(firstPath);
    writeFixture(secondPath, 2);
    auto first = uomul::MapReader::open(firstPath, dimensions);
    auto second = uomul::MapReader::open(secondPath, dimensions, {.recoverTruncatedMap = true});
    REQUIRE(first);
    REQUIRE(second);

    *first = std::move(*second);

    REQUIRE(first->completeBlocks() == 2);
    REQUIRE(first->readTile({0, 0}));
    REQUIRE_FALSE(first->readTile({8, 0}));
}

TEST_CASE("open_Should_RejectDirectory_If_RecoveryIsEnabled")
{
    const TestDirectory directory;
    constexpr uomul::Dimensions directoryDimensions{.widthTiles = 65536, .heightTiles = 65536};

    const auto reader = uomul::MapReader::open(directory.path(), directoryDimensions, {.recoverTruncatedMap = true});

    REQUIRE_FALSE(reader);
    REQUIRE(reader.error().code == uomul::ErrorCode::io);
}

TEST_CASE("readBlock_Should_DecodeTileIdsAndSignedElevations_If_BlockIsAvailable")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    const auto block = reader->readBlock({.x = 0, .y = 0});

    REQUIRE(block);
    REQUIRE(block->at(0).id == 0);
    REQUIRE(block->at(0).z == -1);
    REQUIRE(block->at(1).id == 1);
    REQUIRE(block->at(1).z == 1);
    REQUIRE(block->at(63).id == 63);
}

TEST_CASE("readBlock_Should_RejectCoordinates_If_BlockIsOutsideMap")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    const auto outsideX = reader->readBlock({.x = blockGridSize, .y = 0});
    const auto outsideY = reader->readBlock({.x = 0, .y = blockGridSize});

    REQUIRE_FALSE(outsideX);
    REQUIRE(outsideX.error().code == uomul::ErrorCode::invalidCoordinate);
    REQUIRE_FALSE(outsideY);
    REQUIRE(outsideY.error().code == uomul::ErrorCode::invalidCoordinate);
}

TEST_CASE("readBlock_Should_ReportReadFailure_If_FileIsTruncatedAfterOpening")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);
    std::filesystem::resize_file(path, 0);

    const auto block = reader->readBlock({.x = 0, .y = 0});

    REQUIRE_FALSE(block);
    REQUIRE(block.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("readTile_Should_DecodeColumnMajorBlocksAndSignedElevation_If_TileIsAvailable")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    const auto tile = reader->readTile({.x = 8, .y = 0});
    REQUIRE(tile);
    REQUIRE(tile->id == 200);
    REQUIRE(tile->z == -1);
    const auto southern = reader->readTile({.x = 0, .y = uomul::mapBlockDimension});
    REQUIRE(southern);
    REQUIRE(southern->id == 100);
}

TEST_CASE("readTile_Should_RejectCoordinates_If_TileIsOutsideMap")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    const auto outsideX = reader->readTile({.x = dimensions.widthTiles, .y = 0});
    const auto outsideY = reader->readTile({.x = 0, .y = dimensions.heightTiles});

    REQUIRE_FALSE(outsideX);
    REQUIRE(outsideX.error().code == uomul::ErrorCode::invalidCoordinate);
    REQUIRE_FALSE(outsideY);
    REQUIRE(outsideY.error().code == uomul::ErrorCode::invalidCoordinate);
}

TEST_CASE("readTile_Should_RejectPartialBlock_If_BlockIsTruncatedAfterOpening")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);
    std::filesystem::resize_file(path, (2U * uomul::mapBlockByteCount) + 2U);

    const auto tile = reader->readTile({.x = uomul::mapBlockDimension, .y = 0});

    REQUIRE_FALSE(tile);
    REQUIRE(tile.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("readTile_Should_ReportReadFailure_If_BlockIsTruncatedAfterOpening")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);
    std::filesystem::resize_file(path, 0);

    const auto tile = reader->readTile({.x = 1, .y = 1});

    REQUIRE_FALSE(tile);
    REQUIRE(tile.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("readTile_Should_ReportMissingBlock_If_TileIsInsideRecoveredDimensions")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path, 2);
    const auto reader = uomul::MapReader::open(path, dimensions, {.recoverTruncatedMap = true});
    REQUIRE(reader);

    const auto tile = reader->readTile({.x = 8, .y = 0});

    REQUIRE_FALSE(tile);
    REQUIRE(tile.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("readBlocks_Should_VisitHalfOpenRegionInBlockOrder_If_RegionIsValid")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);
    std::vector<uomul::BlockCoord> coordinates;

    const auto result = reader->readBlocks(
        {.min = {.x = 0, .y = 0}, .maxExclusive = {.x = testRegionEnd, .y = testRegionEnd}},
        [&coordinates](uomul::BlockCoord coordinate, const uomul::MapBlock &) -> std::expected<void, uomul::Error>
        {
            coordinates.push_back(coordinate);
            return {};
        });

    REQUIRE(result);
    REQUIRE(blockCoordinatesAreInReadOrder(coordinates));
}

TEST_CASE("readBlocks_Should_ReturnSuccessWithoutCallbacks_If_RegionIsEmpty")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    const auto result =
        reader->readBlocks({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 0, .y = 2}},
                           [](uomul::BlockCoord, const uomul::MapBlock &) -> std::expected<void, uomul::Error> { return {}; });

    REQUIRE(result);
}

TEST_CASE("readBlocks_Should_RejectRequest_If_RegionIsReversedOrOutOfBounds")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);
    const auto callback = [](uomul::BlockCoord, const uomul::MapBlock &) -> std::expected<void, uomul::Error> { return {}; };

    const auto reversedX = reader->readBlocks({.min = {.x = 1, .y = 0}, .maxExclusive = {.x = 0, .y = 1}}, callback);
    const auto reversedY = reader->readBlocks({.min = {.x = 0, .y = 1}, .maxExclusive = {.x = 1, .y = 0}}, callback);
    const auto outside =
        reader->readBlocks({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 0, .y = blockGridSize + 1}}, callback);

    requireInvalidCoordinate(reversedX);
    requireInvalidCoordinate(reversedY);
    requireInvalidCoordinate(outside);
}

TEST_CASE("readBlocks_Should_RejectRequest_If_CallbackIsEmpty")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    const auto result = reader->readBlocks({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 1, .y = 1}}, {});

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::io);
}

TEST_CASE("readBlocks_Should_PropagateCallbackError_If_CallbackFails")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);
    std::uint32_t callbackCount = 0;

    const auto result =
        reader->readBlocks({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = testRegionEnd, .y = testRegionEnd}},
                           [&callbackCount](uomul::BlockCoord, const uomul::MapBlock &) -> std::expected<void, uomul::Error>
                           {
                               callbackCount++;
                               if (callbackCount == callbackFailureInvocation)
                               {
                                   return std::unexpected(uomul::Error{
                                       .code = uomul::ErrorCode::io,
                                       .path = {},
                                       .offset = 0,
                                       .message = "callback failure",
                                   });
                               }
                               return {};
                           });

    REQUIRE_FALSE(result);
    REQUIRE(callbackCount == callbackFailureInvocation);
    REQUIRE(result.error().message == "callback failure");
}

TEST_CASE("readBlocks_Should_ReportMissingBlock_If_RegionIncludesUnavailableData")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path, 2);
    const auto reader = uomul::MapReader::open(path, dimensions, {.recoverTruncatedMap = true});
    REQUIRE(reader);

    const auto result =
        reader->readBlocks({.min = {.x = 1, .y = 0}, .maxExclusive = {.x = 2, .y = 1}},
                           [](uomul::BlockCoord, const uomul::MapBlock &) -> std::expected<void, uomul::Error> { return {}; });

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("readRawBlocks_Should_PreserveHeaders_If_BlocksAreRead")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);
    std::uint32_t callbackCount = 0;

    const auto result = reader->readRawBlocks(
        {.min = {.x = 0, .y = 0}, .maxExclusive = {.x = testRegionEnd, .y = testRegionEnd}},
        [&callbackCount](uomul::BlockCoord coordinate, const uomul::MapBlockBytes & bytes) -> std::expected<void, uomul::Error>
        {
            const auto blockId = (coordinate.x * blockGridSize) + coordinate.y;
            for (std::size_t byte = 0; byte < landHeaderByteCount; byte++)
            {
                REQUIRE(std::to_integer<std::uint8_t>(bytes.at(byte)) == blockId + byte + 1);
            }
            callbackCount++;
            return {};
        });

    REQUIRE(result);
    REQUIRE(callbackCount == testRegionEnd * testRegionEnd);
}

TEST_CASE("readRawBlocks_Should_ReturnSuccessWithoutCallbacks_If_RegionIsEmpty")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);
    std::uint32_t callbackCount = 0;

    const auto result = reader->readRawBlocks(
        {.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 0, .y = 2}},
        [&callbackCount](uomul::BlockCoord, const uomul::MapBlockBytes &) -> std::expected<void, uomul::Error>
        {
            callbackCount++;
            return {};
        });

    REQUIRE(result);
    REQUIRE(callbackCount == 0);
}

TEST_CASE("readRawBlocks_Should_RejectRequest_If_RegionIsOutOfBounds")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);
    const auto callback = [](uomul::BlockCoord, const uomul::MapBlockBytes &) -> std::expected<void, uomul::Error>
    { return {}; };

    const auto result =
        reader->readRawBlocks({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 0, .y = blockGridSize + 1}}, callback);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::invalidCoordinate);
}

TEST_CASE("readRawBlocks_Should_RejectRequest_If_CallbackIsEmpty")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    const auto result = reader->readRawBlocks({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 1, .y = 1}}, {});

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::io);
}

TEST_CASE("readRawBlocks_Should_PropagateCallbackError_If_CallbackFails")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    const auto result = reader->readRawBlocks(
        {.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 1, .y = 1}},
        [](uomul::BlockCoord, const uomul::MapBlockBytes &) -> std::expected<void, uomul::Error>
        {
            return std::unexpected(
                uomul::Error{.code = uomul::ErrorCode::io, .path = {}, .offset = 0, .message = "raw callback failure"});
        });

    REQUIRE_FALSE(result);
    REQUIRE(result.error().message == "raw callback failure");
}

TEST_CASE("readRawBlocks_Should_ReportMissingBlock_If_RegionIncludesUnavailableData")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path, 2);
    const auto reader = uomul::MapReader::open(path, dimensions, {.recoverTruncatedMap = true});
    REQUIRE(reader);

    const auto result = reader->readRawBlocks(
        {.min = {.x = 1, .y = 0}, .maxExclusive = {.x = 2, .y = 1}},
        [](uomul::BlockCoord, const uomul::MapBlockBytes &) -> std::expected<void, uomul::Error> { return {}; });

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("readTiles_Should_ReadHalfOpenEdgeRegion_If_RegionTouchesMapBoundary")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    const auto result = readTilesInRegion(*reader,
                                          {
                                              .min = {.x = edgeStartTile, .y = edgeStartTile},
                                              .maxExclusive = {.x = dimensions.widthTiles, .y = dimensions.heightTiles},
                                          });

    REQUIRE(result);
    REQUIRE(result->size() == edgeRegionWidth * edgeRegionWidth);
    REQUIRE(result->at(0).x == edgeStartTile);
    REQUIRE(result->at(0).y == edgeStartTile);
    REQUIRE(result->at(edgeRegionWidth).x == edgeStartTile + 1);
    REQUIRE(result->at(edgeRegionWidth).y == edgeStartTile);
    REQUIRE(result->back().x == dimensions.widthTiles - 1);
    REQUIRE(result->back().y == dimensions.heightTiles - 1);
}

TEST_CASE("readTiles_Should_ReturnEmptyRegion_If_RegionStartsAtMapBoundary")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    const auto result = readTilesInRegion(*reader,
                                          {
                                              .min = {.x = dimensions.widthTiles, .y = dimensions.heightTiles},
                                              .maxExclusive = {.x = dimensions.widthTiles, .y = dimensions.heightTiles},
                                          });

    REQUIRE(result);
    REQUIRE(result->empty());
}

TEST_CASE("readTiles_Should_RejectRequest_If_RegionIsReversedOrOutOfBounds")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);
    const auto callback = [](uomul::TileCoord, const uomul::MapTile &) -> std::expected<void, uomul::Error> { return {}; };

    const auto reversedX = reader->readTiles({.min = {.x = 2, .y = 0}, .maxExclusive = {.x = 1, .y = 1}}, callback);
    const auto reversedY = reader->readTiles({.min = {.x = 0, .y = 2}, .maxExclusive = {.x = 1, .y = 1}}, callback);
    const auto outsideX =
        reader->readTiles({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = dimensions.widthTiles + 1, .y = 1}}, callback);
    const auto outsideY =
        reader->readTiles({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 1, .y = dimensions.heightTiles + 1}}, callback);

    requireInvalidCoordinate(reversedX);
    requireInvalidCoordinate(reversedY);
    requireInvalidCoordinate(outsideX);
    requireInvalidCoordinate(outsideY);
}

TEST_CASE("readTiles_Should_RejectRequest_If_CallbackIsEmpty")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    const auto result = reader->readTiles({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 1, .y = 1}}, {});

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::io);
}

TEST_CASE("readTiles_Should_PropagateCallbackError_If_CallbackFailsAfterCrossingBlockBoundary")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);
    std::uint32_t callbackCount = 0;

    const auto result = reader->readTiles(
        {.min = {.x = 7, .y = 0}, .maxExclusive = {.x = 9, .y = 1}},
        [&callbackCount](uomul::TileCoord, const uomul::MapTile &) -> std::expected<void, uomul::Error>
        {
            callbackCount++;
            if (callbackCount == 2)
            {
                return std::unexpected(
                    uomul::Error{.code = uomul::ErrorCode::io, .path = {}, .offset = 0, .message = "tile callback failure"});
            }
            return {};
        });

    REQUIRE_FALSE(result);
    REQUIRE(callbackCount == 2);
    REQUIRE(result.error().message == "tile callback failure");
}

TEST_CASE("readTiles_Should_ReportMissingBlock_If_RegionIncludesUnavailableData")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path, 1);
    const auto reader = uomul::MapReader::open(path, dimensions, {.recoverTruncatedMap = true});
    REQUIRE(reader);
    std::uint32_t callbackCount = 0;

    const auto result =
        reader->readTiles({.min = {.x = 7, .y = 0}, .maxExclusive = {.x = 9, .y = 1}},
                          [&callbackCount](uomul::TileCoord, const uomul::MapTile &) -> std::expected<void, uomul::Error>
                          {
                              callbackCount++;
                              return {};
                          });

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::truncatedInput);
    REQUIRE(callbackCount == 1);
}

TEST_CASE("stats_Should_ReportWholeMapTileIdAndElevationDistributions")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    const auto result = reader->stats();

    REQUIRE(result);
    REQUIRE(result->tileCount == 256);
    REQUIRE(result->tileIds.size() == 256);
    REQUIRE(result->zs.size() == 64);
    REQUIRE(result->tileIds.front().value == 0);
    REQUIRE(result->tileIds.front().count == 1);
    REQUIRE(result->tileIds.front().percentage == Catch::Approx(expectedSingleTilePercentage));
    REQUIRE(result->zs.front().value == -1);
    REQUIRE(result->zs.front().count == 4);
    REQUIRE(result->averageZ);
    REQUIRE(result->averageZ.value_or(std::numeric_limits<double>::quiet_NaN()) == Catch::Approx(expectedAverageElevation));
    REQUIRE(result->medianZ);
    REQUIRE(result->medianZ.value_or(std::numeric_limits<double>::quiet_NaN()) == Catch::Approx(expectedMedianElevation));
}

TEST_CASE("stats_Should_UseExactTileOrBlockRegionAndLeaveEmptyStatisticsUnset")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    const auto statsTiles = reader->statsTiles({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 2, .y = 2}});
    const auto statsBlocks = reader->statsBlocks({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 1, .y = 1}});
    const auto emptyStats = reader->statsTiles({.min = {.x = 3, .y = 4}, .maxExclusive = {.x = 3, .y = 4}});

    REQUIRE(statsTiles);
    REQUIRE(statsTiles->tileCount == 4);
    REQUIRE(statsTiles->tileIds.size() == 4);
    REQUIRE(statsTiles->zs.size() == 4);
    REQUIRE(statsTiles->zs.front().value == -1);
    REQUIRE(statsTiles->zs.front().count == 1);
    REQUIRE(statsBlocks);
    REQUIRE(statsBlocks->tileCount == uomul::mapBlockTileCount);
    REQUIRE(statsBlocks->tileIds.size() == uomul::mapBlockTileCount);
    REQUIRE(emptyStats);
    REQUIRE(emptyStats->tileCount == 0);
    REQUIRE(emptyStats->tileIds.empty());
    REQUIRE(emptyStats->zs.empty());
    REQUIRE_FALSE(emptyStats->averageZ);
    REQUIRE_FALSE(emptyStats->medianZ);
}

TEST_CASE("facetDimensions_Should_ProvideKnownPresets_If_FacetNameIsKnown")
{
    REQUIRE(uomul::facetDimensions("felucca") == uomul::Dimensions{.widthTiles = 6144, .heightTiles = 4096});
    REQUIRE(uomul::facetDimensions("tokuno") == uomul::Dimensions{.widthTiles = 1448, .heightTiles = 1448});
    REQUIRE_FALSE(uomul::facetDimensions("unknown"));
}
