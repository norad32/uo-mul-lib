#include "map_test_helpers.h"
#include "uomul/map.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <ios>
#include <string>
#include <utility>
#include <vector>

namespace
{

using uomul::test::dimensions;
using uomul::test::TestDirectory;
using uomul::test::writeFixture;

constexpr std::uint32_t mapIndex = 2;
constexpr std::uint16_t changedTileId = 1234;
constexpr std::int8_t changedTileElevation = -20;

[[nodiscard]] std::filesystem::path diffDataPath(const std::filesystem::path & directory, std::uint32_t index = mapIndex)
{
    return directory / ("mapdif" + std::to_string(index) + ".mul");
}

[[nodiscard]] std::filesystem::path diffIndexPath(const std::filesystem::path & directory, std::uint32_t index = mapIndex)
{
    return directory / ("mapdifl" + std::to_string(index) + ".mul");
}

void writeBytes(const std::filesystem::path & path, const std::string & bytes)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void writeIndex(const std::filesystem::path & path, std::initializer_list<std::uint32_t> ids)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    for (const auto id : ids)
    {
        const char bytes[]{static_cast<char>(id & 0xffU),
                           static_cast<char>((id >> 8U) & 0xffU),
                           static_cast<char>((id >> 16U) & 0xffU),
                           static_cast<char>((id >> 24U) & 0xffU)};
        output.write(bytes, sizeof(bytes));
    }
}

[[nodiscard]] bool createChangedMap(const std::filesystem::path & source,
                                    const std::filesystem::path & target,
                                    std::vector<std::pair<uomul::TileCoord, uomul::MapTile>> edits)
{
    auto editor = uomul::MapEditor::create(source, target, dimensions);
    if (!editor)
    {
        return false;
    }
    for (const auto & [coordinate, tile] : edits)
    {
        if (auto result = editor->editTile(coordinate, tile); !result)
        {
            return false;
        }
    }
    return static_cast<bool>(editor->commit());
}

} // namespace

TEST_CASE("generate_Should_ReportNoChanges_If_MapsAreIdentical")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto target = directory.path() / "target.mul";
    const auto diffDirectory = directory.path() / "diff";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(base);
    writeFixture(target);

    const auto generated = uomul::MapDiff::generate(base, target, diffDirectory, mapIndex, dimensions);

    REQUIRE(generated);
    REQUIRE(generated->blocksChanged == 0);
    REQUIRE(generated->tilesChanged == 0);
    REQUIRE(std::filesystem::exists(diffDataPath(diffDirectory)));
    REQUIRE(std::filesystem::exists(diffIndexPath(diffDirectory)));
    REQUIRE(std::filesystem::file_size(diffDataPath(diffDirectory)) == 0);
    REQUIRE(std::filesystem::file_size(diffIndexPath(diffDirectory)) == 0);
}

TEST_CASE("generate_Should_ReportChangedBlocksAndTiles_If_TargetContainsMultipleTileEdits")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto target = directory.path() / "target.mul";
    const auto diffDirectory = directory.path() / "diff";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(base);
    writeFixture(target);
    REQUIRE(createChangedMap(target,
                             directory.path() / "edited.mul",
                             {{{1, 1}, {.id = changedTileId, .z = changedTileElevation}},
                              {{9, 9}, {.id = 4321, .z = 12}},
                              {{9, 10}, {.id = 876, .z = -5}}}));
    std::filesystem::rename(directory.path() / "edited.mul", target);

    const auto generated = uomul::MapDiff::generate(base, target, diffDirectory, mapIndex, dimensions);

    REQUIRE(generated);
    REQUIRE(generated->blocksChanged == 2);
    REQUIRE(generated->tilesChanged == 3);
    REQUIRE(std::filesystem::file_size(diffDataPath(diffDirectory)) == 2 * uomul::mapBlockByteCount);
    REQUIRE(std::filesystem::file_size(diffIndexPath(diffDirectory)) == 2 * sizeof(std::uint32_t));
    REQUIRE(uomul::test::readBytes(diffIndexPath(diffDirectory)) == std::string("\0\0\0\0\3\0\0\0", 8));
}

