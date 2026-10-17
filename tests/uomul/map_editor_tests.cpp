#include "map_test_helpers.h"
#include "uomul/map.h"
#include <catch2/catch_test_macros.hpp>
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
constexpr std::uint32_t testRegionEnd = 2;
constexpr std::uint16_t stagedTileId = 777;
constexpr std::uint16_t editedTileId = 778;
constexpr std::uint16_t stagedBlockTileId = 999;
constexpr std::int8_t stagedTileElevation = -12;
constexpr uomul::TileRegion smallTileRegion{.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 2, .y = 2}};

[[nodiscard]] std::expected<uomul::LandBlock, uomul::Error> acceptEditedBlock(uomul::BlockCoord, const uomul::LandBlock & block)
{
    return block;
}

[[nodiscard]] std::expected<uomul::LandTile, uomul::Error> acceptEditedTile(uomul::TileCoord, const uomul::LandTile & tile)
{
    return tile;
}

class CurrentPathGuard
{
public:
    explicit CurrentPathGuard(const std::filesystem::path & path) : original_(std::filesystem::current_path())
    {
        std::filesystem::current_path(path);
    }

    ~CurrentPathGuard() { std::filesystem::current_path(original_); }

private:
    std::filesystem::path original_;
};

} // namespace

TEST_CASE("create_Should_RejectMissingSource_If_SourceMapDoesNotExist")
{
    const TestDirectory directory;
    const auto output = directory.path() / "output.mul";

    const auto editor = uomul::MapEditor::create(directory.path() / "missing.mul", output, dimensions);

    REQUIRE_FALSE(editor);
    REQUIRE(editor.error().code == uomul::ErrorCode::io);
}

TEST_CASE("create_Should_RejectInvalidDimensions_If_DimensionsAreNotBlockAligned")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    writeFixture(source);

    const auto editor =
        uomul::MapEditor::create(source, directory.path() / "output.mul", {.widthTiles = 15, .heightTiles = 16});

    REQUIRE_FALSE(editor);
    REQUIRE(editor.error().code == uomul::ErrorCode::invalidDimensions);
}

TEST_CASE("create_Should_RejectInPlaceEditing_If_OutputNamesTheSource")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    writeFixture(source);

    const auto editor = uomul::MapEditor::create(source, source, dimensions, {.overwriteOutput = true});

    REQUIRE_FALSE(editor);
    REQUIRE(editor.error().code == uomul::ErrorCode::io);
}

TEST_CASE("create_Should_RejectAliasedPaths_If_OutputIsHardLinkToSource")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "alias.mul";
    writeFixture(source);
    std::filesystem::create_hard_link(source, output);

    const auto editor = uomul::MapEditor::create(source, output, dimensions, {.overwriteOutput = true});

    REQUIRE_FALSE(editor);
    REQUIRE(editor.error().code == uomul::ErrorCode::io);
}

TEST_CASE("create_Should_RejectExistingOutput_If_OverwriteIsDisabled")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    {
        std::ofstream existing(output, std::ios::binary);
        existing << "existing output";
    }

    const auto editor = uomul::MapEditor::create(source, output, dimensions);

    REQUIRE_FALSE(editor);
    REQUIRE(editor.error().code == uomul::ErrorCode::io);
    REQUIRE(uomul::test::readBytes(output) == "existing output");
}

TEST_CASE("create_Should_RejectOutput_If_ParentDirectoryDoesNotExist")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    writeFixture(source);

    const auto editor = uomul::MapEditor::create(source, directory.path() / "missing" / "output.mul", dimensions);

    REQUIRE_FALSE(editor);
    REQUIRE(editor.error().code == uomul::ErrorCode::io);
}

TEST_CASE("create_Should_CopyMultipleBuffers_If_SourceExceedsBufferSize")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    constexpr uomul::Dimensions largeDimensions{.widthTiles = 768, .heightTiles = 768};
    constexpr std::uint32_t blocksPerDimension = largeDimensions.widthTiles / uomul::landBlockDimension;
    constexpr std::uint32_t blockCount = blocksPerDimension * blocksPerDimension;
    writeFixture(source, blockCount);

    auto editor = uomul::MapEditor::create(source, output, largeDimensions);

    REQUIRE(editor);
    REQUIRE(editor->commit());
    REQUIRE(std::filesystem::file_size(output) == std::filesystem::file_size(source));
    const auto reader = uomul::MapReader::open(output, largeDimensions);
    REQUIRE(reader);
    REQUIRE(reader->completeBlocks() == blockCount);
}

