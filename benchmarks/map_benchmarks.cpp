#include "uomul/map.h"
#include "uomul/mul_common/mul_common.h"
#include <algorithm>
#include <array>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unistd.h>

namespace
{

constexpr uomul::Dimensions mapDimensions{.widthTiles = 6144, .heightTiles = 4096};
constexpr std::uint32_t blocksWide = mapDimensions.widthTiles / uomul::mapBlockDimension;
constexpr std::uint32_t blocksHigh = mapDimensions.heightTiles / uomul::mapBlockDimension;
constexpr std::uint64_t blockCount = static_cast<std::uint64_t>(blocksWide) * blocksHigh;
constexpr std::size_t headerBytes = 4;
constexpr std::size_t tileBytes = 3;
constexpr std::uint32_t fixtureIdStride = 251;
constexpr std::size_t fixtureBufferSize = 1U << 20U;
constexpr std::uint32_t tileIdMask = 0xffffU;
constexpr std::uint32_t byteMask = 0xffU;
constexpr std::uint32_t byteShift = 8U;
constexpr std::uint16_t tileIdBenchmarkValue = 42;
constexpr std::uint16_t firstFillTileId = 7;
constexpr std::uint16_t secondFillTileId = 9;
constexpr std::int8_t tileBenchmarkElevation = 7;
constexpr std::int8_t firstFillElevation = 2;
constexpr std::int8_t secondFillElevation = 3;
constexpr uomul::TileCoord benchmarkTileCoordinate{
    .x = mapDimensions.widthTiles / 2,
    .y = mapDimensions.heightTiles / 2,
};

class BenchmarkFiles
{
public:
    BenchmarkFiles()
    {
        static std::uint64_t sequence = 0;
        directory_ = std::filesystem::temp_directory_path() /
                     ("uomul-map-benchmark-" + std::to_string(::getpid()) + "-" + std::to_string(sequence++));
        std::filesystem::create_directories(directory_);
        base_ = directory_ / "base.mul";
        changed_ = directory_ / "changed.mul";
        dense_ = directory_ / "dense.mul";
        writeMap(base_, false, false);
        writeMap(changed_, true, false);
        writeMap(dense_, false, true);
    }

    BenchmarkFiles(const BenchmarkFiles &) = delete;
    BenchmarkFiles & operator=(const BenchmarkFiles &) = delete;
    BenchmarkFiles(BenchmarkFiles &&) = delete;
    BenchmarkFiles & operator=(BenchmarkFiles &&) = delete;

    ~BenchmarkFiles()
    {
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
    }

    [[nodiscard]] const std::filesystem::path & directory() const { return directory_; }
    [[nodiscard]] const std::filesystem::path & base() const { return base_; }
    [[nodiscard]] const std::filesystem::path & changed() const { return changed_; }
    [[nodiscard]] const std::filesystem::path & dense() const { return dense_; }

private:
    static void writeMap(const std::filesystem::path & path, bool changeOneBlock, bool changeAllTiles)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            throw std::runtime_error("could not create full-size map fixture");
        }
        std::array<char, fixtureBufferSize> buffer{};
        std::size_t buffered = 0;
        for (std::uint64_t block = 0; block < blockCount; block++)
        {
            const std::array<char, headerBytes> header{
                static_cast<char>(block & byteMask),
                static_cast<char>((block >> byteShift) & byteMask),
                static_cast<char>((block >> (2U * byteShift)) & byteMask),
                static_cast<char>((block >> (3U * byteShift)) & byteMask),
            };
            append(output, buffer, buffered, header.data(), header.size());
            for (std::uint32_t tile = 0; tile < uomul::mapBlockTileCount; tile++)
            {
                const auto id = static_cast<std::uint16_t>(((block * fixtureIdStride) + tile) & tileIdMask);
                const bool changedTile = changeAllTiles || (changeOneBlock && block == 0 && tile == 0);
                const auto tileId = static_cast<std::uint16_t>(id + (changedTile ? 1U : 0U));
                const auto unsignedTileId = static_cast<std::uint32_t>(tileId);
                const std::array<char, tileBytes> record{
                    static_cast<char>(unsignedTileId & byteMask),
                    static_cast<char>((unsignedTileId >> byteShift) & byteMask),
                    static_cast<char>(static_cast<std::int8_t>(static_cast<std::int32_t>(tile % 127U) - 63)),
                };
                append(output, buffer, buffered, record.data(), record.size());
            }
        }
        if (buffered != 0)
        {
            output.write(buffer.data(), static_cast<std::streamsize>(buffered));
        }
        output.flush();
        if (!output)
        {
            throw std::runtime_error("could not write full-size map fixture");
        }
    }

    static void append(std::ofstream & output,
                       std::array<char, fixtureBufferSize> & buffer,
                       std::size_t & buffered,
                       const char * data,
                       std::size_t size)
    {
        while (size > 0)
        {
            const auto available = buffer.size() - buffered;
            const auto amount = size < available ? size : available;
            std::copy_n(data, amount, buffer.data() + buffered);
            buffered += amount;
            data += amount;
            size -= amount;
            if (buffered == buffer.size())
            {
                output.write(buffer.data(), static_cast<std::streamsize>(buffered));
                buffered = 0;
            }
        }
    }

    std::filesystem::path directory_;
    std::filesystem::path base_;
    std::filesystem::path changed_;
    std::filesystem::path dense_;
};