TEST_CASE("generate_Should_RejectMissingBaseMap_If_SourceDoesNotExist")
{
    const TestDirectory directory;
    const auto target = directory.path() / "target.mul";
    const auto diffDirectory = directory.path() / "diff";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(target);

    const auto generated =
        uomul::MapDiff::generate(directory.path() / "missing.mul", target, diffDirectory, mapIndex, dimensions);

    REQUIRE_FALSE(generated);
    REQUIRE(generated.error().code == uomul::ErrorCode::io);
    REQUIRE_FALSE(std::filesystem::exists(diffDataPath(diffDirectory)));
}

TEST_CASE("generate_Should_RejectMissingTargetMap_If_TargetDoesNotExist")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto diffDirectory = directory.path() / "diff";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(base);

    const auto generated =
        uomul::MapDiff::generate(base, directory.path() / "missing.mul", diffDirectory, mapIndex, dimensions);

    REQUIRE_FALSE(generated);
    REQUIRE(generated.error().code == uomul::ErrorCode::io);
    REQUIRE_FALSE(std::filesystem::exists(diffDataPath(diffDirectory)));
}

TEST_CASE("generate_Should_RejectInvalidDimensions_If_DimensionsAreNotBlockAligned")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto diffDirectory = directory.path() / "diff";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(base);

    const auto generated = uomul::MapDiff::generate(base, base, diffDirectory, mapIndex, {.widthTiles = 15, .heightTiles = 16});

    REQUIRE_FALSE(generated);
    REQUIRE(generated.error().code == uomul::ErrorCode::invalidDimensions);
}

TEST_CASE("generate_Should_RejectOutputDirectory_If_DirectoryDoesNotExistOrIsNotDirectory")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    writeFixture(base);
    const auto missing = uomul::MapDiff::generate(base, base, directory.path() / "missing", mapIndex, dimensions);
    REQUIRE_FALSE(missing);
    REQUIRE(missing.error().code == uomul::ErrorCode::io);

    const auto notDirectory = uomul::MapDiff::generate(base, base, base, mapIndex, dimensions);
    REQUIRE_FALSE(notDirectory);
    REQUIRE(notDirectory.error().code == uomul::ErrorCode::io);
}

TEST_CASE("generate_Should_RejectExistingDataFile_If_OverwriteIsDisabled")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto diffDirectory = directory.path() / "diff";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(base);
    writeBytes(diffDataPath(diffDirectory), "existing data");

    const auto generated = uomul::MapDiff::generate(base, base, diffDirectory, mapIndex, dimensions);

    REQUIRE_FALSE(generated);
    REQUIRE(generated.error().code == uomul::ErrorCode::io);
    REQUIRE(uomul::test::readBytes(diffDataPath(diffDirectory)) == "existing data");
}

TEST_CASE("generate_Should_RejectExistingIndexFile_If_OverwriteIsDisabled")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto diffDirectory = directory.path() / "diff";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(base);
    writeBytes(diffIndexPath(diffDirectory), "existing index");

    const auto generated = uomul::MapDiff::generate(base, base, diffDirectory, mapIndex, dimensions);

    REQUIRE_FALSE(generated);
    REQUIRE(generated.error().code == uomul::ErrorCode::io);
    REQUIRE(uomul::test::readBytes(diffIndexPath(diffDirectory)) == "existing index");
}

TEST_CASE("generate_Should_ReplaceExistingDiffPair_If_OverwriteIsEnabled")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto diffDirectory = directory.path() / "diff";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(base);
    writeBytes(diffDataPath(diffDirectory), "old data");
    writeBytes(diffIndexPath(diffDirectory), "old index");

    const auto generated = uomul::MapDiff::generate(base, base, diffDirectory, mapIndex, dimensions, true);

    REQUIRE(generated);
    REQUIRE(generated->blocksChanged == 0);
    REQUIRE(generated->tilesChanged == 0);
    REQUIRE(std::filesystem::file_size(diffDataPath(diffDirectory)) == 0);
    REQUIRE(std::filesystem::file_size(diffIndexPath(diffDirectory)) == 0);
}