TEST_CASE("editLandBlocks_Should_ProcessJournalAcrossReadChunks_If_RegionExceedsChunkSize")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    constexpr std::uint32_t journalBlockCount = 1025;
    constexpr uomul::Dimensions journalDimensions{
        .widthTiles = uomul::landBlockDimension,
        .heightTiles = journalBlockCount * uomul::landBlockDimension,
    };
    writeFixture(source, journalBlockCount);
    auto editor = uomul::MapEditor::create(source, output, journalDimensions);
    REQUIRE(editor);
    std::uint32_t callbackCount = 0;

    REQUIRE(editor->editLandBlocks({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 1, .y = journalBlockCount}},
                                   [&callbackCount](uomul::BlockCoord coordinate, const uomul::LandBlock & current)
                                       -> std::expected<uomul::LandBlock, uomul::Error>
                                   {
                                       ++callbackCount;
                                       auto changed = current;
                                       changed.at(0).id = static_cast<std::uint16_t>(stagedBlockTileId + coordinate.y);
                                       return changed;
                                   }));
    REQUIRE(callbackCount == journalBlockCount);
    REQUIRE(editor->commit());

    const auto reader = uomul::MapReader::open(output, journalDimensions);
    REQUIRE(reader);
    REQUIRE(reader->readLandTile({.x = 0, .y = 1023 * uomul::landBlockDimension})->id == stagedBlockTileId + 1023);
    REQUIRE(reader->readLandTile({.x = 0, .y = 1024 * uomul::landBlockDimension})->id == stagedBlockTileId + 1024);
}

TEST_CASE("create_Should_ReplaceExistingOutput_If_OverwriteIsEnabled")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    {
        std::ofstream existing(output, std::ios::binary);
        existing << "old output";
    }
    auto editor = uomul::MapEditor::create(source, output, dimensions, {.overwriteOutput = true});
    REQUIRE(editor);
    REQUIRE(editor->writeLandTile({1, 1}, {.id = stagedTileId, .z = stagedTileElevation}));

    REQUIRE(editor->commit());

    const auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readLandTile({1, 1}) == uomul::LandTile{.id = stagedTileId, .z = stagedTileElevation});
    REQUIRE(reader->readLandTile({2, 2})->id == 18);
}

TEST_CASE("moveConstructor_Should_TransferActiveEditor_If_EditorIsMoved")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto opened = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(opened);

    uomul::MapEditor editor(std::move(*opened));
    opened->abort();
    REQUIRE(editor.writeLandTile({0, 0}, {.id = stagedTileId, .z = stagedTileElevation}));
    REQUIRE(editor.commit());

    const auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readLandTile({0, 0}) == uomul::LandTile{.id = stagedTileId, .z = stagedTileElevation});
}

TEST_CASE("moveAssignment_Should_TransferActiveEditor_If_DestinationIsAlreadyCommitted")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto firstOutput = directory.path() / "first.mul";
    const auto secondOutput = directory.path() / "second.mul";
    writeFixture(source);
    auto first = uomul::MapEditor::create(source, firstOutput, dimensions);
    auto second = uomul::MapEditor::create(source, secondOutput, dimensions);
    REQUIRE(first);
    REQUIRE(second);
    REQUIRE(first->commit());

    *first = std::move(*second);

    REQUIRE(first->writeLandTile({0, 0}, {.id = stagedTileId, .z = stagedTileElevation}));
    REQUIRE(first->commit());
    const auto reader = uomul::MapReader::open(secondOutput, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readLandTile({0, 0}) == uomul::LandTile{.id = stagedTileId, .z = stagedTileElevation});
}

