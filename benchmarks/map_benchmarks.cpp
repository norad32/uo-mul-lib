#include "uomul/map.h"
#include <algorithm>
#include <array>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unistd.h>

namespace
{

constexpr uomul::Dimensions mapDimensions{.widthTiles = 6144, .heightTiles = 4096};
constexpr std::uint32_t blocksWide = mapDimensions.widthTiles / uomul::landBlockDimension;
constexpr std::uint32_t blocksHigh = mapDimensions.heightTiles / uomul::landBlockDimension;
constexpr std::uint64_t blockCount = static_cast<std::uint64_t>(blocksWide) * blocksHigh;
constexpr std::size_t headerBytes = 4;
constexpr std::size_t tileBytes = 3;
constexpr std::uint32_t fixtureIdStride = 251;
constexpr std::size_t fixtureBufferSize = 1U << 20U;

class BenchmarkFiles
{
public:
    BenchmarkFiles()
    {
        static std::uint64_t sequence = 0;
        directory_ = std::filesystem::temp_directory_path() /
                     ("uomul-map-benchmark-" + std::to_string(::getpid()) + "-" + std::to_string(++sequence));
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

    ~BenchmarkFiles()
    {
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
    }

    const std::filesystem::path & directory() const { return directory_; }
    const std::filesystem::path & base() const { return base_; }
    const std::filesystem::path & changed() const { return changed_; }
    const std::filesystem::path & dense() const { return dense_; }

private:
    void writeMap(const std::filesystem::path & path, bool changeOneBlock, bool changeAllTiles) const
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            throw std::runtime_error("could not create full-size map fixture");
        }
        std::array<char, fixtureBufferSize> buffer{};
        std::size_t buffered = 0;
        for (std::uint64_t block = 0; block < blockCount; ++block)
        {
            const std::array<char, headerBytes> header{
                static_cast<char>(block & 0xffU),
                static_cast<char>((block >> 8U) & 0xffU),
                static_cast<char>((block >> 16U) & 0xffU),
                static_cast<char>((block >> 24U) & 0xffU),
            };
            append(output, buffer, buffered, header.data(), header.size());
            for (std::uint32_t tile = 0; tile < uomul::landBlockTileCount; ++tile)
            {
                const auto id = static_cast<std::uint16_t>((block * fixtureIdStride + tile) & 0xffffU);
                const bool changedTile = changeAllTiles || (changeOneBlock && block == 0 && tile == 0);
                const auto tileId = static_cast<std::uint16_t>(id + (changedTile ? 1U : 0U));
                const std::array<char, tileBytes> record{
                    static_cast<char>(tileId & 0xffU),
                    static_cast<char>((tileId >> 8U) & 0xffU),
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
        auto result = reader.readLandTile({.x = 3072, .y = 2048});
        requireSuccess(result);
        return result->id;
    };

    BENCHMARK("Trammel/one-block/read")
    {
        auto result = reader.readLandBlock({.x = blocksWide / 2, .y = blocksHigh / 2});
        requireSuccess(result);
        return result->at(0).id;
    };

    BENCHMARK("Trammel/16x16-blocks/read")
    {
        std::uint64_t visited = 0;
        const auto result =
            reader.readLandBlocks({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 16, .y = 16}},
                                  [&visited](uomul::BlockCoord, const uomul::LandBlock &) -> std::expected<void, uomul::Error>
                                  {
                                      ++visited;
                                      return {};
                                  });
        requireSuccess(result);
        return visited;
    };

    BENCHMARK("Trammel/full/read-raw-blocks")
    {
        std::uint64_t visited = 0;
        auto result = reader.readRawLandBlocks(
            fullBlocks,
            [&visited](uomul::BlockCoord, const uomul::LandBlockBytes &) -> std::expected<void, uomul::Error>
            {
                ++visited;
                return {};
            });
        requireSuccess(result);
        return visited;
    };

    BENCHMARK("Trammel/full/read-decoded-blocks")
    {
        std::uint64_t visited = 0;
        auto result =
            reader.readLandBlocks(fullBlocks,
                                  [&visited](uomul::BlockCoord, const uomul::LandBlock &) -> std::expected<void, uomul::Error>
                                  {
                                      ++visited;
                                      return {};
                                  });
        requireSuccess(result);
        return visited;
    };

    BENCHMARK("Trammel/full/read-tiles")
    {
        std::uint64_t visited = 0;
        auto result =
            reader.readLandTiles(fullTiles,
                                 [&visited](uomul::TileCoord, const uomul::LandTile &) -> std::expected<void, uomul::Error>
                                 {
                                     ++visited;
                                     return {};
                                 });
        requireSuccess(result);
        return visited;
    };

    BENCHMARK("Trammel/64x64-tiles/read")
    {
        std::uint64_t visited = 0;
        const auto result =
            reader.readLandTiles({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 64, .y = 64}},
                                 [&visited](uomul::TileCoord, const uomul::LandTile &) -> std::expected<void, uomul::Error>
                                 {
                                     ++visited;
                                     return {};
                                 });
        requireSuccess(result);
        return visited;
    };

