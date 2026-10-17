#include "map_test_helpers.h"
#include "uomul/map.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

using uomul::test::dimensions;
using uomul::test::TestDirectory;
using uomul::test::writeFixture;

constexpr std::uint32_t blockGridSize = dimensions.widthTiles / uomul::mapBlockDimension;
constexpr std::uint32_t testRegionEnd = 2;
constexpr std::uint16_t stagedTileId = 777;
constexpr std::uint16_t editedTileId = 778;
constexpr std::uint16_t stagedBlockTileId = 999;
constexpr std::int8_t stagedTileElevation = -12;
constexpr uomul::TileRegion smallTileRegion{.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 2, .y = 2}};

[[nodiscard]] std::expected<uomul::MapBlock, uomul::Error> acceptEditedBlock(uomul::BlockCoord, const uomul::MapBlock & block)
{
    return block;
}

[[nodiscard]] std::expected<uomul::MapTile, uomul::Error> acceptEditedTile(uomul::TileCoord, const uomul::MapTile & tile)
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

TEST_CASE("createEmpty_Should_CreateCompleteMapWithDefaultTile_If_DimensionsAreValid")
{
    const TestDirectory directory;
    const auto output = directory.path() / "empty.mul";

    REQUIRE(uomul::MapEditor::createEmpty(output, dimensions));

    const auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->completeBlocks() == 4);
    REQUIRE(reader->missingBlocks() == 0);
    REQUIRE(std::filesystem::file_size(output) == 4 * uomul::mapBlockByteCount);
    REQUIRE(reader->readTile({.x = 0, .y = 0}) == uomul::MapTile{});
    REQUIRE(reader->readTile({.x = 15, .y = 15}) == uomul::MapTile{});
}

TEST_CASE("createEmpty_Should_InitializeEveryTile_If_CustomTileIsProvided")
{
    const TestDirectory directory;
    const auto output = directory.path() / "custom.mul";
    constexpr uomul::Dimensions customDimensions{.widthTiles = 24, .heightTiles = 8};
    constexpr uomul::MapTile initialTile{.id = 0xface, .z = -42};

    REQUIRE(uomul::MapEditor::createEmpty(output, customDimensions, initialTile));

    const auto reader = uomul::MapReader::open(output, customDimensions);
    REQUIRE(reader);
    REQUIRE(reader->completeBlocks() == 3);
    std::uint64_t tileCount = 0;
    REQUIRE(reader->readTiles(
        {.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 24, .y = 8}},
        [initialTile, &tileCount](uomul::TileCoord, const uomul::MapTile & tile) -> std::expected<void, uomul::Error>
        {
            REQUIRE(tile == initialTile);
            tileCount++;
            return {};
        }));
    REQUIRE(tileCount == 24 * 8);
}

TEST_CASE("createEmpty_Should_RejectInvalidDimensionsAndPaths_WithoutCreatingDestination")
{
    const TestDirectory directory;
    const auto invalidDimensionsOutput = directory.path() / "invalid.mul";
    const auto missingDirectoryOutput = directory.path() / "missing" / "output.mul";

    const auto invalidDimensions = uomul::MapEditor::createEmpty(invalidDimensionsOutput, {.widthTiles = 15, .heightTiles = 8});
    const auto missingDirectory = uomul::MapEditor::createEmpty(missingDirectoryOutput, dimensions);

    REQUIRE_FALSE(invalidDimensions);
    REQUIRE(invalidDimensions.error().code == uomul::ErrorCode::invalidDimensions);
    REQUIRE_FALSE(std::filesystem::exists(invalidDimensionsOutput));
    REQUIRE_FALSE(missingDirectory);
    REQUIRE_FALSE(std::filesystem::exists(missingDirectoryOutput));
}

TEST_CASE("createEmpty_Should_HonorOverwriteOption_AndPreserveExistingOutputOnFailure")
{
    const TestDirectory directory;
    const auto output = directory.path() / "existing.mul";
    constexpr std::string_view existingBytes = "existing output";
    {
        std::ofstream existing(output, std::ios::binary);
        existing << existingBytes;
    }

    const auto rejected = uomul::MapEditor::createEmpty(output, dimensions);

    REQUIRE_FALSE(rejected);
    REQUIRE(uomul::test::readBytes(output) == existingBytes);
    REQUIRE(uomul::MapEditor::createEmpty(output, dimensions, {.id = 1234, .z = 7}, {.overwriteOutput = true}));
    const auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readTile({.x = 8, .y = 8}) == uomul::MapTile{.id = 1234, .z = 7});
}

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
    constexpr std::uint32_t blocksPerDimension = largeDimensions.widthTiles / uomul::mapBlockDimension;
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

