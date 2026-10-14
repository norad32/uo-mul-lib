#include "uomul/map.h"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <vector>

namespace
{

class TestDirectory
{
public:
    TestDirectory()
    {
        static std::uint64_t sequence = 0;
        path_ = std::filesystem::temp_directory_path() / ("uomul-test-" + std::to_string(++sequence));
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }

    ~TestDirectory() { std::filesystem::remove_all(path_); }
    [[nodiscard]] const std::filesystem::path & path() const { return path_; }

private:
    std::filesystem::path path_;
};

void write_fixture(const std::filesystem::path & path, std::uint32_t blocks = 4)
{
    std::ofstream output(path, std::ios::binary);
    for (std::uint32_t block = 0; block < blocks; ++block)
    {
        const std::array<char, 4> header{static_cast<char>(block + 1),
                                         static_cast<char>(block + 2),
                                         static_cast<char>(block + 3),
                                         static_cast<char>(block + 4)};
        output.write(header.data(), static_cast<std::streamsize>(header.size()));
        for (std::uint32_t tile = 0; tile < 64; ++tile)
        {
            const auto id = static_cast<std::uint16_t>(block * 100 + tile);
            const std::array<char, 3> record{
                static_cast<char>(id & 0xffU), static_cast<char>(id >> 8), static_cast<char>(tile == 0 ? 0xff : tile)};
            output.write(record.data(), static_cast<std::streamsize>(record.size()));
        }
    }
}

[[nodiscard]] std::string read_bytes(const std::filesystem::path & path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

[[nodiscard]] std::expected<std::vector<uomul::TileCoord>, uomul::Error> readTilesInRegion(const uomul::MapReader & reader,
                                                                                           uomul::TileRegion region)
{
    std::vector<uomul::TileCoord> coordinates;
    auto result = reader.readLandTiles(
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

constexpr uomul::Dimensions dimensions{.widthTiles = 16, .heightTiles = 16};
constexpr std::uint32_t blockGridSize = dimensions.widthTiles / uomul::landBlockDimension;
constexpr std::uint32_t invalidTileDimension = uomul::landBlockDimension - 1;
constexpr std::uint32_t edgeStartTile = uomul::landBlockDimension - 1;
constexpr std::uint32_t edgeRegionWidth = dimensions.widthTiles - edgeStartTile;
constexpr std::uint32_t callbackFailureInvocation = 3;
constexpr std::uint32_t testRegionEnd = 2;
constexpr std::size_t landHeaderByteCount = 4;
constexpr std::size_t mapBlockIdByteCount = 4;
constexpr std::uint32_t rectangularWidthBlocks = 3;
constexpr std::uint32_t rectangularWidthTiles = rectangularWidthBlocks * uomul::landBlockDimension;
constexpr std::uint32_t rectangularBlockCount = rectangularWidthBlocks * blockGridSize;
constexpr std::uint32_t secondBlockCoordinate = blockGridSize;
constexpr std::uint32_t expectedChangedBlockId = secondBlockCoordinate * blockGridSize;
constexpr std::uint32_t secondTileCoordinate = testRegionEnd;
constexpr std::uint32_t firstTileCoordinate = testRegionEnd - 1;
constexpr std::uint16_t changedTileId = 1234;
constexpr std::uint16_t stagedTileId = 777;
constexpr std::uint16_t editedTileId = 778;
constexpr std::uint16_t stagedBlockTileId = 999;
constexpr std::int8_t changedTileElevation = -20;
constexpr std::int8_t stagedTileElevation = -12;

} // namespace

TEST_CASE("MapReader_Should_DecodeColumnMajorBlocksAndSignedElevation")
{
    TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    write_fixture(path);
    auto reader = uomul::MapReader::open(path, dimensions);

    REQUIRE(reader);
    REQUIRE(reader->completeBlocks() == 4);
    auto tile = reader->readLandTile({8, 0});
    REQUIRE(tile);
    REQUIRE(tile->id == 200);
    REQUIRE(tile->z == -1);
    auto southern = reader->readLandTile({.x = 0, .y = uomul::landBlockDimension});
    REQUIRE(southern);
    REQUIRE(southern->id == 100);
}

TEST_CASE("MapReader_Should_ValidateSizeAndRecoverCompletePrefix")
{
    TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    write_fixture(path, 2);
    REQUIRE_FALSE(uomul::MapReader::open(path, dimensions));
    auto reader = uomul::MapReader::open(path, dimensions, {.recoverTruncatedMap = true});
    REQUIRE(reader);
    REQUIRE(reader->completeBlocks() == 2);
    REQUIRE(reader->readLandBlock({0, 1}));
    REQUIRE_FALSE(reader->readLandBlock({1, 0}));
}

TEST_CASE("MapReader_Should_RejectOversizedMapEvenInRecoveryMode")
{
    TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    write_fixture(path);
    {
        std::ofstream output(path, std::ios::binary | std::ios::app);
        output.put('x');
    }
    REQUIRE_FALSE(uomul::MapReader::open(path, dimensions, {.recoverTruncatedMap = true}));
}

TEST_CASE("MapReader_Should_RejectPartialBlockByDefaultAndReportRecoveryBytes")
{
    TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    write_fixture(path);
    {
        std::ofstream output(path, std::ios::binary | std::ios::app);
        output.put('x');
    }
    REQUIRE_FALSE(uomul::MapReader::open(path, dimensions));
    write_fixture(path, 3);
    {
        std::ofstream output(path, std::ios::binary | std::ios::app);
        output.put('x');
    }
    auto reader = uomul::MapReader::open(path, dimensions, {.recoverTruncatedMap = true});
    REQUIRE(reader);
    REQUIRE(reader->ignoredTrailingBytes() == 1);
}

TEST_CASE("MapEditor_Should_PreserveHeadersAndPublishOnlyOnCommit")
{
    TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    write_fixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);
    REQUIRE(editor->fill({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 2, .y = 2}}, {.id = 777, .z = -12}));
    REQUIRE_FALSE(std::filesystem::exists(output));
    REQUIRE(editor->commit());
    const auto original = read_bytes(source);
    const auto modified = read_bytes(output);
    REQUIRE(modified.size() == original.size());
    REQUIRE(modified.substr(0, 4) == original.substr(0, 4));
    auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readLandTile({1, 1})->id == 777);
    REQUIRE(reader->readLandTile({2, 2})->id == 18);
}

