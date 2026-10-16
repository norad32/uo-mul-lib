#include "map_test_helpers.h"
#include "uomul/map.h"
#include <array>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <utility>
#include <vector>

namespace
{

using uomul::test::dimensions;
using uomul::test::TestDirectory;
using uomul::test::writeFixture;

constexpr std::uint32_t blockGridSize = dimensions.widthTiles / uomul::landBlockDimension;
constexpr std::uint32_t invalidTileDimension = uomul::landBlockDimension - 1;
constexpr std::uint32_t edgeStartTile = uomul::landBlockDimension - 1;
constexpr std::uint32_t edgeRegionWidth = dimensions.widthTiles - edgeStartTile;
constexpr std::uint32_t callbackFailureInvocation = 3;
constexpr std::uint32_t testRegionEnd = 2;
constexpr std::size_t landHeaderByteCount = 4;
constexpr uomul::Dimensions partialEdgeDimensions{.widthTiles = 17, .heightTiles = 9};
constexpr uomul::Dimensions truncatedMapDimensions{.widthTiles = 32, .heightTiles = 16};
constexpr std::uint32_t partialEdgeFixtureBlockCount = 6;

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
    const auto result = reader.readLandTiles(
        region,
        [&coordinates](uomul::TileCoord coordinate, const uomul::LandTile &) -> std::expected<void, uomul::Error>
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
    REQUIRE(reader->readLandBlock({0, 1}));
    const auto missing = reader->readLandBlock({1, 0});
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
    writeFixture(path, 5);

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
    REQUIRE(reader->readLandBlock({2, 1}));
    REQUIRE_FALSE(reader->readLandBlock({3, 0}));
}

TEST_CASE("moveConstructor_Should_KeepReaderUsable_If_ReaderIsMoved")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    auto opened = uomul::MapReader::open(path, dimensions);
    REQUIRE(opened);

    uomul::MapReader reader(std::move(*opened));

    REQUIRE(reader.dimensions() == dimensions);
    REQUIRE(reader.readLandTile({0, 0})->id == 0);
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
    REQUIRE(first->readLandTile({0, 0}));
    REQUIRE_FALSE(first->readLandTile({8, 0}));
}

TEST_CASE("open_Should_RejectDirectory_If_RecoveryIsEnabled")
{
    const TestDirectory directory;
    constexpr uomul::Dimensions directoryDimensions{.widthTiles = 65536, .heightTiles = 65536};

    const auto reader = uomul::MapReader::open(directory.path(), directoryDimensions, {.recoverTruncatedMap = true});

    REQUIRE_FALSE(reader);
    REQUIRE(reader.error().code == uomul::ErrorCode::io);
}

TEST_CASE("readLandBlock_Should_DecodeTileIdsAndSignedElevations_If_BlockIsAvailable")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    const auto block = reader->readLandBlock({0, 0});

    REQUIRE(block);
    REQUIRE(block->at(0).id == 0);
    REQUIRE(block->at(0).z == -1);
    REQUIRE(block->at(1).id == 1);
    REQUIRE(block->at(1).z == 1);
    REQUIRE(block->at(63).id == 63);
}

TEST_CASE("readLandBlock_Should_RejectCoordinates_If_BlockIsOutsideMap")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    const auto outsideX = reader->readLandBlock({.x = blockGridSize, .y = 0});
    const auto outsideY = reader->readLandBlock({.x = 0, .y = blockGridSize});

    REQUIRE_FALSE(outsideX);
    REQUIRE(outsideX.error().code == uomul::ErrorCode::invalidCoordinate);
    REQUIRE_FALSE(outsideY);
    REQUIRE(outsideY.error().code == uomul::ErrorCode::invalidCoordinate);
}

TEST_CASE("readLandBlock_Should_ReportReadFailure_If_FileIsTruncatedAfterOpening")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);
    std::filesystem::resize_file(path, 0);

    const auto block = reader->readLandBlock({0, 0});

    REQUIRE_FALSE(block);
    REQUIRE(block.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("readLandTile_Should_DecodeColumnMajorBlocksAndSignedElevation_If_TileIsAvailable")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    const auto tile = reader->readLandTile({.x = 8, .y = 0});
    REQUIRE(tile);
    REQUIRE(tile->id == 200);
    REQUIRE(tile->z == -1);
    const auto southern = reader->readLandTile({.x = 0, .y = uomul::landBlockDimension});
    REQUIRE(southern);
    REQUIRE(southern->id == 100);
}

