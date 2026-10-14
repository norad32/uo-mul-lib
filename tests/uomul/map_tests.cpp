#include "uomul/map.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
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

constexpr uomul::Dimensions dimensions{.widthTiles = 16, .heightTiles = 16};

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
    auto southern = reader->readLandTile({0, 8});
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
    REQUIRE(reader->readLandTile({0, 0})->id == 0);
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

TEST_CASE("FacetDimensions_Should_ProvideKnownPresets")
{
    REQUIRE(uomul::facetDimensions("felucca") == uomul::Dimensions{.widthTiles = 6144, .heightTiles = 4096});
    REQUIRE(uomul::facetDimensions("tokuno") == uomul::Dimensions{.widthTiles = 1448, .heightTiles = 1448});
    REQUIRE_FALSE(uomul::facetDimensions("unknown"));
}