TEST_CASE("MapEditor_Should_AbortWithoutPublishingAndKeepSourceUntouched")
{
    TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    write_fixture(source);
    const auto original = read_bytes(source);
    {
        auto editor = uomul::MapEditor::create(source, output, dimensions);
        REQUIRE(editor);
        REQUIRE(editor->writeLandTile({0, 0}, {.id = 900, .z = 3}));
    }
    REQUIRE_FALSE(std::filesystem::exists(output));
    REQUIRE(read_bytes(source) == original);
}

TEST_CASE("MapEditor_Should_RejectInPlaceEditing")
{
    TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    write_fixture(source);
    REQUIRE_FALSE(uomul::MapEditor::create(source, source, dimensions, {.overwriteOutput = true}));
}

TEST_CASE("MapEditor_Should_KeepFailedRegionCallbackAllOrNothing")
{
    TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    write_fixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);
    const auto failed = editor->editLandTiles(
        {.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 16, .y = 16}},
        [](uomul::TileCoord coordinate, const uomul::LandTile & tile) -> std::expected<uomul::LandTile, uomul::Error>
        {
            if (coordinate.x == 8 && coordinate.y == 8)
            {
                return std::unexpected(uomul::Error{uomul::ErrorCode::io, {}, 0, "test failure"});
            }
            return uomul::LandTile{.id = 999, .z = tile.z};
        });
    REQUIRE_FALSE(failed);
    REQUIRE(editor->commit());
    auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readLandTile({.x = 0, .y = 0})->id == 0);
}

TEST_CASE("MapDiff_Should_RejectDuplicateAndInvalidBlockIDs")
{
    TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto output = directory.path() / "output.mul";
    const auto diff_directory = directory.path() / "diff";
    std::filesystem::create_directories(diff_directory);
    write_fixture(base);
    {
        std::ofstream data(diff_directory / "mapdif0.mul", std::ios::binary);
        std::ofstream index(diff_directory / "mapdifl0.mul", std::ios::binary);
        std::array<char, 196> block{};
        data.write(block.data(), static_cast<std::streamsize>(block.size()));
        const std::array<char, 8> ids{0, 0, 0, 0, 0, 0, 0, 0};
        index.write(ids.data(), static_cast<std::streamsize>(ids.size()));
    }
    REQUIRE_FALSE(uomul::MapDiff::apply(base, diff_directory, output, 0, dimensions));
    REQUIRE_FALSE(std::filesystem::exists(output));
}