TEST_CASE("moveAssignment_Should_DiscardDestinationStaging_If_DestinationIsActive")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto firstOutput = directory.path() / "first.mul";
    const auto secondOutput = directory.path() / "second.mul";
    writeFixture(source);
    auto first = uomul::MapEditor::create(source, firstOutput, dimensions);
    auto second = uomul::MapEditor::create(source, secondOutput, dimensions);
    REQUIRE(first);
    REQUIRE(second);
    std::vector<std::filesystem::path> firstStagingFiles;
    for (const auto & entry : std::filesystem::directory_iterator(directory.path()))
    {
        if (entry.path().filename().string().starts_with("first.mul.uomul-tmp-"))
        {
            firstStagingFiles.push_back(entry.path());
        }
    }
    REQUIRE(firstStagingFiles.size() == 1);

    *first = std::move(*second);

    REQUIRE_FALSE(std::filesystem::exists(firstStagingFiles.front()));
    REQUIRE(first->writeLandTile({0, 0}, {.id = stagedTileId, .z = stagedTileElevation}));
    REQUIRE(first->commit());
    REQUIRE_FALSE(std::filesystem::exists(firstOutput));
    const auto reader = uomul::MapReader::open(secondOutput, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readLandTile({0, 0}) == uomul::LandTile{.id = stagedTileId, .z = stagedTileElevation});
}

TEST_CASE("create_Should_AcceptRelativeOutputPath_If_OutputIsInCurrentDirectory")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    const CurrentPathGuard currentPath(directory.path());
    auto editor = uomul::MapEditor::create("source.mul", "output.mul", dimensions);

    REQUIRE(editor);
    REQUIRE(editor->commit());
    REQUIRE(std::filesystem::exists(output));
}

TEST_CASE("writeLandTile_Should_StageTile_If_CoordinateIsValid")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);

    REQUIRE(editor->writeLandTile({.x = 8, .y = 1}, {.id = stagedTileId, .z = stagedTileElevation}));
    REQUIRE_FALSE(std::filesystem::exists(output));
    REQUIRE(editor->commit());

    const auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readLandTile({8, 1}) == uomul::LandTile{.id = stagedTileId, .z = stagedTileElevation});
    const auto sourceReader = uomul::MapReader::open(source, dimensions);
    REQUIRE(sourceReader);
    REQUIRE(sourceReader->readLandTile({8, 1})->id == 208);
}

TEST_CASE("writeLandBlock_Should_StageBlock_If_CoordinateIsValid")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);
    uomul::LandBlock block{};
    for (auto & tile : block)
    {
        tile = {.id = stagedBlockTileId, .z = stagedTileElevation};
    }

    REQUIRE(editor->writeLandBlock({1, 0}, block));
    REQUIRE(editor->commit());

    const auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readLandTile({8, 0}) == uomul::LandTile{.id = stagedBlockTileId, .z = stagedTileElevation});
    REQUIRE(reader->readLandTile({15, 7}) == uomul::LandTile{.id = stagedBlockTileId, .z = stagedTileElevation});
    REQUIRE(reader->readLandTile({0, 0})->id == 0);
}

TEST_CASE("writeLandTile_Should_RejectReadFailure_If_StagingFileIsTruncated")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);
    for (const auto & entry : std::filesystem::directory_iterator(directory.path()))
    {
        if (entry.path().filename().string().starts_with("output.mul.uomul-tmp-"))
        {
            std::filesystem::resize_file(entry.path(), 0);
        }
    }

    const auto result = editor->writeLandTile({0, 0}, {.id = 1, .z = 0});

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("writeLandTile_Should_RejectCoordinates_If_TileIsOutsideMap")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);

    const auto outsideX = editor->writeLandTile({.x = dimensions.widthTiles, .y = 0}, {.id = 1, .z = 0});
    const auto outsideY = editor->writeLandTile({.x = 0, .y = dimensions.heightTiles}, {.id = 1, .z = 0});

    REQUIRE_FALSE(outsideX);
    REQUIRE(outsideX.error().code == uomul::ErrorCode::invalidCoordinate);
    REQUIRE_FALSE(outsideY);
    REQUIRE(outsideY.error().code == uomul::ErrorCode::invalidCoordinate);
}

TEST_CASE("writeLandBlock_Should_RejectReadFailure_If_StagingFileIsTruncated")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);
    for (const auto & entry : std::filesystem::directory_iterator(directory.path()))
    {
        if (entry.path().filename().string().starts_with("output.mul.uomul-tmp-"))
        {
            std::filesystem::resize_file(entry.path(), 0);
        }
    }

    const auto result = editor->writeLandBlock({0, 0}, uomul::LandBlock{});

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::io);
}

TEST_CASE("writeLandBlock_Should_RejectCoordinates_If_BlockIsOutsideMap")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);

    const auto outsideX = editor->writeLandBlock({blockGridSize, 0}, uomul::LandBlock{});
    const auto outsideY = editor->writeLandBlock({0, blockGridSize}, uomul::LandBlock{});

    REQUIRE_FALSE(outsideX);
    REQUIRE(outsideX.error().code == uomul::ErrorCode::invalidCoordinate);
    REQUIRE_FALSE(outsideY);
    REQUIRE(outsideY.error().code == uomul::ErrorCode::invalidCoordinate);
}