void requireSuccess(const auto & result)
{
    if (!result)
    {
        throw std::runtime_error(result.error().message);
    }
}

constexpr uomul::BlockRegion fullBlocks{
    .min = {.x = 0, .y = 0},
    .maxExclusive = {.x = blocksWide, .y = blocksHigh},
};
constexpr uomul::TileRegion fullTiles{
    .min = {.x = 0, .y = 0},
    .maxExclusive = {.x = mapDimensions.widthTiles, .y = mapDimensions.heightTiles},
};
constexpr uomul::BlockRegion quarterBlocks{
    .min = {.x = 0, .y = 0},
    .maxExclusive = {.x = blocksWide / 2, .y = blocksHigh / 2},
};
constexpr uomul::TileRegion quarterTiles{
    .min = {.x = 0, .y = 0},
    .maxExclusive = {.x = mapDimensions.widthTiles / 2, .y = mapDimensions.heightTiles / 2},
};

const BenchmarkFiles & benchmarkFiles()
{
    static const BenchmarkFiles files;
    return files;
}

uomul::MapBlock readReferenceBlock(const BenchmarkFiles & files)
{
    const auto readerResult = uomul::MapReader::open(files.base(), mapDimensions);
    REQUIRE(readerResult);
    const auto reference = readerResult->readBlock({.x = 0, .y = 0});
    REQUIRE(reference);
    return *reference;
}

} // namespace

TEST_CASE("MapReader Trammel benchmarks", "[benchmark][map]")
{
    const auto & files = benchmarkFiles();
    const auto readerResult = uomul::MapReader::open(files.base(), mapDimensions);
    REQUIRE(readerResult);
    const auto & reader = *readerResult;

    BENCHMARK("Trammel/full/open-reader")
    {
        auto result = uomul::MapReader::open(files.base(), mapDimensions);
        requireSuccess(result);
        return result->completeBlocks();
    };

    BENCHMARK("Trammel/full/read-metadata")
    {
        return reader.dimensions().widthTiles + reader.dimensions().heightTiles +
               static_cast<std::uint32_t>(reader.completeBlocks()) + static_cast<std::uint32_t>(reader.ignoredTrailingBytes()) +
               static_cast<std::uint32_t>(reader.missingBlocks());
    };

    BENCHMARK("Trammel/one-tile/read")
    {
        auto result = reader.readTile(benchmarkTileCoordinate);
        requireSuccess(result);
        return result->id;
    };

    BENCHMARK("Trammel/one-block/read")
    {
        auto result = reader.readBlock({.x = blocksWide / 2, .y = blocksHigh / 2});
        requireSuccess(result);
        return result->at(0).id;
    };

    BENCHMARK("Trammel/16x16-blocks/read")
    {
        std::uint64_t visited = 0;
        const auto result =
            reader.readBlocks({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 16, .y = 16}},
                              [&visited](uomul::BlockCoord, const uomul::MapBlock &) -> std::expected<void, uomul::Error>
                              {
                                  visited++;
                                  return {};
                              });
        requireSuccess(result);
        return visited;
    };

    BENCHMARK("Trammel/full/read-raw-blocks")
    {
        std::uint64_t visited = 0;
        const auto result = reader.readRawBlocks(
            fullBlocks,
            [&visited](uomul::BlockCoord, const uomul::MapBlockBytes &) -> std::expected<void, uomul::Error>
            {
                visited++;
                return {};
            });
        requireSuccess(result);
        return visited;
    };

    BENCHMARK("Trammel/full/read-decoded-blocks")
    {
        std::uint64_t visited = 0;
        const auto result =
            reader.readBlocks(fullBlocks,
                              [&visited](uomul::BlockCoord, const uomul::MapBlock &) -> std::expected<void, uomul::Error>
                              {
                                  visited++;
                                  return {};
                              });
        requireSuccess(result);
        return visited;
    };

    BENCHMARK("Trammel/full/read-tiles")
    {
        std::uint64_t visited = 0;
        const auto result =
            reader.readTiles(fullTiles,
                             [&visited](uomul::TileCoord, const uomul::MapTile &) -> std::expected<void, uomul::Error>
                             {
                                 visited++;
                                 return {};
                             });
        requireSuccess(result);
        return visited;
    };

    BENCHMARK("Trammel/64x64-tiles/read")
    {
        std::uint64_t visited = 0;
        const auto result =
            reader.readTiles({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 64, .y = 64}},
                             [&visited](uomul::TileCoord, const uomul::MapTile &) -> std::expected<void, uomul::Error>
                             {
                                 visited++;
                                 return {};
                             });
        requireSuccess(result);
        return visited;
    };

    BENCHMARK("Trammel/quarter/read-tiles")
    {
        std::uint64_t visited = 0;
        const auto result =
            reader.readTiles(quarterTiles,
                             [&visited](uomul::TileCoord, const uomul::MapTile &) -> std::expected<void, uomul::Error>
                             {
                                 visited++;
                                 return {};
                             });
        requireSuccess(result);
        return visited;
    };

    BENCHMARK("Trammel/full/stats")
    {
        auto result = reader.stats();
        return result->tileCount;
    };

    BENCHMARK("Trammel/full/stats-tiles")
    {
        const auto result = reader.statsTiles(fullTiles);
        return result->tileCount;
    };

    BENCHMARK("Trammel/full/stats-blocks")
    {
        const auto result = reader.statsBlocks(fullBlocks);
        return result->tileCount;
    };

    BENCHMARK("Trammel/64x64-tiles/stats")
    {
        const auto result = reader.statsTiles({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 64, .y = 64}});
        return result->tileCount;
    };

    BENCHMARK("Trammel/quarter/stats-tiles")
    {
        auto result = reader.statsTiles(quarterTiles);
        return result->tileCount;
    };

    BENCHMARK("Trammel/quarter/stats-blocks")
    {
        auto result = reader.statsBlocks(quarterBlocks);
        return result->tileCount;
    };
}