TEST_CASE("editBlocks_Should_ProcessJournalAcrossReadChunks_If_RegionExceedsChunkSize")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    constexpr std::uint32_t journalBlockCount = 1025;
    constexpr uomul::Dimensions journalDimensions{
        .widthTiles = uomul::mapBlockDimension,
        .heightTiles = journalBlockCount * uomul::mapBlockDimension,
    };
    writeFixture(source, journalBlockCount);
    auto editor = uomul::MapEditor::create(source, output, journalDimensions);
    REQUIRE(editor);
    std::uint32_t callbackCount = 0;

    REQUIRE(editor->editBlocks({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 1, .y = journalBlockCount}},
                               [&callbackCount](uomul::BlockCoord coordinate,
                                                const uomul::MapBlock & current) -> std::expected<uomul::MapBlock, uomul::Error>
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
    REQUIRE(reader->readTile({.x = 0, .y = 1023 * uomul::mapBlockDimension})->id == stagedBlockTileId + 1023);
    REQUIRE(reader->readTile({.x = 0, .y = 1024 * uomul::mapBlockDimension})->id == stagedBlockTileId + 1024);
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
    REQUIRE(editor->editTile({1, 1}, {.id = stagedTileId, .z = stagedTileElevation}));

    REQUIRE(editor->commit());

    const auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readTile({1, 1}) == uomul::MapTile{.id = stagedTileId, .z = stagedTileElevation});
    REQUIRE(reader->readTile({2, 2})->id == 18);
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
    REQUIRE(editor.editTile({0, 0}, {.id = stagedTileId, .z = stagedTileElevation}));
    REQUIRE(editor.commit());

    const auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readTile({0, 0}) == uomul::MapTile{.id = stagedTileId, .z = stagedTileElevation});
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

    REQUIRE(first->editTile({0, 0}, {.id = stagedTileId, .z = stagedTileElevation}));
    REQUIRE(first->commit());
    const auto reader = uomul::MapReader::open(secondOutput, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readTile({0, 0}) == uomul::MapTile{.id = stagedTileId, .z = stagedTileElevation});
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
    REQUIRE(first->editTile({0, 0}, {.id = stagedTileId, .z = stagedTileElevation}));
    REQUIRE(first->commit());
    REQUIRE_FALSE(std::filesystem::exists(firstOutput));
    const auto reader = uomul::MapReader::open(secondOutput, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readTile({0, 0}) == uomul::MapTile{.id = stagedTileId, .z = stagedTileElevation});
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

TEST_CASE("editTile_Should_StageTile_If_CoordinateIsValid")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);

    REQUIRE(editor->editTile({.x = 8, .y = 1}, {.id = stagedTileId, .z = stagedTileElevation}));
    REQUIRE_FALSE(std::filesystem::exists(output));
    REQUIRE(editor->commit());

    const auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readTile({8, 1}) == uomul::MapTile{.id = stagedTileId, .z = stagedTileElevation});
    const auto sourceReader = uomul::MapReader::open(source, dimensions);
    REQUIRE(sourceReader);
    REQUIRE(sourceReader->readTile({8, 1})->id == 208);
}

TEST_CASE("editBlock_Should_StageBlock_If_CoordinateIsValid")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);
    uomul::MapBlock block{};
    for (auto & tile : block)
    {
        tile = {.id = stagedBlockTileId, .z = stagedTileElevation};
    }

    REQUIRE(editor->editBlock({1, 0}, block));
    REQUIRE(editor->commit());

    const auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readTile({8, 0}) == uomul::MapTile{.id = stagedBlockTileId, .z = stagedTileElevation});
    REQUIRE(reader->readTile({15, 7}) == uomul::MapTile{.id = stagedBlockTileId, .z = stagedTileElevation});
    REQUIRE(reader->readTile({0, 0})->id == 0);
}

TEST_CASE("editTile_Should_RejectReadFailure_If_StagingFileIsTruncated")
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

    const auto result = editor->editTile({0, 0}, {.id = 1, .z = 0});

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("editTile_Should_RejectCoordinates_If_TileIsOutsideMap")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);

    const auto outsideX = editor->editTile({.x = dimensions.widthTiles, .y = 0}, {.id = 1, .z = 0});
    const auto outsideY = editor->editTile({.x = 0, .y = dimensions.heightTiles}, {.id = 1, .z = 0});

    REQUIRE_FALSE(outsideX);
    REQUIRE(outsideX.error().code == uomul::ErrorCode::invalidCoordinate);
    REQUIRE_FALSE(outsideY);
    REQUIRE(outsideY.error().code == uomul::ErrorCode::invalidCoordinate);
}