TEST_CASE("editLandBlocks_Should_ChangeRegion_If_CallbackSucceeds")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);
    std::uint32_t callbackCount = 0;

    REQUIRE(editor->editLandBlocks(
        {.min = {0, 0}, .maxExclusive = {1, testRegionEnd}},
        [&callbackCount](uomul::BlockCoord, const uomul::LandBlock & current) -> std::expected<uomul::LandBlock, uomul::Error>
        {
            ++callbackCount;
            auto changed = current;
            changed.at(0) = {.id = stagedBlockTileId, .z = stagedTileElevation};
            return changed;
        }));
    REQUIRE(callbackCount == testRegionEnd);
    REQUIRE(editor->commit());

    const auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readLandTile({0, 0}) == uomul::LandTile{.id = stagedBlockTileId, .z = stagedTileElevation});
    REQUIRE(reader->readLandTile({0, 8}) == uomul::LandTile{.id = stagedBlockTileId, .z = stagedTileElevation});
    REQUIRE(reader->readLandTile({1, 0})->id == 1);
}

TEST_CASE("editLandBlocks_Should_DiscardWholeEdit_If_CallbackFails")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);
    std::uint32_t callbackCount = 0;

    const auto result = editor->editLandBlocks(
        {.min = {0, 0}, .maxExclusive = {testRegionEnd, testRegionEnd}},
        [&callbackCount](uomul::BlockCoord coordinate,
                         const uomul::LandBlock & current) -> std::expected<uomul::LandBlock, uomul::Error>
        {
            if (++callbackCount == 3)
            {
                return std::unexpected(
                    uomul::Error{.code = uomul::ErrorCode::io, .path = {}, .offset = 0, .message = "callback failure"});
            }
            auto changed = current;
            changed.at(0).id = static_cast<std::uint16_t>(stagedBlockTileId + coordinate.x + coordinate.y);
            return changed;
        });

    REQUIRE_FALSE(result);
    REQUIRE(result.error().message == "callback failure");
    REQUIRE(callbackCount == 3);
    REQUIRE(editor->commit());
    const auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readLandTile({0, 0})->id == 0);
    REQUIRE(reader->readLandTile({0, 8})->id == 100);
    REQUIRE(reader->readLandTile({8, 0})->id == 200);
}

TEST_CASE("editLandTiles_Should_ChangeHalfOpenRegion_If_CallbackSucceedsAcrossBlocks")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);
    std::vector<uomul::TileCoord> coordinates;

    REQUIRE(
        editor->editLandTiles({.min = {7, 7}, .maxExclusive = {9, 9}},
                              [&coordinates](uomul::TileCoord coordinate,
                                             const uomul::LandTile & current) -> std::expected<uomul::LandTile, uomul::Error>
                              {
                                  coordinates.push_back(coordinate);
                                  return uomul::LandTile{.id = static_cast<std::uint16_t>(current.id + 1000), .z = current.z};
                              }));
    REQUIRE(coordinates.size() == 4);
    REQUIRE(coordinates.at(0).x == 7);
    REQUIRE(coordinates.at(0).y == 7);
    REQUIRE(coordinates.at(3).x == 8);
    REQUIRE(coordinates.at(3).y == 8);
    REQUIRE(editor->commit());

    const auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readLandTile({7, 7})->id == 63 + 1000);
    REQUIRE(reader->readLandTile({8, 7})->id == 256 + 1000);
    REQUIRE(reader->readLandTile({6, 7})->id == 62);
    REQUIRE(reader->readLandTile({9, 9})->id == 309);
}

TEST_CASE("editLandTiles_Should_DiscardWholeEdit_If_CallbackFails")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);
    std::uint32_t callbackCount = 0;

    const auto result = editor->editLandTiles(
        {.min = {0, 0}, .maxExclusive = {3, 3}},
        [&callbackCount](uomul::TileCoord coordinate,
                         const uomul::LandTile & current) -> std::expected<uomul::LandTile, uomul::Error>
        {
            if (++callbackCount == 5)
            {
                return std::unexpected(
                    uomul::Error{.code = uomul::ErrorCode::io, .path = {}, .offset = 0, .message = "callback failure"});
            }
            return uomul::LandTile{.id = static_cast<std::uint16_t>(current.id + coordinate.x + 1), .z = current.z};
        });

    REQUIRE_FALSE(result);
    REQUIRE(result.error().message == "callback failure");
    REQUIRE(callbackCount == 5);
    REQUIRE(editor->commit());
    const auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readLandTile({0, 0})->id == 0);
    REQUIRE(reader->readLandTile({2, 1})->id == 10);
}