TEST_CASE("apply_Should_CopyBaseWithoutChanges_If_DiffPairIsEmpty")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto diffDirectory = directory.path() / "diff";
    const auto output = directory.path() / "output.mul";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(base);
    writeFixture(directory.path() / "identical.mul");
    REQUIRE(uomul::MapDiff::generate(base, directory.path() / "identical.mul", diffDirectory, mapIndex, dimensions));

    const auto applied = uomul::MapDiff::apply(base, diffDirectory, output, mapIndex, dimensions);

    REQUIRE(applied);
    REQUIRE(uomul::test::readBytes(output) == uomul::test::readBytes(base));
}

TEST_CASE("generateAndApply_Should_RoundTripChangedBlocks_If_TargetDiffersFromBase")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto target = directory.path() / "target.mul";
    const auto diffDirectory = directory.path() / "diff";
    const auto output = directory.path() / "output.mul";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(base);
    writeFixture(target);
    REQUIRE(createChangedMap(target,
                             directory.path() / "edited.mul",
                             {{{1, 1}, {.id = changedTileId, .z = changedTileElevation}}, {{9, 9}, {.id = 4321, .z = 12}}}));
    std::filesystem::rename(directory.path() / "edited.mul", target);
    const auto generated = uomul::MapDiff::generate(base, target, diffDirectory, mapIndex, dimensions);
    REQUIRE(generated);

    const auto applied = uomul::MapDiff::apply(base, diffDirectory, output, mapIndex, dimensions);

    REQUIRE(applied);
    REQUIRE(uomul::test::readBytes(output) == uomul::test::readBytes(target));
}

TEST_CASE("generateTilesAndApply_Should_OnlyPatchSelectedTiles_If_RegionCutsAcrossBlocks")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto target = directory.path() / "target.mul";
    const auto diffDirectory = directory.path() / "diff";
    const auto output = directory.path() / "output.mul";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(base);
    writeFixture(target);
    REQUIRE(createChangedMap(target,
                             directory.path() / "edited.mul",
                             {{{0, 0}, {.id = 5000, .z = 10}},
                              {{1, 1}, {.id = 5001, .z = 11}},
                              {{7, 7}, {.id = 5002, .z = 12}},
                              {{8, 8}, {.id = 5003, .z = 13}}}));
    std::filesystem::rename(directory.path() / "edited.mul", target);
    const uomul::TileRegion region{.min = {1, 1}, .maxExclusive = {4, 4}};

    const auto generated = uomul::MapDiff::generateTiles(base, target, diffDirectory, mapIndex, dimensions, region);

    REQUIRE(generated);
    REQUIRE(generated->blocksChanged == 1);
    REQUIRE(generated->tilesChanged == 1);
    REQUIRE(std::filesystem::file_size(diffDataPath(diffDirectory)) == uomul::mapBlockByteCount);
    REQUIRE(std::filesystem::file_size(diffIndexPath(diffDirectory)) == sizeof(std::uint32_t));
    REQUIRE(uomul::MapDiff::apply(base, diffDirectory, output, mapIndex, dimensions));

    const auto result = uomul::MapReader::open(output, dimensions);
    const auto original = uomul::MapReader::open(base, dimensions);
    const auto targetReader = uomul::MapReader::open(target, dimensions);
    REQUIRE(result);
    REQUIRE(original);
    REQUIRE(targetReader);
    for (std::uint32_t x = 0; x < dimensions.widthTiles; ++x)
    {
        for (std::uint32_t y = 0; y < dimensions.heightTiles; ++y)
        {
            const uomul::TileCoord tile{.x = x, .y = y};
            const bool selected =
                x >= region.min.x && x < region.maxExclusive.x && y >= region.min.y && y < region.maxExclusive.y;
            const auto expected = selected ? targetReader->readTile(tile).value() : original->readTile(tile).value();
            REQUIRE(result->readTile(tile).value() == expected);
        }
    }
}