TEST_CASE("editBlock_Should_RejectReadFailure_If_StagingFileIsTruncated")
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

    const auto result = editor->editBlock({0, 0}, uomul::MapBlock{});

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::io);
}

TEST_CASE("editBlock_Should_RejectCoordinates_If_BlockIsOutsideMap")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);

    const auto outsideX = editor->editBlock({blockGridSize, 0}, uomul::MapBlock{});
    const auto outsideY = editor->editBlock({0, blockGridSize}, uomul::MapBlock{});

    REQUIRE_FALSE(outsideX);
    REQUIRE(outsideX.error().code == uomul::ErrorCode::invalidCoordinate);
    REQUIRE_FALSE(outsideY);
    REQUIRE(outsideY.error().code == uomul::ErrorCode::invalidCoordinate);
}

TEST_CASE("editBlocks_Should_ChangeRegion_If_CallbackSucceeds")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);
    std::uint32_t callbackCount = 0;

    REQUIRE(editor->editBlocks(
        {.min = {0, 0}, .maxExclusive = {1, testRegionEnd}},
        [&callbackCount](uomul::BlockCoord, const uomul::MapBlock & current) -> std::expected<uomul::MapBlock, uomul::Error>
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
    REQUIRE(reader->readTile({0, 0}) == uomul::MapTile{.id = stagedBlockTileId, .z = stagedTileElevation});
    REQUIRE(reader->readTile({0, 8}) == uomul::MapTile{.id = stagedBlockTileId, .z = stagedTileElevation});
    REQUIRE(reader->readTile({1, 0})->id == 1);
}

TEST_CASE("editBlocks_Should_DiscardWholeEdit_If_CallbackFails")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);
    std::uint32_t callbackCount = 0;

    const auto result =
        editor->editBlocks({.min = {0, 0}, .maxExclusive = {testRegionEnd, testRegionEnd}},
                           [&callbackCount](uomul::BlockCoord coordinate,
                                            const uomul::MapBlock & current) -> std::expected<uomul::MapBlock, uomul::Error>
                           {
                               if (++callbackCount == 3)
                               {
                                   return std::unexpected(uomul::Error{
                                       .code = uomul::ErrorCode::io, .path = {}, .offset = 0, .message = "callback failure"});
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
    REQUIRE(reader->readTile({0, 0})->id == 0);
    REQUIRE(reader->readTile({0, 8})->id == 100);
    REQUIRE(reader->readTile({8, 0})->id == 200);
}

TEST_CASE("editTiles_Should_ChangeHalfOpenRegion_If_CallbackSucceedsAcrossBlocks")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);
    std::vector<uomul::TileCoord> coordinates;

    REQUIRE(editor->editTiles({.min = {7, 7}, .maxExclusive = {9, 9}},
                              [&coordinates](uomul::TileCoord coordinate,
                                             const uomul::MapTile & current) -> std::expected<uomul::MapTile, uomul::Error>
                              {
                                  coordinates.push_back(coordinate);
                                  return uomul::MapTile{.id = static_cast<std::uint16_t>(current.id + 1000), .z = current.z};
                              }));
    REQUIRE(coordinates.size() == 4);
    REQUIRE(coordinates.at(0).x == 7);
    REQUIRE(coordinates.at(0).y == 7);
    REQUIRE(coordinates.at(3).x == 8);
    REQUIRE(coordinates.at(3).y == 8);
    REQUIRE(editor->commit());

    const auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readTile({7, 7})->id == 63 + 1000);
    REQUIRE(reader->readTile({8, 7})->id == 256 + 1000);
    REQUIRE(reader->readTile({6, 7})->id == 62);
    REQUIRE(reader->readTile({9, 9})->id == 309);
}

TEST_CASE("editTiles_Should_DiscardWholeEdit_If_CallbackFails")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);
    std::uint32_t callbackCount = 0;

    const auto result = editor->editTiles(
        {.min = {0, 0}, .maxExclusive = {3, 3}},
        [&callbackCount](uomul::TileCoord coordinate,
                         const uomul::MapTile & current) -> std::expected<uomul::MapTile, uomul::Error>
        {
            if (++callbackCount == 5)
            {
                return std::unexpected(
                    uomul::Error{.code = uomul::ErrorCode::io, .path = {}, .offset = 0, .message = "callback failure"});
            }
            return uomul::MapTile{.id = static_cast<std::uint16_t>(current.id + coordinate.x + 1), .z = current.z};
        });

    REQUIRE_FALSE(result);
    REQUIRE(result.error().message == "callback failure");
    REQUIRE(callbackCount == 5);
    REQUIRE(editor->commit());
    const auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readTile({0, 0})->id == 0);
    REQUIRE(reader->readTile({2, 1})->id == 10);
}