TEST_CASE("editLandBlocks_Should_AcceptEmptyRegion_If_CallbackIsValid")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);
    std::uint32_t callbackCount = 0;

    REQUIRE(editor->editLandBlocks(
        {.min = {0, 0}, .maxExclusive = {0, testRegionEnd}},
        [&callbackCount](uomul::BlockCoord, const uomul::LandBlock & block) -> std::expected<uomul::LandBlock, uomul::Error>
        {
            ++callbackCount;
            return block;
        }));

    REQUIRE(callbackCount == 0);
    REQUIRE(editor->commit());
}

TEST_CASE("editLandTiles_Should_AcceptEmptyRegion_If_CallbackIsValid")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);
    std::uint32_t callbackCount = 0;

    REQUIRE(editor->editLandTiles(
        {.min = {dimensions.widthTiles, dimensions.heightTiles},
         .maxExclusive = {dimensions.widthTiles, dimensions.heightTiles}},
        [&callbackCount](uomul::TileCoord, const uomul::LandTile & tile) -> std::expected<uomul::LandTile, uomul::Error>
        {
            ++callbackCount;
            return tile;
        }));

    REQUIRE(callbackCount == 0);
    REQUIRE(editor->commit());
}

TEST_CASE("editLandBlocks_Should_RejectRequest_If_RegionIsInvalidOrCallbackIsEmpty")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);

    const auto reversedX = editor->editLandBlocks({.min = {1, 0}, .maxExclusive = {0, 1}}, acceptEditedBlock);
    const auto reversedY = editor->editLandBlocks({.min = {0, 1}, .maxExclusive = {1, 0}}, acceptEditedBlock);
    const auto outsideX = editor->editLandBlocks({.min = {0, 0}, .maxExclusive = {blockGridSize + 1, 0}}, acceptEditedBlock);
    const auto outsideY = editor->editLandBlocks({.min = {0, 0}, .maxExclusive = {0, blockGridSize + 1}}, acceptEditedBlock);
    const auto emptyCallback = editor->editLandBlocks({.min = {0, 0}, .maxExclusive = {1, 1}}, {});

    REQUIRE_FALSE(reversedX);
    REQUIRE(reversedX.error().code == uomul::ErrorCode::invalidCoordinate);
    REQUIRE_FALSE(reversedY);
    REQUIRE(reversedY.error().code == uomul::ErrorCode::invalidCoordinate);
    REQUIRE_FALSE(outsideX);
    REQUIRE(outsideX.error().code == uomul::ErrorCode::invalidCoordinate);
    REQUIRE_FALSE(outsideY);
    REQUIRE(outsideY.error().code == uomul::ErrorCode::invalidCoordinate);
    REQUIRE_FALSE(emptyCallback);
    REQUIRE(emptyCallback.error().code == uomul::ErrorCode::io);
}

TEST_CASE("editLandTiles_Should_RejectRequest_If_RegionIsInvalidOrCallbackIsEmpty")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);

    const auto reversedX = editor->editLandTiles({.min = {1, 0}, .maxExclusive = {0, 1}}, acceptEditedTile);
    const auto reversedY = editor->editLandTiles({.min = {0, 1}, .maxExclusive = {1, 0}}, acceptEditedTile);
    const auto outsideX =
        editor->editLandTiles({.min = {0, 0}, .maxExclusive = {dimensions.widthTiles + 1, 0}}, acceptEditedTile);
    const auto outsideY =
        editor->editLandTiles({.min = {0, 0}, .maxExclusive = {0, dimensions.heightTiles + 1}}, acceptEditedTile);
    const auto emptyCallback = editor->editLandTiles(smallTileRegion, {});

    REQUIRE_FALSE(reversedX);
    REQUIRE(reversedX.error().code == uomul::ErrorCode::invalidCoordinate);
    REQUIRE_FALSE(reversedY);
    REQUIRE(reversedY.error().code == uomul::ErrorCode::invalidCoordinate);
    REQUIRE_FALSE(outsideX);
    REQUIRE(outsideX.error().code == uomul::ErrorCode::invalidCoordinate);
    REQUIRE_FALSE(outsideY);
    REQUIRE(outsideY.error().code == uomul::ErrorCode::invalidCoordinate);
    REQUIRE_FALSE(emptyCallback);
    REQUIRE(emptyCallback.error().code == uomul::ErrorCode::io);
}