TEST_CASE("generateTilesAndApply_Should_FastPathWholeBlocks_If_RegionIsBlockAligned")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto target = directory.path() / "target.mul";
    const auto diffDirectory = directory.path() / "diff";
    const auto output = directory.path() / "output.mul";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(base);
    writeFixture(target);
    REQUIRE(createChangedMap(
        target, directory.path() / "edited.mul", {{{1, 1}, {.id = 5000, .z = 10}}, {{9, 9}, {.id = 5001, .z = 11}}}));
    std::filesystem::rename(directory.path() / "edited.mul", target);
    const uomul::TileRegion region{.min = {0, 0}, .maxExclusive = {8, 8}};

    const auto generated = uomul::MapDiff::generateTiles(base, target, diffDirectory, mapIndex, dimensions, region);

    REQUIRE(generated);
    REQUIRE(generated->blocksChanged == 1);
    REQUIRE(generated->tilesChanged == 1);
    REQUIRE(uomul::test::readBytes(diffIndexPath(diffDirectory)) == std::string(4, '\0'));
    REQUIRE(uomul::MapDiff::apply(base, diffDirectory, output, mapIndex, dimensions));
    const auto result = uomul::MapReader::open(output, dimensions);
    const auto original = uomul::MapReader::open(base, dimensions);
    REQUIRE(result);
    REQUIRE(original);
    REQUIRE(result->readTile({1, 1}) == uomul::MapTile{.id = 5000, .z = 10});
    REQUIRE(result->readTile({9, 9}).value() == original->readTile({9, 9}).value());
}

TEST_CASE("generateBlocksAndApply_Should_OnlyPatchSelectedBlocks")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto target = directory.path() / "target.mul";
    const auto diffDirectory = directory.path() / "diff";
    const auto output = directory.path() / "output.mul";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(base);
    writeFixture(target);
    REQUIRE(createChangedMap(
        target, directory.path() / "edited.mul", {{{1, 1}, {.id = 5000, .z = 10}}, {{9, 9}, {.id = 5001, .z = 11}}}));
    std::filesystem::rename(directory.path() / "edited.mul", target);

    const auto generated = uomul::MapDiff::generateBlocks(
        base, target, diffDirectory, mapIndex, dimensions, {.min = {0, 0}, .maxExclusive = {1, 1}});

    REQUIRE(generated);
    REQUIRE(generated->blocksChanged == 1);
    REQUIRE(generated->tilesChanged == 1);
    REQUIRE(uomul::MapDiff::apply(base, diffDirectory, output, mapIndex, dimensions));
    const auto result = uomul::MapReader::open(output, dimensions);
    const auto original = uomul::MapReader::open(base, dimensions);
    REQUIRE(result);
    REQUIRE(original);
    REQUIRE(result->readTile({1, 1}) == uomul::MapTile{.id = 5000, .z = 10});
    REQUIRE(result->readTile({9, 9}).value() == original->readTile({9, 9}).value());
}

TEST_CASE("apply_Should_RejectDiff_If_DataOrIndexFileIsMissing")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto diffDirectory = directory.path() / "diff";
    const auto output = directory.path() / "output.mul";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(base);

    const auto missingBoth = uomul::MapDiff::apply(base, diffDirectory, output, mapIndex, dimensions);
    REQUIRE_FALSE(missingBoth);
    REQUIRE(missingBoth.error().code == uomul::ErrorCode::io);

    writeBytes(diffDataPath(diffDirectory), {});
    const auto missingIndex = uomul::MapDiff::apply(base, diffDirectory, output, mapIndex, dimensions);
    REQUIRE_FALSE(missingIndex);
    REQUIRE(missingIndex.error().code == uomul::ErrorCode::io);

    std::filesystem::remove(diffDataPath(diffDirectory));
    writeBytes(diffIndexPath(diffDirectory), {});
    const auto missingData = uomul::MapDiff::apply(base, diffDirectory, output, mapIndex, dimensions);
    REQUIRE_FALSE(missingData);
    REQUIRE(missingData.error().code == uomul::ErrorCode::io);
    REQUIRE_FALSE(std::filesystem::exists(output));
}

