#include "map_test_helpers.h"
#include "uomul/map.h"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
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

TEST_CASE("MapReader_Should_DecodeColumnMajorBlocksAndSignedElevation")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);

    REQUIRE(reader);
    REQUIRE(reader->completeBlocks() == 4);
    const auto tile = reader->readLandTile({.x = 8, .y = 0});
    REQUIRE(tile);
    REQUIRE(tile->id == 200);
    REQUIRE(tile->z == -1);
    const auto southern = reader->readLandTile({.x = 0, .y = uomul::landBlockDimension});
    REQUIRE(southern);
    REQUIRE(southern->id == 100);
}

TEST_CASE("MapReader_Should_ValidateSizeAndRecoverCompletePrefix")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path, 2);
    REQUIRE_FALSE(uomul::MapReader::open(path, dimensions));
    const auto reader = uomul::MapReader::open(path, dimensions, {.recoverTruncatedMap = true});
    REQUIRE(reader);
    REQUIRE(reader->completeBlocks() == 2);
    REQUIRE(reader->readLandBlock({0, 1}));
    REQUIRE_FALSE(reader->readLandBlock({1, 0}));
}

TEST_CASE("MapReader_Should_RejectOversizedMapEvenInRecoveryMode")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    {
        std::ofstream output(path, std::ios::binary | std::ios::app);
        output.put('x');
    }
    REQUIRE_FALSE(uomul::MapReader::open(path, dimensions, {.recoverTruncatedMap = true}));
}

TEST_CASE("MapReader_Should_RejectPartialBlockByDefaultAndReportRecoveryBytes")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    {
        std::ofstream output(path, std::ios::binary | std::ios::app);
        output.put('x');
    }
    REQUIRE_FALSE(uomul::MapReader::open(path, dimensions));
    writeFixture(path, 3);
    {
        std::ofstream output(path, std::ios::binary | std::ios::app);
        output.put('x');
    }
    const auto reader = uomul::MapReader::open(path, dimensions, {.recoverTruncatedMap = true});
    REQUIRE(reader);
    REQUIRE(reader->ignoredTrailingBytes() == 1);
}

TEST_CASE("open_Should_RejectMap_If_DimensionsAreZeroOrNotMultiplesOfEight")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    const std::array<uomul::Dimensions, 4> invalidDimensions{
        uomul::Dimensions{.widthTiles = 0, .heightTiles = dimensions.heightTiles},
        uomul::Dimensions{.widthTiles = dimensions.widthTiles, .heightTiles = 0},
        uomul::Dimensions{.widthTiles = invalidTileDimension, .heightTiles = dimensions.heightTiles},
        uomul::Dimensions{.widthTiles = dimensions.widthTiles, .heightTiles = invalidTileDimension},
    };

    for (const auto dimensionsToCheck : invalidDimensions)
    {
        const auto reader = uomul::MapReader::open(path, dimensionsToCheck);
        REQUIRE_FALSE(reader);
        REQUIRE(reader.error().code == uomul::ErrorCode::invalidDimensions);
    }
}

TEST_CASE("readLandTile_Should_RejectCoordinates_If_TileOrBlockIsOutOfBounds")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    writeFixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    const auto tile = reader->readLandTile({.x = dimensions.widthTiles, .y = 0});
    REQUIRE_FALSE(tile);
    REQUIRE(tile.error().code == uomul::ErrorCode::invalidCoordinate);
    const auto block = reader->readLandBlock({.x = blockGridSize, .y = 0});
    REQUIRE_FALSE(block);
    REQUIRE(block.error().code == uomul::ErrorCode::invalidCoordinate);
}

TEST_CASE("readLandTiles_Should_ReadEmptyRegion_If_RegionTouchesMapBoundary")
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
            ++callbackCount;
            if (callbackCount == callbackFailureInvocation)
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

TEST_CASE("FacetDimensions_Should_ProvideKnownPresets")
{
    REQUIRE(uomul::facetDimensions("felucca") == uomul::Dimensions{.widthTiles = 6144, .heightTiles = 4096});
    REQUIRE(uomul::facetDimensions("tokuno") == uomul::Dimensions{.widthTiles = 1448, .heightTiles = 1448});
    REQUIRE_FALSE(uomul::facetDimensions("unknown"));
}