TEST_CASE("fill_Should_FillRegion_If_RegionIsValid")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);

    REQUIRE(editor->fillTiles(smallTileRegion, {.id = stagedTileId, .z = stagedTileElevation}));
    REQUIRE(editor->replaceTileIds(smallTileRegion, stagedTileId, editedTileId));
    REQUIRE(editor->replaceTileIds(smallTileRegion, 42, 43));
    REQUIRE(editor->replaceTileZ(smallTileRegion, stagedTileElevation, 9));
    REQUIRE(editor->replaceTileZ(smallTileRegion, 42, 43));
    REQUIRE(editor->commit());

    const auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readLandTile({0, 0}) == uomul::LandTile{.id = editedTileId, .z = 9});
    REQUIRE(reader->readLandTile({1, 1}) == uomul::LandTile{.id = editedTileId, .z = 9});
    REQUIRE(reader->readLandTile({2, 2})->id == 18);
}

TEST_CASE("fillAndReplace_Should_SupportWholeMapAndBlockRegions")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);

    REQUIRE(editor->fillBlocks({.min = {0, 0}, .maxExclusive = {1, 1}}, {.id = stagedTileId, .z = stagedTileElevation}));
    REQUIRE(editor->replaceBlockIds({.min = {1, 1}, .maxExclusive = {2, 2}}, 363, editedTileId));
    REQUIRE(editor->replaceBlockZ({.min = {1, 1}, .maxExclusive = {2, 2}}, 63, 9));
    REQUIRE(editor->commit());

    const auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readLandTile({0, 0}) == uomul::LandTile{.id = stagedTileId, .z = stagedTileElevation});
    REQUIRE(reader->readLandTile({7, 7}) == uomul::LandTile{.id = stagedTileId, .z = stagedTileElevation});
    REQUIRE(reader->readLandTile({15, 15}) == uomul::LandTile{.id = editedTileId, .z = 9});
    REQUIRE(reader->readLandTile({8, 0}) == uomul::LandTile{.id = 200, .z = -1});

    const auto replacedOutput = directory.path() / "replaced.mul";
    auto replacedEditor = uomul::MapEditor::create(source, replacedOutput, dimensions);
    REQUIRE(replacedEditor);
    REQUIRE(replacedEditor->replaceIds(0, 77));
    REQUIRE(replacedEditor->replaceZ(-1, 5));
    REQUIRE(replacedEditor->commit());
    const auto replacedReader = uomul::MapReader::open(replacedOutput, dimensions);
    REQUIRE(replacedReader);
    REQUIRE(replacedReader->readLandTile({0, 0}) == uomul::LandTile{.id = 77, .z = 5});
    REQUIRE(replacedReader->readLandTile({15, 15}) == uomul::LandTile{.id = 363, .z = 63});
    REQUIRE(replacedReader->readLandTile({8, 0}) == uomul::LandTile{.id = 200, .z = 5});
    REQUIRE(replacedReader->readLandTile({8, 1}) == uomul::LandTile{.id = 208, .z = 8});

    const auto fullOutput = directory.path() / "full.mul";
    auto fullEditor = uomul::MapEditor::create(source, fullOutput, dimensions);
    REQUIRE(fullEditor);
    REQUIRE(fullEditor->fill({.id = editedTileId, .z = stagedTileElevation}));
    REQUIRE(fullEditor->commit());
    const auto fullReader = uomul::MapReader::open(fullOutput, dimensions);
    REQUIRE(fullReader);
    REQUIRE(fullReader->readLandTile({15, 15}) == uomul::LandTile{.id = editedTileId, .z = stagedTileElevation});
}