TEST_CASE("apply_Should_RejectMalformedLengths_If_DataAndIndexCountsDoNotMatch")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto diffDirectory = directory.path() / "diff";
    const auto output = directory.path() / "output.mul";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(base);
    writeBytes(diffDataPath(diffDirectory), std::string(uomul::mapBlockByteCount, '\0'));
    writeBytes(diffIndexPath(diffDirectory), {});

    const auto applied = uomul::MapDiff::apply(base, diffDirectory, output, mapIndex, dimensions);

    REQUIRE_FALSE(applied);
    REQUIRE(applied.error().code == uomul::ErrorCode::truncatedInput);
    REQUIRE_FALSE(std::filesystem::exists(output));
}

TEST_CASE("apply_Should_RejectMalformedLengths_If_IndexHasPartialBlockId")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto diffDirectory = directory.path() / "diff";
    const auto output = directory.path() / "output.mul";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(base);
    writeBytes(diffDataPath(diffDirectory), {});
    writeBytes(diffIndexPath(diffDirectory), "x");

    const auto applied = uomul::MapDiff::apply(base, diffDirectory, output, mapIndex, dimensions);

    REQUIRE_FALSE(applied);
    REQUIRE(applied.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("apply_Should_RejectInvalidBlockId_If_IndexContainsOutOfRangeId")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto diffDirectory = directory.path() / "diff";
    const auto output = directory.path() / "output.mul";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(base);
    writeBytes(diffDataPath(diffDirectory), std::string(uomul::mapBlockByteCount, '\0'));
    writeIndex(diffIndexPath(diffDirectory), {4});

    const auto applied = uomul::MapDiff::apply(base, diffDirectory, output, mapIndex, dimensions);

    REQUIRE_FALSE(applied);
    REQUIRE(applied.error().code == uomul::ErrorCode::invalidBlockCount);
    REQUIRE_FALSE(std::filesystem::exists(output));
}

TEST_CASE("apply_Should_RejectInvalidBlockId_If_IndexContainsDuplicateIds")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto diffDirectory = directory.path() / "diff";
    const auto output = directory.path() / "output.mul";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(base);
    writeBytes(diffDataPath(diffDirectory), std::string(2 * uomul::mapBlockByteCount, '\0'));
    writeIndex(diffIndexPath(diffDirectory), {0, 0});

    const auto applied = uomul::MapDiff::apply(base, diffDirectory, output, mapIndex, dimensions);

    REQUIRE_FALSE(applied);
    REQUIRE(applied.error().code == uomul::ErrorCode::invalidBlockCount);
}

TEST_CASE("apply_Should_RejectTruncatedDiffBlock_If_DataEndsBeforeBlockIsComplete")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto diffDirectory = directory.path() / "diff";
    const auto output = directory.path() / "output.mul";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(base);
    writeBytes(diffDataPath(diffDirectory), std::string(uomul::mapBlockByteCount - 1, '\0'));
    writeIndex(diffIndexPath(diffDirectory), {0});

    const auto applied = uomul::MapDiff::apply(base, diffDirectory, output, mapIndex, dimensions);

    REQUIRE_FALSE(applied);
    REQUIRE(applied.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("apply_Should_RejectMissingBaseMap_If_BaseDoesNotExist")
{
    const TestDirectory directory;
    const auto diffDirectory = directory.path() / "diff";
    std::filesystem::create_directories(diffDirectory);
    writeBytes(diffDataPath(diffDirectory), {});
    writeBytes(diffIndexPath(diffDirectory), {});

    const auto applied = uomul::MapDiff::apply(
        directory.path() / "missing.mul", diffDirectory, directory.path() / "output.mul", mapIndex, dimensions);

    REQUIRE_FALSE(applied);
    REQUIRE(applied.error().code == uomul::ErrorCode::io);
}

TEST_CASE("apply_Should_RejectInvalidDimensions_If_DimensionsAreNotBlockAligned")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto diffDirectory = directory.path() / "diff";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(base);

    const auto applied = uomul::MapDiff::apply(
        base, diffDirectory, directory.path() / "output.mul", mapIndex, {.widthTiles = 15, .heightTiles = 16});

    REQUIRE_FALSE(applied);
    REQUIRE(applied.error().code == uomul::ErrorCode::invalidDimensions);
}