TEST_CASE("MapDiff_Should_GenerateApplyAndReportLandOnlyChanges")
{
    TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto target = directory.path() / "target.mul";
    const auto applied = directory.path() / "applied.mul";
    const auto diff_directory = directory.path() / "diff";
    const auto report_path = directory.path() / "report.csv";
    std::filesystem::create_directories(diff_directory);
    write_fixture(base);
    write_fixture(target);
    auto edit = uomul::MapEditor::create(target, directory.path() / "target-edited.mul", dimensions);
    REQUIRE(edit);
    REQUIRE(edit->writeLandTile({9, 9}, {.id = 1234, .z = -20}));
    REQUIRE(edit->commit());
    const auto edited_target = directory.path() / "target-edited.mul";

    auto generated = uomul::MapDiff::generate(base, edited_target, diff_directory, 2, dimensions);
    REQUIRE(generated);
    REQUIRE(generated->landBlocksChanged == 1);
    REQUIRE(generated->landTilesChanged == 1);
    REQUIRE(uomul::MapDiff::apply(base, diff_directory, applied, 2, dimensions));
    REQUIRE(read_bytes(applied) == read_bytes(edited_target));
    auto report = uomul::MapDiff::report(base, edited_target, report_path, dimensions);
    REQUIRE(report);
    REQUIRE(report->landTilesChanged == 1);
    REQUIRE(read_bytes(report_path).find("3,1,1,1") != std::string::npos);
}

TEST_CASE("open_Should_RejectMap_If_DimensionsAreZeroOrNotMultiplesOfEight")
{
    TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    const std::array<uomul::Dimensions, 4> invalidDimensions{
        uomul::Dimensions{.widthTiles = 0, .heightTiles = dimensions.heightTiles},
        uomul::Dimensions{.widthTiles = dimensions.widthTiles, .heightTiles = 0},
        uomul::Dimensions{.widthTiles = invalidTileDimension, .heightTiles = dimensions.heightTiles},
        uomul::Dimensions{.widthTiles = dimensions.widthTiles, .heightTiles = invalidTileDimension},
    };

    for (const auto dimensionsToCheck : invalidDimensions)
    {
        auto reader = uomul::MapReader::open(path, dimensionsToCheck);
        REQUIRE_FALSE(reader);
        REQUIRE(reader.error().code == uomul::ErrorCode::invalidDimensions);
    }
}

TEST_CASE("readLandTile_Should_RejectCoordinates_If_TileOrBlockIsOutOfBounds")
{
    TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    write_fixture(path);
    auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    auto tile = reader->readLandTile({.x = dimensions.widthTiles, .y = 0});
    REQUIRE_FALSE(tile);
    REQUIRE(tile.error().code == uomul::ErrorCode::invalidCoordinate);
    auto block = reader->readLandBlock({.x = blockGridSize, .y = 0});
    REQUIRE_FALSE(block);
    REQUIRE(block.error().code == uomul::ErrorCode::invalidCoordinate);
}

TEST_CASE("readLandTiles_Should_ReadEmptyRegion_If_RegionTouchesMapBoundary")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    write_fixture(path);
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
    write_fixture(path);
    const auto reader = uomul::MapReader::open(path, dimensions);
    REQUIRE(reader);

    const auto result = readTilesInRegion(*reader,
                                          {
                                              .min = {.x = edgeStartTile, .y = edgeStartTile},
                                              .maxExclusive = {.x = dimensions.widthTiles, .y = dimensions.heightTiles},
                                          });
    REQUIRE(result);
    REQUIRE(result->size() == edgeRegionWidth * edgeRegionWidth);
    REQUIRE((result->at(0).x == edgeStartTile && result->at(0).y == edgeStartTile &&
             result->at(edgeRegionWidth).x == edgeStartTile + 1 && result->at(edgeRegionWidth).y == edgeStartTile &&
             result->back().x == dimensions.widthTiles - 1 && result->back().y == dimensions.heightTiles - 1));
}

TEST_CASE("readLandBlocks_Should_PropagateCallbackError_If_CallbackFails")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    write_fixture(path);
    auto reader = uomul::MapReader::open(path, dimensions);
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
    REQUIRE(callbackCount == 3);
    REQUIRE(result.error().message == "callback failure");
}