TEST_CASE("editBlocks_Should_AcceptEmptyRegion_If_CallbackIsValid")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);
    std::uint32_t callbackCount = 0;

    REQUIRE(editor->editBlocks(
        {.min = {0, 0}, .maxExclusive = {0, testRegionEnd}},
        [&callbackCount](uomul::BlockCoord, const uomul::MapBlock & block) -> std::expected<uomul::MapBlock, uomul::Error>
        {
            ++callbackCount;
            return block;
        }));

    REQUIRE(callbackCount == 0);
    REQUIRE(editor->commit());
}

TEST_CASE("editTiles_Should_AcceptEmptyRegion_If_CallbackIsValid")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);
    std::uint32_t callbackCount = 0;

    REQUIRE(editor->editTiles(
        {.min = {dimensions.widthTiles, dimensions.heightTiles},
         .maxExclusive = {dimensions.widthTiles, dimensions.heightTiles}},
        [&callbackCount](uomul::TileCoord, const uomul::MapTile & tile) -> std::expected<uomul::MapTile, uomul::Error>
        {
            ++callbackCount;
            return tile;
        }));

    REQUIRE(callbackCount == 0);
    REQUIRE(editor->commit());
}

TEST_CASE("editBlocks_Should_RejectRequest_If_RegionIsInvalidOrCallbackIsEmpty")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);

    const auto reversedX = editor->editBlocks({.min = {1, 0}, .maxExclusive = {0, 1}}, acceptEditedBlock);
    const auto reversedY = editor->editBlocks({.min = {0, 1}, .maxExclusive = {1, 0}}, acceptEditedBlock);
    const auto outsideX = editor->editBlocks({.min = {0, 0}, .maxExclusive = {blockGridSize + 1, 0}}, acceptEditedBlock);
    const auto outsideY = editor->editBlocks({.min = {0, 0}, .maxExclusive = {0, blockGridSize + 1}}, acceptEditedBlock);
    const auto emptyCallback = editor->editBlocks({.min = {0, 0}, .maxExclusive = {1, 1}}, {});

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

TEST_CASE("editTiles_Should_RejectRequest_If_RegionIsInvalidOrCallbackIsEmpty")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);

    const auto reversedX = editor->editTiles({.min = {1, 0}, .maxExclusive = {0, 1}}, acceptEditedTile);
    const auto reversedY = editor->editTiles({.min = {0, 1}, .maxExclusive = {1, 0}}, acceptEditedTile);
    const auto outsideX = editor->editTiles({.min = {0, 0}, .maxExclusive = {dimensions.widthTiles + 1, 0}}, acceptEditedTile);
    const auto outsideY = editor->editTiles({.min = {0, 0}, .maxExclusive = {0, dimensions.heightTiles + 1}}, acceptEditedTile);
    const auto emptyCallback = editor->editTiles(smallTileRegion, {});

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
    REQUIRE(editor->replaceIdsTiles(smallTileRegion, stagedTileId, editedTileId));
    REQUIRE(editor->replaceIdsTiles(smallTileRegion, 42, 43));
    REQUIRE(editor->replaceZTiles(smallTileRegion, stagedTileElevation, 9));
    REQUIRE(editor->replaceZTiles(smallTileRegion, 42, 43));
    REQUIRE(editor->commit());

    const auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readTile({0, 0}) == uomul::MapTile{.id = editedTileId, .z = 9});
    REQUIRE(reader->readTile({1, 1}) == uomul::MapTile{.id = editedTileId, .z = 9});
    REQUIRE(reader->readTile({2, 2})->id == 18);
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
    REQUIRE(editor->replaceIdsBlocks({.min = {1, 1}, .maxExclusive = {2, 2}}, 363, editedTileId));
    REQUIRE(editor->replaceZBlocks({.min = {1, 1}, .maxExclusive = {2, 2}}, 63, 9));
    REQUIRE(editor->commit());

    const auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readTile({0, 0}) == uomul::MapTile{.id = stagedTileId, .z = stagedTileElevation});
    REQUIRE(reader->readTile({7, 7}) == uomul::MapTile{.id = stagedTileId, .z = stagedTileElevation});
    REQUIRE(reader->readTile({15, 15}) == uomul::MapTile{.id = editedTileId, .z = 9});
    REQUIRE(reader->readTile({8, 0}) == uomul::MapTile{.id = 200, .z = -1});

    const auto replacedOutput = directory.path() / "replaced.mul";
    auto replacedEditor = uomul::MapEditor::create(source, replacedOutput, dimensions);
    REQUIRE(replacedEditor);
    REQUIRE(replacedEditor->replaceIds(0, 77));
    REQUIRE(replacedEditor->replaceZ(-1, 5));
    REQUIRE(replacedEditor->commit());
    const auto replacedReader = uomul::MapReader::open(replacedOutput, dimensions);
    REQUIRE(replacedReader);
    REQUIRE(replacedReader->readTile({0, 0}) == uomul::MapTile{.id = 77, .z = 5});
    REQUIRE(replacedReader->readTile({15, 15}) == uomul::MapTile{.id = 363, .z = 63});
    REQUIRE(replacedReader->readTile({8, 0}) == uomul::MapTile{.id = 200, .z = 5});
    REQUIRE(replacedReader->readTile({8, 1}) == uomul::MapTile{.id = 208, .z = 8});

    const auto fullOutput = directory.path() / "full.mul";
    auto fullEditor = uomul::MapEditor::create(source, fullOutput, dimensions);
    REQUIRE(fullEditor);
    REQUIRE(fullEditor->fill({.id = editedTileId, .z = stagedTileElevation}));
    REQUIRE(fullEditor->commit());
    const auto fullReader = uomul::MapReader::open(fullOutput, dimensions);
    REQUIRE(fullReader);
    REQUIRE(fullReader->readTile({15, 15}) == uomul::MapTile{.id = editedTileId, .z = stagedTileElevation});
}