TEST_CASE("apply_Should_RejectExistingOutput_If_OverwriteIsDisabled")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto target = directory.path() / "target.mul";
    const auto diffDirectory = directory.path() / "diff";
    const auto output = directory.path() / "output.mul";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(base);
    writeFixture(target);
    REQUIRE(uomul::MapDiff::generate(base, target, diffDirectory, mapIndex, dimensions));
    writeBytes(output, "existing map");

    const auto applied = uomul::MapDiff::apply(base, diffDirectory, output, mapIndex, dimensions);

    REQUIRE_FALSE(applied);
    REQUIRE(applied.error().code == uomul::ErrorCode::io);
    REQUIRE(uomul::test::readBytes(output) == "existing map");
}

TEST_CASE("apply_Should_ReplaceExistingOutput_If_OverwriteIsEnabled")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto target = directory.path() / "target.mul";
    const auto diffDirectory = directory.path() / "diff";
    const auto output = directory.path() / "output.mul";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(base);
    writeFixture(target);
    REQUIRE(createChangedMap(
        target, directory.path() / "edited.mul", {{{1, 1}, {.id = changedTileId, .z = changedTileElevation}}}));
    std::filesystem::rename(directory.path() / "edited.mul", target);
    REQUIRE(uomul::MapDiff::generate(base, target, diffDirectory, mapIndex, dimensions));
    writeBytes(output, "existing map");

    const auto applied = uomul::MapDiff::apply(base, diffDirectory, output, mapIndex, dimensions, true);

    REQUIRE(applied);
    REQUIRE(uomul::test::readBytes(output) == uomul::test::readBytes(target));
}

TEST_CASE("report_Should_CountChangedTiles_If_MapsDiffer")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto target = directory.path() / "target.mul";
    const auto reportPath = directory.path() / "report.csv";
    writeFixture(base);
    writeFixture(target);
    REQUIRE(createChangedMap(
        target, directory.path() / "edited.mul", {{{9, 9}, {.id = changedTileId, .z = changedTileElevation}}}));
    std::filesystem::rename(directory.path() / "edited.mul", target);

    const auto report = uomul::MapDiff::report(base, target, reportPath, dimensions);

    REQUIRE(report);
    REQUIRE(report->blocksChanged == 1);
    REQUIRE(report->tilesChanged == 1);
    REQUIRE(uomul::test::readBytes(reportPath) == "blockId,blockX,blockY,changedTiles\n3,1,1,1\n");
}

TEST_CASE("report_Should_ReportNoChanges_If_MapsAreIdentical")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto target = directory.path() / "target.mul";
    const auto reportPath = directory.path() / "report.csv";
    writeFixture(base);
    writeFixture(target);

    const auto report = uomul::MapDiff::report(base, target, reportPath, dimensions);

    REQUIRE(report);
    REQUIRE(report->blocksChanged == 0);
    REQUIRE(report->tilesChanged == 0);
    REQUIRE(uomul::test::readBytes(reportPath) == "blockId,blockX,blockY,changedTiles\n");
}

TEST_CASE("report_Should_RejectExistingOutput_If_OverwriteIsDisabled")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto reportPath = directory.path() / "report.csv";
    writeFixture(base);
    writeBytes(reportPath, "existing report");

    const auto report = uomul::MapDiff::report(base, base, reportPath, dimensions);

    REQUIRE_FALSE(report);
    REQUIRE(report.error().code == uomul::ErrorCode::io);
    REQUIRE(uomul::test::readBytes(reportPath) == "existing report");
}