TEST_CASE("MapEditor Trammel benchmarks", "[benchmark][map]")
{
    const auto & files = benchmarkFiles();
    const auto reference = readReferenceBlock(files);

    BENCHMARK("Trammel/full/create-and-stage-copy")
    {
        const auto output = files.directory() / "create-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/one-tile/write")
    {
        const auto output = files.directory() / "write-tile-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->editTile({.x = 0, .y = 0}, {.id = tileIdBenchmarkValue, .z = tileBenchmarkElevation}));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/one-block/edit")
    {
        const auto output = files.directory() / "edit-one-block-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->editBlocks(
            {.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 1, .y = 1}},
            [](uomul::BlockCoord, const uomul::MapBlock & current) -> std::expected<uomul::MapBlock, uomul::Error>
            {
                auto changed = current;
                changed.front().id++;
                return changed;
            }));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/one-tile/edit")
    {
        const auto output = files.directory() / "edit-one-tile-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->editTiles(
            {.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 1, .y = 1}},
            [](uomul::TileCoord, const uomul::MapTile & current) -> std::expected<uomul::MapTile, uomul::Error>
            { return uomul::MapTile{.id = static_cast<std::uint16_t>(current.id + 1U), .z = current.z}; }));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/one-block/write")
    {
        const auto output = files.directory() / "write-block-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->editBlock({.x = 0, .y = 0}, reference));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/one-tile/fill")
    {
        const auto output = files.directory() / "fill-tile-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->fillTiles({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 1, .y = 1}},
                                         {.id = firstFillTileId, .z = firstFillElevation}));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/one-block/fill")
    {
        const auto output = files.directory() / "fill-block-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->fillBlocks({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 1, .y = 1}},
                                          {.id = secondFillTileId, .z = secondFillElevation}));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/full/fill")
    {
        const auto output = files.directory() / "fill-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->fill({.id = firstFillTileId, .z = firstFillElevation}));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/quarter/fill-tiles")
    {
        const auto output = files.directory() / "fill-tile-region-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->fillTiles(quarterTiles, {.id = firstFillTileId, .z = firstFillElevation}));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/quarter/fill-blocks")
    {
        const auto output = files.directory() / "fill-block-region-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->fillBlocks(quarterBlocks, {.id = secondFillTileId, .z = secondFillElevation}));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/full/replace-ids")
    {
        const auto output = files.directory() / "replace-ids-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->replaceIds(0, tileIdBenchmarkValue));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/full/replace-elevations")
    {
        const auto output = files.directory() / "replace-z-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->replaceZ(0, 1));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/full/replace-block-ids")
    {
        const auto output = files.directory() / "replace-block-ids-full-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->replaceIdsBlocks(fullBlocks, 0, tileIdBenchmarkValue));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/full/replace-block-elevations")
    {
        const auto output = files.directory() / "replace-block-z-full-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->replaceZBlocks(fullBlocks, 0, 1));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/quarter/replace-tile-ids")
    {
        const auto output = files.directory() / "replace-tile-ids-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->replaceIdsTiles(quarterTiles, 0, tileIdBenchmarkValue));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/quarter/replace-block-ids")
    {
        const auto output = files.directory() / "replace-block-ids-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->replaceIdsBlocks(quarterBlocks, 0, tileIdBenchmarkValue));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/quarter/replace-tile-elevations")
    {
        const auto output = files.directory() / "replace-tile-z-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->replaceZTiles(quarterTiles, 0, 1));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/quarter/replace-block-elevations")
    {
        const auto output = files.directory() / "replace-block-z-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->replaceZBlocks(quarterBlocks, 0, 1));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/quarter/edit-blocks")
    {
        const auto output = files.directory() / "edit-blocks-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->editBlocks(
            quarterBlocks,
            [](uomul::BlockCoord, const uomul::MapBlock & current) -> std::expected<uomul::MapBlock, uomul::Error>
            {
                auto changed = current;
                changed.front().id++;
                return changed;
            }));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/quarter/edit-tiles")
    {
        const auto output = files.directory() / "edit-tiles-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->editTiles(
            quarterTiles,
            [](uomul::TileCoord, const uomul::MapTile & current) -> std::expected<uomul::MapTile, uomul::Error>
            { return uomul::MapTile{.id = static_cast<std::uint16_t>(current.id + 1U), .z = current.z}; }));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/full/commit-staged-copy")
    {
        const auto output = files.directory() / "commit-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->commit());
        return std::filesystem::file_size(output);
    };
}