TEST_CASE("editTiles_Should_UseEarlierStagedValues_If_RegionIsEditedAgain")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);

    REQUIRE(editor->fillTiles(smallTileRegion, {.id = stagedTileId, .z = stagedTileElevation}));
    REQUIRE(editor->editTiles(
        smallTileRegion,
        [](uomul::TileCoord, const uomul::MapTile & current) -> std::expected<uomul::MapTile, uomul::Error>
        {
            if (current.id != stagedTileId)
            {
                return std::unexpected(uomul::Error{
                    .code = uomul::ErrorCode::io, .path = {}, .offset = 0, .message = "staged value was not visible"});
            }
            return uomul::MapTile{.id = editedTileId, .z = current.z};
        }));
    REQUIRE(editor->commit());

    const auto reader = uomul::MapReader::open(output, dimensions);
    REQUIRE(reader);
    REQUIRE(reader->readTile({1, 1}) == uomul::MapTile{.id = editedTileId, .z = stagedTileElevation});
    REQUIRE(reader->readTile({2, 2})->id == 18);
}

TEST_CASE("abort_Should_DiscardChanges_If_EditorIsActive")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeFixture(source);
    auto editor = uomul::MapEditor::create(source, output, dimensions);
    REQUIRE(editor);
    REQUIRE(editor->editTile({0, 0}, {.id = stagedTileId, .z = stagedTileElevation}));

    editor->abort();
    editor->abort();

    REQUIRE_FALSE(std::filesystem::exists(output));
    const auto sourceReader = uomul::MapReader::open(source, dimensions);
    REQUIRE(sourceReader);
    REQUIRE(sourceReader->readTile({0, 0})->id == 0);
    REQUIRE_FALSE(editor->editTile({0, 0}, {.id = 1, .z = 0}));
    REQUIRE_FALSE(editor->editBlock({0, 0}, uomul::MapBlock{}));
    const auto blockEdit = editor->editBlocks({{0, 0}, {1, 1}}, acceptEditedBlock);
    const auto tileEdit = editor->editTiles(smallTileRegion, acceptEditedTile);
    const auto fillResult = editor->fillTiles(smallTileRegion, {.id = 1, .z = 0});
    const auto replaceIdResult = editor->replaceIdsTiles(smallTileRegion, 0, 1);
    const auto replaceZResult = editor->replaceZTiles(smallTileRegion, 0, 1);
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

    REQUIRE_FALSE(editor->editBlock({0, 0}, uomul::MapBlock{}));
    REQUIRE_FALSE(editor->editTile({0, 0}, {.id = 1, .z = 0}));
    const auto blockEdit = editor->editBlocks({{0, 0}, {1, 1}}, acceptEditedBlock);
    const auto tileEdit = editor->editTiles(smallTileRegion, acceptEditedTile);
    const auto fillResult = editor->fillTiles(smallTileRegion, {.id = 1, .z = 0});
    const auto replaceIdResult = editor->replaceIdsTiles(smallTileRegion, 0, 1);
    const auto replaceZResult = editor->replaceZTiles(smallTileRegion, 0, 1);
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