TEST_CASE("editLandTiles_Should_UseEarlierStagedValues_If_RegionIsEditedAgain")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);

    REQUIRE(editor->fillTiles(smallTileRegion, {.id = stagedTileId, .z = stagedTileElevation}));
    REQUIRE(editor->editLandTiles(
        smallTileRegion,
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
    REQUIRE(reader->readLandTile({1, 1}) == uomul::LandTile{.id = editedTileId, .z = stagedTileElevation});
    REQUIRE(reader->readLandTile({2, 2})->id == 18);
}

TEST_CASE("abort_Should_DiscardChanges_If_EditorIsActive")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);
    REQUIRE(editor->writeLandTile({0, 0}, {.id = stagedTileId, .z = stagedTileElevation}));

    editor->abort();
    editor->abort();

    REQUIRE_FALSE(std::filesystem::exists(output));
    const auto sourceReader = uomul::MapReader::open(source, dimensions);
    REQUIRE(sourceReader);
    REQUIRE(sourceReader->readLandTile({0, 0})->id == 0);
    REQUIRE_FALSE(editor->writeLandTile({0, 0}, {.id = 1, .z = 0}));
    REQUIRE_FALSE(editor->writeLandBlock({0, 0}, uomul::LandBlock{}));
    const auto blockEdit = editor->editLandBlocks({{0, 0}, {1, 1}}, acceptEditedBlock);
    const auto tileEdit = editor->editLandTiles(smallTileRegion, acceptEditedTile);
    const auto fillResult = editor->fillTiles(smallTileRegion, {.id = 1, .z = 0});
    const auto replaceIdResult = editor->replaceTileIds(smallTileRegion, 0, 1);
    const auto replaceZResult = editor->replaceTileZ(smallTileRegion, 0, 1);
    REQUIRE_FALSE(blockEdit);
    REQUIRE(blockEdit.error().code == uomul::ErrorCode::commitFailed);
    REQUIRE_FALSE(tileEdit);
    REQUIRE(tileEdit.error().code == uomul::ErrorCode::commitFailed);
    REQUIRE_FALSE(fillResult);
    REQUIRE(fillResult.error().code == uomul::ErrorCode::commitFailed);
    REQUIRE_FALSE(replaceIdResult);
    REQUIRE(replaceIdResult.error().code == uomul::ErrorCode::commitFailed);
    REQUIRE_FALSE(replaceZResult);
    REQUIRE(replaceZResult.error().code == uomul::ErrorCode::commitFailed);
    const auto result = editor->commit();
    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::commitFailed);
}

TEST_CASE("commit_Should_RejectRepeatedOperations_If_EditorHasAlreadyCommitted")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);
    REQUIRE(editor->commit());

    REQUIRE_FALSE(editor->writeLandBlock({0, 0}, uomul::LandBlock{}));
    REQUIRE_FALSE(editor->writeLandTile({0, 0}, {.id = 1, .z = 0}));
    const auto blockEdit = editor->editLandBlocks({{0, 0}, {1, 1}}, acceptEditedBlock);
    const auto tileEdit = editor->editLandTiles(smallTileRegion, acceptEditedTile);
    const auto fillResult = editor->fillTiles(smallTileRegion, {.id = 1, .z = 0});
    const auto replaceIdResult = editor->replaceTileIds(smallTileRegion, 0, 1);
    const auto replaceZResult = editor->replaceTileZ(smallTileRegion, 0, 1);
    REQUIRE_FALSE(blockEdit);
    REQUIRE(blockEdit.error().code == uomul::ErrorCode::commitFailed);
    REQUIRE_FALSE(tileEdit);
    REQUIRE(tileEdit.error().code == uomul::ErrorCode::commitFailed);
    REQUIRE_FALSE(fillResult);
    REQUIRE(fillResult.error().code == uomul::ErrorCode::commitFailed);
    REQUIRE_FALSE(replaceIdResult);
    REQUIRE(replaceIdResult.error().code == uomul::ErrorCode::commitFailed);
    REQUIRE_FALSE(replaceZResult);
    REQUIRE(replaceZResult.error().code == uomul::ErrorCode::commitFailed);
    const auto result = editor->commit();
    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::commitFailed);
}

TEST_CASE("commit_Should_RejectOutputRace_If_OutputAppearsAfterEditorCreation")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);
    {
        std::ofstream existing(output, std::ios::binary);
        existing << "external output";
    }

    const auto result = editor->commit();

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::commitFailed);
    REQUIRE(uomul::test::readBytes(output) == "external output");
}

TEST_CASE("abort_Should_KeepMovedToEditorActive_If_MovedFromEditorIsAborted")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto opened = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(opened);

    uomul::MapEditor editor(std::move(*opened));

    opened->abort();
    REQUIRE(editor.commit());
}