TEST_CASE("readLandTile_Should_RejectCoordinates_If_TileIsOutsideMap")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    const auto outsideX = reader->readLandTile({.x = dimensions.widthTiles, .y = 0});
    const auto outsideY = reader->readLandTile({.x = 0, .y = dimensions.heightTiles});

    REQUIRE_FALSE(outsideX);
    REQUIRE(outsideX.error().code == uomul::ErrorCode::invalidCoordinate);
    REQUIRE_FALSE(outsideY);
    REQUIRE(outsideY.error().code == uomul::ErrorCode::invalidCoordinate);
}

TEST_CASE("readLandTile_Should_RejectPartialBlock_If_BlockIsTruncatedAfterOpening")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);
    std::filesystem::resize_file(path, (2U * uomul::landBlockByteCount) + 2U);

    const auto tile = reader->readLandTile({.x = uomul::landBlockDimension, .y = 0});

    REQUIRE_FALSE(tile);
    REQUIRE(tile.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("readLandTile_Should_ReportReadFailure_If_BlockIsTruncatedAfterOpening")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);
    std::filesystem::resize_file(path, 0);

    const auto tile = reader->readLandTile({.x = 1, .y = 1});

    REQUIRE_FALSE(tile);
    REQUIRE(tile.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("readLandTile_Should_ReportMissingBlock_If_TileIsInsideRecoveredDimensions")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path, 2);
    const auto reader = uomul::MapReader::open(path, dimensions, {.recoverTruncatedMap = true});
    REQUIRE(reader);

    const auto tile = reader->readLandTile({.x = 8, .y = 0});

    REQUIRE_FALSE(tile);
    REQUIRE(tile.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("readLandBlocks_Should_VisitHalfOpenRegionInBlockOrder_If_RegionIsValid")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);
    std::vector<uomul::BlockCoord> coordinates;

    const auto result = reader->readLandBlocks(
        {.min = {.x = 0, .y = 0}, .maxExclusive = {.x = testRegionEnd, .y = testRegionEnd}},
        [&coordinates](uomul::BlockCoord coordinate, const uomul::LandBlock &) -> std::expected<void, uomul::Error>
        {
            coordinates.push_back(coordinate);
            return {};
        });

    REQUIRE(result);
    REQUIRE(coordinates.size() == 4);
    REQUIRE(coordinates.at(0).x == 0);
    REQUIRE(coordinates.at(0).y == 0);
    REQUIRE(coordinates.at(1).x == 0);
    REQUIRE(coordinates.at(1).y == 1);
    REQUIRE(coordinates.at(2).x == 1);
    REQUIRE(coordinates.at(2).y == 0);
    REQUIRE(coordinates.at(3).x == 1);
    REQUIRE(coordinates.at(3).y == 1);
}

TEST_CASE("readLandBlocks_Should_ReturnSuccessWithoutCallbacks_If_RegionIsEmpty")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    const auto result = reader->readLandBlocks(
        {.min = {0, 0}, .maxExclusive = {0, 2}},
        [](uomul::BlockCoord, const uomul::LandBlock &) -> std::expected<void, uomul::Error> { return {}; });

    REQUIRE(result);
}

TEST_CASE("readLandBlocks_Should_RejectRequest_If_RegionIsReversedOrOutOfBounds")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);
    const auto callback = [](uomul::BlockCoord, const uomul::LandBlock &) -> std::expected<void, uomul::Error> { return {}; };

    const auto reversedX = reader->readLandBlocks({.min = {1, 0}, .maxExclusive = {0, 1}}, callback);
    const auto reversedY = reader->readLandBlocks({.min = {0, 1}, .maxExclusive = {1, 0}}, callback);
    const auto outside = reader->readLandBlocks({.min = {0, 0}, .maxExclusive = {0, blockGridSize + 1}}, callback);

    REQUIRE_FALSE(reversedX);
    REQUIRE(reversedX.error().code == uomul::ErrorCode::invalidCoordinate);
    REQUIRE_FALSE(reversedY);
    REQUIRE(reversedY.error().code == uomul::ErrorCode::invalidCoordinate);
    REQUIRE_FALSE(outside);
    REQUIRE(outside.error().code == uomul::ErrorCode::invalidCoordinate);
}

TEST_CASE("readLandBlocks_Should_RejectRequest_If_CallbackIsEmpty")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    const auto result = reader->readLandBlocks({.min = {0, 0}, .maxExclusive = {1, 1}}, {});

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::io);
}