    BENCHMARK("Trammel/quarter/read-tiles")
    {
        std::uint64_t visited = 0;
        auto result =
            reader.readLandTiles(quarterTiles,
                                 [&visited](uomul::TileCoord, const uomul::LandTile &) -> std::expected<void, uomul::Error>
                                 {
                                     ++visited;
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
        const auto result = reader.tileStats(fullTiles);
        return result->tileCount;
    };

    BENCHMARK("Trammel/full/stats-blocks")
    {
        const auto result = reader.blockStats(fullBlocks);
        return result->tileCount;
    };

    BENCHMARK("Trammel/64x64-tiles/stats")
    {
        const auto result = reader.tileStats({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 64, .y = 64}});
        return result->tileCount;
    };

    BENCHMARK("Trammel/quarter/stats-tiles")
    {
        auto result = reader.tileStats(quarterTiles);
        return result->tileCount;
    };

    BENCHMARK("Trammel/quarter/stats-blocks")
    {
        auto result = reader.blockStats(quarterBlocks);
        return result->tileCount;
    };
}

TEST_CASE("MapEditor Trammel benchmarks", "[benchmark][map]")
{
    const auto & files = benchmarkFiles();
    const auto referenceResult = uomul::MapReader::open(files.base(), mapDimensions);
    REQUIRE(referenceResult);
    const auto reference = referenceResult->readLandBlock({.x = 0, .y = 0});
    REQUIRE(reference);

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
        requireSuccess(editor->writeLandTile({.x = 0, .y = 0}, {.id = 42, .z = 7}));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/one-block/edit")
    {
        const auto output = files.directory() / "edit-one-block-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->editLandBlocks(
            {.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 1, .y = 1}},
            [](uomul::BlockCoord, const uomul::LandBlock & current) -> std::expected<uomul::LandBlock, uomul::Error>
            {
                auto changed = current;
                ++changed.front().id;
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
        requireSuccess(editor->editLandTiles(
            {.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 1, .y = 1}},
            [](uomul::TileCoord, const uomul::LandTile & current) -> std::expected<uomul::LandTile, uomul::Error>
            { return uomul::LandTile{.id = static_cast<std::uint16_t>(current.id + 1U), .z = current.z}; }));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/one-block/write")
    {
        const auto output = files.directory() / "write-block-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->writeLandBlock({.x = 0, .y = 0}, *reference));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/one-tile/fill")
    {
        const auto output = files.directory() / "fill-tile-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->fillTiles({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 1, .y = 1}}, {.id = 7, .z = 2}));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/one-block/fill")
    {
        const auto output = files.directory() / "fill-block-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->fillBlocks({.min = {.x = 0, .y = 0}, .maxExclusive = {.x = 1, .y = 1}}, {.id = 9, .z = 3}));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/full/fill")
    {
        const auto output = files.directory() / "fill-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->fill({.id = 7, .z = 2}));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/quarter/fill-tiles")
    {
        const auto output = files.directory() / "fill-tile-region-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->fillTiles(quarterTiles, {.id = 7, .z = 2}));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/quarter/fill-blocks")
    {
        const auto output = files.directory() / "fill-block-region-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->fillBlocks(quarterBlocks, {.id = 9, .z = 3}));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/full/replace-ids")
    {
        const auto output = files.directory() / "replace-ids-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->replaceIds(0, 42));
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
        requireSuccess(editor->replaceBlockIds(fullBlocks, 0, 42));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/full/replace-block-elevations")
    {
        const auto output = files.directory() / "replace-block-z-full-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->replaceBlockZ(fullBlocks, 0, 1));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/quarter/replace-tile-ids")
    {
        const auto output = files.directory() / "replace-tile-ids-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->replaceTileIds(quarterTiles, 0, 42));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/quarter/replace-block-ids")
    {
        const auto output = files.directory() / "replace-block-ids-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->replaceBlockIds(quarterBlocks, 0, 42));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/quarter/replace-tile-elevations")
    {
        const auto output = files.directory() / "replace-tile-z-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->replaceTileZ(quarterTiles, 0, 1));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/quarter/replace-block-elevations")
    {
        const auto output = files.directory() / "replace-block-z-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->replaceBlockZ(quarterBlocks, 0, 1));
        editor->abort();
        return std::filesystem::file_size(files.base());
    };

    BENCHMARK("Trammel/quarter/edit-blocks")
    {
        const auto output = files.directory() / "edit-blocks-output.mul";
        auto editor = uomul::MapEditor::create(files.base(), output, mapDimensions, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->editLandBlocks(
            quarterBlocks,
            [](uomul::BlockCoord, const uomul::LandBlock & current) -> std::expected<uomul::LandBlock, uomul::Error>
            {
                auto changed = current;
                ++changed.front().id;
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
        requireSuccess(editor->editLandTiles(
            quarterTiles,
            [](uomul::TileCoord, const uomul::LandTile & current) -> std::expected<uomul::LandTile, uomul::Error>
            { return uomul::LandTile{.id = static_cast<std::uint16_t>(current.id + 1U), .z = current.z}; }));
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
        return result->landBlocksChanged;
    };

    BENCHMARK("Trammel/full/generate-diff-sparse")
    {
        const auto result = uomul::MapDiff::generate(files.base(), files.changed(), diffDirectory, 0, mapDimensions, true);
        requireSuccess(result);
        return result->landTilesChanged;
    };

    BENCHMARK("Trammel/full/generate-diff-dense")
    {
        const auto result = uomul::MapDiff::generate(files.base(), files.dense(), diffDirectory, 0, mapDimensions, true);
        requireSuccess(result);
        return result->landTilesChanged;
    };

    BENCHMARK("Trammel/one-block/generate-diff-from-tiles")
    {
        constexpr uomul::TileRegion region{
            .min = {.x = 0, .y = 0},
            .maxExclusive = {.x = uomul::landBlockDimension, .y = uomul::landBlockDimension},
        };
        const auto result = uomul::MapDiff::generateFromTileRegion(
            files.base(), files.changed(), diffDirectory, 0, mapDimensions, region, true);
        requireSuccess(result);
        return result->landTilesChanged;
    };

    BENCHMARK("Trammel/quarter/generate-diff-from-tiles")
    {
        const auto result = uomul::MapDiff::generateFromTileRegion(
            files.base(), files.changed(), diffDirectory, 0, mapDimensions, quarterTiles, true);
        requireSuccess(result);
        return result->landTilesChanged;
    };

    BENCHMARK("Trammel/quarter/generate-diff-from-blocks")
    {
        const auto result = uomul::MapDiff::generateFromBlockRegion(
            files.base(), files.changed(), diffDirectory, 0, mapDimensions, quarterBlocks, true);
        requireSuccess(result);
        return result->landTilesChanged;
    };

    const auto generated = uomul::MapDiff::generate(files.base(), files.changed(), diffDirectory, 0, mapDimensions, true);
    requireSuccess(generated);

    BENCHMARK("Trammel/full/apply-diff-sparse")
    {
        const auto output = applyDirectory / "applied.mul";
        const auto applied = uomul::MapDiff::apply(files.base(), diffDirectory, output, 0, mapDimensions, true);
        requireSuccess(applied);
        return generated->landTilesChanged;
    };

    BENCHMARK("Trammel/full/write-diff-csv-report")
    {
        const auto report = files.directory() / "changes.csv";
        const auto result = uomul::MapDiff::report(files.base(), files.changed(), report, mapDimensions, true);
        requireSuccess(result);
        return result->landTilesChanged;
    };
}