TEST_CASE("readRawLandBlocks_Should_PreserveHeaders_If_BlocksAreRead")
{
    const TestDirectory directory;
    const auto path = directory.path() / "map.mul";
    write_fixture(path);
    auto reader = uomul::MapReader::open(path, dimensions);
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
    REQUIRE(callbackCount == 4);
}

TEST_CASE("writeLandTile_Should_RejectCoordinates_If_TileOrBlockIsOutOfBounds")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    write_fixture(source);
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
    write_fixture(source);
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

    auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readLandTile({.x = 0, .y = 0})->id == 0);
}

TEST_CASE("editLandTiles_Should_UseEarlierStagedValues_If_RegionIsEditedAgain")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    write_fixture(source);
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

    auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readLandTile({.x = firstTileCoordinate, .y = firstTileCoordinate}) ==
            uomul::LandTile{.id = editedTileId, .z = stagedTileElevation});
    REQUIRE(reader->readLandTile({.x = secondTileCoordinate, .y = secondTileCoordinate})->id == 18);
}

TEST_CASE("generate_Should_WriteEmptyDiff_If_MapsAreIdentical")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto target = directory.path() / "target.mul";
    const auto applied = directory.path() / "applied.mul";
    const auto diffDirectory = directory.path() / "diff";
    std::filesystem::create_directories(diffDirectory);
    write_fixture(base);
    write_fixture(target);

    const auto generated = uomul::MapDiff::generate(base, target, diffDirectory, 0, dimensions);
    REQUIRE(generated);
    REQUIRE(generated->landBlocksChanged == 0);
    REQUIRE(generated->landTilesChanged == 0);
    REQUIRE(std::filesystem::file_size(diffDirectory / "mapdif0.mul") == 0);
    REQUIRE(std::filesystem::file_size(diffDirectory / "mapdifl0.mul") == 0);
    REQUIRE(uomul::MapDiff::apply(base, diffDirectory, applied, 0, dimensions));
    REQUIRE(read_bytes(applied) == read_bytes(base));
}

TEST_CASE("generate_Should_PreserveColumnMajorBlockIDs_If_FacetIsRectangular")
{
    const TestDirectory directory;
    const uomul::Dimensions rectangularDimensions{.widthTiles = rectangularWidthTiles, .heightTiles = dimensions.heightTiles};
    const auto base = directory.path() / "base.mul";
    const auto target = directory.path() / "target.mul";
    const auto editedTarget = directory.path() / "edited-target.mul";
    const auto applied = directory.path() / "applied.mul";
    const auto diffDirectory = directory.path() / "diff";
    std::filesystem::create_directories(diffDirectory);
    write_fixture(base, rectangularBlockCount);
    write_fixture(target, rectangularBlockCount);
    auto editor = uomul::MapEditor::create(target, editedTarget, rectangularDimensions);
    REQUIRE(editor);
    REQUIRE(editor->writeLandTile({.x = secondBlockCoordinate * uomul::landBlockDimension, .y = 0},
                                  {.id = changedTileId, .z = changedTileElevation}));
    REQUIRE(editor->commit());

    auto generated = uomul::MapDiff::generate(base, editedTarget, diffDirectory, 0, rectangularDimensions);
    REQUIRE(generated);
    REQUIRE(generated->landBlocksChanged == 1);
    const std::array<char, mapBlockIdByteCount> expectedBlockId{static_cast<char>(expectedChangedBlockId), 0, 0, 0};
    REQUIRE(read_bytes(diffDirectory / "mapdifl0.mul") == std::string(expectedBlockId.data(), expectedBlockId.size()));
    REQUIRE(uomul::MapDiff::apply(base, diffDirectory, applied, 0, rectangularDimensions));
    REQUIRE(read_bytes(applied) == read_bytes(editedTarget));
}

TEST_CASE("FacetDimensions_Should_ProvideKnownPresets")
{
    REQUIRE(uomul::facetDimensions("felucca") == uomul::Dimensions{.widthTiles = 6144, .heightTiles = 4096});
    REQUIRE(uomul::facetDimensions("tokuno") == uomul::Dimensions{.widthTiles = 1448, .heightTiles = 1448});
    REQUIRE_FALSE(uomul::facetDimensions("unknown"));
}