TEST_CASE("readLandBlocks_Should_PropagateCallbackError_If_CallbackFails")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);
    std::uint32_t callbackCount = 0;

    const auto result = reader->readLandBlocks(
        {.min = {.x = 0, .y = 0}, .maxExclusive = {.x = testRegionEnd, .y = testRegionEnd}},
        [&callbackCount](uomul::BlockCoord, const uomul::LandBlock &) -> std::expected<void, uomul::Error>
        {
            if (++callbackCount == callbackFailureInvocation)
            {
                return std::unexpected(
                    uomul::Error{.code = uomul::ErrorCode::io, .path = {}, .offset = 0, .message = "callback failure"});
            }
            return {};
        });

    REQUIRE_FALSE(result);
    REQUIRE(callbackCount == callbackFailureInvocation);
    REQUIRE(result.error().message == "callback failure");
}

TEST_CASE("readLandBlocks_Should_ReportMissingBlock_If_RegionIncludesUnavailableData")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path, 2);
    const auto reader = uomul::MapReader::open(path, dimensions, {.recoverTruncatedMap = true});
    REQUIRE(reader);

    const auto result = reader->readLandBlocks(
        {.min = {.x = 1, .y = 0}, .maxExclusive = {.x = 2, .y = 1}},
        [](uomul::BlockCoord, const uomul::LandBlock &) -> std::expected<void, uomul::Error> { return {}; });

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("readRawLandBlocks_Should_PreserveHeaders_If_BlocksAreRead")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);
    std::uint32_t callbackCount = 0;

    const auto result = reader->readRawLandBlocks(
        {.min = {.x = 0, .y = 0}, .maxExclusive = {.x = testRegionEnd, .y = testRegionEnd}},
        [&callbackCount](uomul::BlockCoord coordinate, const uomul::LandBlockBytes & bytes) -> std::expected<void, uomul::Error>
        {
            const auto blockId = (coordinate.x * blockGridSize) + coordinate.y;
            for (std::size_t byte = 0; byte < landHeaderByteCount; ++byte)
            {
                REQUIRE(std::to_integer<std::uint8_t>(bytes.at(byte)) == blockId + byte + 1);
            }
            ++callbackCount;
            return {};
        });

    REQUIRE(result);
    REQUIRE(callbackCount == testRegionEnd * testRegionEnd);
}

TEST_CASE("readRawLandBlocks_Should_ReturnSuccessWithoutCallbacks_If_RegionIsEmpty")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);
    std::uint32_t callbackCount = 0;

    const auto result = reader->readRawLandBlocks(
        {.min = {0, 0}, .maxExclusive = {0, 2}},
        [&callbackCount](uomul::BlockCoord, const uomul::LandBlockBytes &) -> std::expected<void, uomul::Error>
        {
            ++callbackCount;
            return {};
        });

    REQUIRE(result);
    REQUIRE(callbackCount == 0);
}

TEST_CASE("readRawLandBlocks_Should_RejectRequest_If_RegionIsOutOfBounds")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);
    const auto callback = [](uomul::BlockCoord, const uomul::LandBlockBytes &) -> std::expected<void, uomul::Error>
    { return {}; };

    const auto result = reader->readRawLandBlocks({.min = {0, 0}, .maxExclusive = {0, blockGridSize + 1}}, callback);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::invalidCoordinate);
}

TEST_CASE("readRawLandBlocks_Should_RejectRequest_If_CallbackIsEmpty")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    const auto result = reader->readRawLandBlocks({.min = {0, 0}, .maxExclusive = {1, 1}}, {});

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::io);
}

TEST_CASE("readRawLandBlocks_Should_PropagateCallbackError_If_CallbackFails")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    const auto result = reader->readRawLandBlocks(
        {.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 1, .y = 1}},
        [](uomul::BlockCoord, const uomul::LandBlockBytes &) -> std::expected<void, uomul::Error>
        {
            return std::unexpected(
                uomul::Error{.code = uomul::ErrorCode::io, .path = {}, .offset = 0, .message = "raw callback failure"});
        });

    REQUIRE_FALSE(result);
    REQUIRE(result.error().message == "raw callback failure");
}

TEST_CASE("readRawLandBlocks_Should_ReportMissingBlock_If_RegionIncludesUnavailableData")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path, 2);
    const auto reader = uomul::MapReader::open(path, dimensions, {.recoverTruncatedMap = true});
    REQUIRE(reader);

    const auto result = reader->readRawLandBlocks(
        {.min = {.x = 1, .y = 0}, .maxExclusive = {.x = 2, .y = 1}},
        [](uomul::BlockCoord, const uomul::LandBlockBytes &) -> std::expected<void, uomul::Error> { return {}; });

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("readLandTiles_Should_ReadHalfOpenEdgeRegion_If_RegionTouchesMapBoundary")
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