TEST_CASE("MapDiff Trammel benchmarks", "[benchmark][map]")
{
    const auto & files = benchmarkFiles();
    const auto diffDirectory = files.directory() / "diff";
    const auto applyDirectory = files.directory() / "apply";
    std::filesystem::create_directories(diffDirectory);
    std::filesystem::create_directories(applyDirectory);

    BENCHMARK("Trammel/full/generate-diff-empty")
    {
        const auto result = uomul::MapDiff::generate(files.base(), files.base(), diffDirectory, 0, mapDimensions, true);
        requireSuccess(result);
        return result->blocksChanged;
    };

    BENCHMARK("Trammel/full/generate-diff-sparse")
    {
        const auto result = uomul::MapDiff::generate(files.base(), files.changed(), diffDirectory, 0, mapDimensions, true);
        requireSuccess(result);
        return result->tilesChanged;
    };

    BENCHMARK("Trammel/full/generate-diff-dense")
    {
        const auto result = uomul::MapDiff::generate(files.base(), files.dense(), diffDirectory, 0, mapDimensions, true);
        requireSuccess(result);
        return result->tilesChanged;
    };

    BENCHMARK("Trammel/one-block/generate-diff-from-tiles")
    {
        constexpr uomul::TileRegion region{
            .min = {.x = 0, .y = 0},
            .maxExclusive = {.x = uomul::mapBlockDimension, .y = uomul::mapBlockDimension},
        };
        const auto result =
            uomul::MapDiff::generateTiles(files.base(), files.changed(), diffDirectory, 0, mapDimensions, region, true);
        requireSuccess(result);
        return result->tilesChanged;
    };

    BENCHMARK("Trammel/quarter/generate-diff-from-tiles")
    {
        const auto result =
            uomul::MapDiff::generateTiles(files.base(), files.changed(), diffDirectory, 0, mapDimensions, quarterTiles, true);
        requireSuccess(result);
        return result->tilesChanged;
    };

    BENCHMARK("Trammel/quarter/generate-diff-from-blocks")
    {
        const auto result =
            uomul::MapDiff::generateBlocks(files.base(), files.changed(), diffDirectory, 0, mapDimensions, quarterBlocks, true);
        requireSuccess(result);
        return result->tilesChanged;
    };

    const auto generated = uomul::MapDiff::generate(files.base(), files.changed(), diffDirectory, 0, mapDimensions, true);
    requireSuccess(generated);

    BENCHMARK("Trammel/full/apply-diff-sparse")
    {
        const auto output = applyDirectory / "applied.mul";
        const auto applied = uomul::MapDiff::apply(files.base(), diffDirectory, output, 0, mapDimensions, true);
        requireSuccess(applied);
        return generated->tilesChanged;
    };

    BENCHMARK("Trammel/full/write-diff-csv-report")
    {
        const auto report = files.directory() / "changes.csv";
        const auto result = uomul::MapDiff::report(files.base(), files.changed(), report, mapDimensions, true);
        requireSuccess(result);
        return result->tilesChanged;
    };
}
