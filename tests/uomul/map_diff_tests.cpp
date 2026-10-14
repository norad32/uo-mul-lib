#include "map_test_helpers.h"
#include "uomul/map.h"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>

namespace
{

using uomul::test::dimensions;
using uomul::test::readBytes;
using uomul::test::TestDirectory;
using uomul::test::writeFixture;

constexpr std::uint32_t blockGridSize = dimensions.widthTiles / uomul::landBlockDimension;
constexpr std::uint32_t rectangularWidthBlocks = 3;
constexpr std::uint32_t rectangularWidthTiles = rectangularWidthBlocks * uomul::landBlockDimension;
constexpr std::uint32_t rectangularBlockCount = rectangularWidthBlocks * blockGridSize;
constexpr std::uint32_t secondBlockCoordinate = blockGridSize;
constexpr std::uint32_t expectedChangedBlockId = secondBlockCoordinate * blockGridSize;
constexpr std::size_t mapBlockIdByteCount = sizeof(std::uint32_t);
constexpr std::size_t mapBlockByteCount = 196;
constexpr std::size_t duplicateBlockIdCount = 2;
constexpr std::uint16_t changedTileId = 1234;
constexpr std::int8_t changedTileElevation = -20;

} // namespace

TEST_CASE("MapDiff_Should_RejectDuplicateAndInvalidBlockIDs")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto output = directory.path() / "output.mul";
    const auto diffDirectory = directory.path() / "diff";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(base);
    {
        std::ofstream data(diffDirectory / "mapdif0.mul", std::ios::binary);
        std::ofstream index(diffDirectory / "mapdifl0.mul", std::ios::binary);
        std::array<char, mapBlockByteCount> block{};
        data.write(block.data(), static_cast<std::streamsize>(block.size()));
        const std::array<char, duplicateBlockIdCount * mapBlockIdByteCount> ids{};
        index.write(ids.data(), static_cast<std::streamsize>(ids.size()));
    }
    REQUIRE_FALSE(uomul::MapDiff::apply(base, diffDirectory, output, 0, dimensions));
    REQUIRE_FALSE(std::filesystem::exists(output));
}

TEST_CASE("MapDiff_Should_GenerateApplyAndReportLandOnlyChanges")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto target = directory.path() / "target.mul";
    const auto applied = directory.path() / "applied.mul";
    const auto diffDirectory = directory.path() / "diff";
    const auto reportPath = directory.path() / "report.csv";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(base);
    writeFixture(target);
    auto edit = uomul::MapEditor::create(target, directory.path() / "target-edited.mul", dimensions);
    REQUIRE(edit);
    REQUIRE(edit->writeLandTile({9, 9}, {.id = changedTileId, .z = changedTileElevation}));
    REQUIRE(edit->commit());
    const auto editedTarget = directory.path() / "target-edited.mul";

    const auto generated = uomul::MapDiff::generate(base, editedTarget, diffDirectory, 2, dimensions);
    REQUIRE(generated);
    REQUIRE(generated->landBlocksChanged == 1);
    REQUIRE(generated->landTilesChanged == 1);
    REQUIRE(uomul::MapDiff::apply(base, diffDirectory, applied, 2, dimensions));
    REQUIRE(readBytes(applied) == readBytes(editedTarget));
    const auto report = uomul::MapDiff::report(base, editedTarget, reportPath, dimensions);
    REQUIRE(report);
    REQUIRE(report->landTilesChanged == 1);
    REQUIRE(readBytes(reportPath).find("3,1,1,1") != std::string::npos);
}

TEST_CASE("generate_Should_WriteEmptyDiff_If_MapsAreIdentical")
{
    const TestDirectory directory;
    const auto base = directory.path() / "base.mul";
    const auto target = directory.path() / "target.mul";
    const auto applied = directory.path() / "applied.mul";
    const auto diffDirectory = directory.path() / "diff";
    std::filesystem::create_directories(diffDirectory);
    writeFixture(base);
    writeFixture(target);

    const auto generated = uomul::MapDiff::generate(base, target, diffDirectory, 0, dimensions);
    REQUIRE(generated);
    REQUIRE(generated->landBlocksChanged == 0);
    REQUIRE(generated->landTilesChanged == 0);
    REQUIRE(std::filesystem::file_size(diffDirectory / "mapdif0.mul") == 0);
    REQUIRE(std::filesystem::file_size(diffDirectory / "mapdifl0.mul") == 0);
    REQUIRE(uomul::MapDiff::apply(base, diffDirectory, applied, 0, dimensions));
    REQUIRE(readBytes(applied) == readBytes(base));
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
    writeFixture(base, rectangularBlockCount);
    writeFixture(target, rectangularBlockCount);
    auto editor = uomul::MapEditor::create(target, editedTarget, rectangularDimensions);
    REQUIRE(editor);
    REQUIRE(editor->writeLandTile({.x = secondBlockCoordinate * uomul::landBlockDimension, .y = 0},
                                  {.id = changedTileId, .z = changedTileElevation}));
    REQUIRE(editor->commit());

    const auto generated = uomul::MapDiff::generate(base, editedTarget, diffDirectory, 0, rectangularDimensions);
    REQUIRE(generated);
    REQUIRE(generated->landBlocksChanged == 1);
    const std::array<char, mapBlockIdByteCount> expectedBlockId{static_cast<char>(expectedChangedBlockId), 0, 0, 0};
    REQUIRE(readBytes(diffDirectory / "mapdifl0.mul") == std::string(expectedBlockId.data(), expectedBlockId.size()));
    REQUIRE(uomul::MapDiff::apply(base, diffDirectory, applied, 0, rectangularDimensions));
    REQUIRE(readBytes(applied) == readBytes(editedTarget));
}