TEST_CASE("readLandTiles_Should_ReturnEmptyRegion_If_RegionStartsAtMapBoundary")
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

TEST_CASE("readLandTiles_Should_RejectRequest_If_RegionIsReversedOrOutOfBounds")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);
    const auto callback = [](uomul::TileCoord, const uomul::LandTile &) -> std::expected<void, uomul::Error> { return {}; };

    const auto reversedX = reader->readLandTiles({.min = {2, 0}, .maxExclusive = {1, 1}}, callback);
    const auto reversedY = reader->readLandTiles({.min = {0, 2}, .maxExclusive = {1, 1}}, callback);
    const auto outsideX = reader->readLandTiles({.min = {0, 0}, .maxExclusive = {dimensions.widthTiles + 1, 1}}, callback);
    const auto outsideY = reader->readLandTiles({.min = {0, 0}, .maxExclusive = {1, dimensions.heightTiles + 1}}, callback);

    REQUIRE_FALSE(reversedX);
    REQUIRE(reversedX.error().code == uomul::ErrorCode::invalidCoordinate);
    REQUIRE_FALSE(reversedY);
    REQUIRE(reversedY.error().code == uomul::ErrorCode::invalidCoordinate);
    REQUIRE_FALSE(outsideX);
    REQUIRE(outsideX.error().code == uomul::ErrorCode::invalidCoordinate);
    REQUIRE_FALSE(outsideY);
    REQUIRE(outsideY.error().code == uomul::ErrorCode::invalidCoordinate);
}

TEST_CASE("readLandTiles_Should_RejectRequest_If_CallbackIsEmpty")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    const auto result = reader->readLandTiles({.min = {0, 0}, .maxExclusive = {1, 1}}, {});

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::io);
}

TEST_CASE("readLandTiles_Should_PropagateCallbackError_If_CallbackFailsAfterCrossingBlockBoundary")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);
    std::uint32_t callbackCount = 0;

    const auto result = reader->readLandTiles(
        {.min = {.x = 7, .y = 0}, .maxExclusive = {.x = 9, .y = 1}},
        [&callbackCount](uomul::TileCoord, const uomul::LandTile &) -> std::expected<void, uomul::Error>
        {
            if (++callbackCount == 2)
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

TEST_CASE("readLandTiles_Should_ReportMissingBlock_If_RegionIncludesUnavailableData")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path, 1);
    const auto reader = uomul::MapReader::open(path, dimensions, {.recoverTruncatedMap = true});
    REQUIRE(reader);
    std::uint32_t callbackCount = 0;

    const auto result =
        reader->readLandTiles({.min = {.x = 7, .y = 0}, .maxExclusive = {.x = 9, .y = 1}},
                              [&callbackCount](uomul::TileCoord, const uomul::LandTile &) -> std::expected<void, uomul::Error>
                              {
                                  ++callbackCount;
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
    REQUIRE(result->tileIds.front().percentage == Catch::Approx(100.0 / 256.0));
    REQUIRE(result->zs.front().value == -1);
    REQUIRE(result->zs.front().count == 4);
    REQUIRE(result->averageZ);
    REQUIRE(*result->averageZ == Catch::Approx(31.484375));
    REQUIRE(result->medianZ);
    REQUIRE(*result->medianZ == Catch::Approx(31.5));
}

TEST_CASE("stats_Should_UseExactTileOrBlockRegionAndLeaveEmptyStatisticsUnset")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    const auto tileStats = reader->tileStats({.min = {0, 0}, .maxExclusive = {2, 2}});
    const auto blockStats = reader->blockStats({.min = {0, 0}, .maxExclusive = {1, 1}});
    const auto emptyStats = reader->tileStats({.min = {3, 4}, .maxExclusive = {3, 4}});

    REQUIRE(tileStats);
    REQUIRE(tileStats->tileCount == 4);
    REQUIRE(tileStats->tileIds.size() == 4);
    REQUIRE(tileStats->zs.size() == 4);
    REQUIRE(tileStats->zs.front().value == -1);
    REQUIRE(tileStats->zs.front().count == 1);
    REQUIRE(blockStats);
    REQUIRE(blockStats->tileCount == uomul::landBlockTileCount);
    REQUIRE(blockStats->tileIds.size() == uomul::landBlockTileCount);
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