TEST_CASE("report_Should_ReplaceExistingOutput_If_OverwriteIsEnabled")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto reportPath = directory.path() / "report.csv";
    writeFixture(base);
    writeBytes(reportPath, "existing report");

    const auto report = uomul::MapDiff::report(base, base, reportPath, dimensions, true);

    REQUIRE(report);
    REQUIRE(uomul::test::readBytes(reportPath) == "blockId,blockX,blockY,changedTiles\n");
}

TEST_CASE("report_Should_RejectInvalidDimensions_If_DimensionsAreNotBlockAligned")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto reportPath = directory.path() / "report.csv";
    writeFixture(base);

    const auto report = uomul::MapDiff::report(base, base, reportPath, {.widthTiles = 15, .heightTiles = 16});

    REQUIRE_FALSE(report);
    REQUIRE(report.error().code == uomul::ErrorCode::invalidDimensions);
}

TEST_CASE("report_Should_RejectMissingMaps_If_SourceOrTargetDoesNotExist")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto reportPath = directory.path() / "report.csv";
    writeFixture(base);

    const auto missingSource = uomul::MapDiff::report(directory.path() / "missing.mul", base, reportPath, dimensions);
    REQUIRE_FALSE(missingSource);
    REQUIRE(missingSource.error().code == uomul::ErrorCode::io);

    const auto missingTarget = uomul::MapDiff::report(base, directory.path() / "missing.mul", reportPath, dimensions);
    REQUIRE_FALSE(missingTarget);
    REQUIRE(missingTarget.error().code == uomul::ErrorCode::io);
}

TEST_CASE("report_Should_RejectOutputPath_If_ParentDirectoryDoesNotExist")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    writeFixture(base);

    const auto report = uomul::MapDiff::report(base, base, directory.path() / "missing" / "report.csv", dimensions);

    REQUIRE_FALSE(report);
    REQUIRE(report.error().code == uomul::ErrorCode::io);
}

TEST_CASE("writeCsvReport_Should_CreateReportFile_If_MapsAreComparable")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto target = directory.path() / "target.mul";
    const auto reportPath = directory.path() / "report.csv";
    writeFixture(base);
    writeFixture(target);

    REQUIRE(uomul::writeCsvReport(base, target, reportPath, dimensions));
    REQUIRE(uomul::test::readBytes(reportPath) == "blockId,blockX,blockY,changedTiles\n");
}

TEST_CASE("writeCsvReport_Should_PropagateError_If_OutputAlreadyExists")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto reportPath = directory.path() / "report.csv";
    writeFixture(base);
    writeBytes(reportPath, "existing report");

    const auto result = uomul::writeCsvReport(base, base, reportPath, dimensions);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::io);
}

TEST_CASE("facetDimensions_Should_ReturnKnownPresets_If_NameUsesAnySupportedAlias")
{
    REQUIRE(uomul::facetDimensions("felucca") == uomul::Dimensions{.widthTiles = 6144, .heightTiles = 4096});
    REQUIRE(uomul::facetDimensions("TRAMMEL") == uomul::Dimensions{.widthTiles = 6144, .heightTiles = 4096});
    REQUIRE(uomul::facetDimensions("IlShEnAr") == uomul::Dimensions{.widthTiles = 2304, .heightTiles = 1600});
    REQUIRE(uomul::facetDimensions("malas") == uomul::Dimensions{.widthTiles = 2560, .heightTiles = 2048});
    REQUIRE(uomul::facetDimensions("Tokuno") == uomul::Dimensions{.widthTiles = 1448, .heightTiles = 1448});
    REQUIRE(uomul::facetDimensions("ter-mur") == uomul::Dimensions{.widthTiles = 1280, .heightTiles = 4096});
    REQUIRE(uomul::facetDimensions("TERMUR") == uomul::Dimensions{.widthTiles = 1280, .heightTiles = 4096});
    const auto unknown = uomul::facetDimensions("unknown");
    REQUIRE_FALSE(unknown);
    REQUIRE(unknown.error().code == uomul::ErrorCode::invalidDimensions);
}
