#include "uomul/texmaps/texmaps.h"
#include <array>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace
{

constexpr std::uint32_t byteMask = 0xFFU;
constexpr unsigned byteShift = std::numeric_limits<unsigned char>::digits;
constexpr std::uint16_t smallTextureSample = 0x1234;
constexpr std::uint16_t largeTextureSample = 0xABCD;
constexpr std::uint16_t editedTextureSample = 0x9876;
constexpr std::uint32_t sampleTextureId = 5;
constexpr std::uint32_t texturesPerBatch = 16;
constexpr std::uint32_t smallTextureFixtureCount = 32;
constexpr std::uint32_t largeTextureFixtureCount = 16;
constexpr std::uint32_t mixedTextureFixtureCount = 8;

void writeWord(std::ofstream & output, std::uint16_t value)
{
    output.put(static_cast<char>(value & byteMask));
    output.put(static_cast<char>(value >> byteShift));
}

void writeIndexEntry(std::ofstream & output, std::uint32_t lookup, std::uint32_t length, std::uint32_t extra)
{
    const std::array<std::uint32_t, 3> fields{lookup, length, extra};
    for (const auto value : fields)
    {
        for (unsigned shift = 0; shift < std::numeric_limits<std::uint32_t>::digits; shift += byteShift)
        {
            output.put(static_cast<char>((value >> shift) & byteMask));
        }
    }
}

class BenchmarkFiles
{
public:
    BenchmarkFiles()
    {
        static std::uint64_t sequence = 0;
        directory_ = std::filesystem::temp_directory_path() /
                     ("uomul-texmaps-benchmark-" + std::to_string(::getpid()) + "-" + std::to_string(sequence++));
        std::filesystem::create_directories(directory_);
        smallIndex_ = directory_ / "small.idx";
        smallData_ = directory_ / "small.mul";
        largeIndex_ = directory_ / "large.idx";
        largeData_ = directory_ / "large.mul";
        mixedIndex_ = directory_ / "mixed.idx";
        mixedData_ = directory_ / "mixed.mul";
        writeSmallTextures();
        writeLargeTextures();
        writeMixedTextures();
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
    [[nodiscard]] const std::filesystem::path & smallIndex() const { return smallIndex_; }
    [[nodiscard]] const std::filesystem::path & smallData() const { return smallData_; }
    [[nodiscard]] const std::filesystem::path & largeIndex() const { return largeIndex_; }
    [[nodiscard]] const std::filesystem::path & largeData() const { return largeData_; }
    [[nodiscard]] const std::filesystem::path & mixedIndex() const { return mixedIndex_; }
    [[nodiscard]] const std::filesystem::path & mixedData() const { return mixedData_; }

private:
    void writeSmallTextures() const
    {
        std::ofstream data(smallData_, std::ios::binary | std::ios::trunc);
        for (std::uint32_t pixel = 0; pixel < uomul::textureSmallPixelCount; pixel++)
        {
            writeWord(data, static_cast<std::uint16_t>(smallTextureSample + pixel));
        }
        data.flush();

        std::ofstream index(smallIndex_, std::ios::binary | std::ios::trunc);
        for (std::uint32_t id = 0; id < smallTextureFixtureCount; id++)
        {
            writeIndexEntry(index, 0, static_cast<std::uint32_t>(uomul::textureSmallByteCount), 0);
        }
        index.flush();
    }

    void writeLargeTextures() const
    {
        std::ofstream data(largeData_, std::ios::binary | std::ios::trunc);
        for (std::uint32_t pixel = 0; pixel < uomul::textureLargePixelCount; pixel++)
        {
            writeWord(data, static_cast<std::uint16_t>(largeTextureSample + pixel));
        }
        data.flush();

        std::ofstream index(largeIndex_, std::ios::binary | std::ios::trunc);
        for (std::uint32_t id = 0; id < largeTextureFixtureCount; id++)
        {
            writeIndexEntry(index, 0, static_cast<std::uint32_t>(uomul::textureLargeByteCount), 1);
        }
        index.flush();
    }

    void writeMixedTextures() const
    {
        std::ofstream data(mixedData_, std::ios::binary | std::ios::trunc);
        std::uint32_t offset = 0;

        // Write 8 small textures
        for (std::uint32_t i = 0; i < mixedTextureFixtureCount; i++)
        {
            for (std::uint32_t pixel = 0; pixel < uomul::textureSmallPixelCount; pixel++)
            {
                writeWord(data, static_cast<std::uint16_t>(smallTextureSample + pixel));
            }
            offset += uomul::textureSmallByteCount;
        }

        // Write 8 large textures
        for (std::uint32_t i = 0; i < mixedTextureFixtureCount; i++)
        {
            for (std::uint32_t pixel = 0; pixel < uomul::textureLargePixelCount; pixel++)
            {
                writeWord(data, static_cast<std::uint16_t>(largeTextureSample + pixel));
            }
            offset += uomul::textureLargeByteCount;
        }
        data.flush();

        std::ofstream index(mixedIndex_, std::ios::binary | std::ios::trunc);
        // 8 small
        for (std::uint32_t i = 0; i < mixedTextureFixtureCount; i++)
        {
            writeIndexEntry(index,
                            (i * static_cast<std::uint32_t>(uomul::textureSmallByteCount)),
                            static_cast<std::uint32_t>(uomul::textureSmallByteCount),
                            0);
        }
        // 8 large
        offset = mixedTextureFixtureCount * uomul::textureSmallByteCount;
        for (std::uint32_t i = 0; i < mixedTextureFixtureCount; i++)
        {
            writeIndexEntry(index,
                            offset + (i * static_cast<std::uint32_t>(uomul::textureLargeByteCount)),
                            static_cast<std::uint32_t>(uomul::textureLargeByteCount),
                            1);
        }
        index.flush();
    }

    std::filesystem::path directory_;
    std::filesystem::path smallIndex_;
    std::filesystem::path smallData_;
    std::filesystem::path largeIndex_;
    std::filesystem::path largeData_;
    std::filesystem::path mixedIndex_;
    std::filesystem::path mixedData_;
};

void requireSuccess(const auto & result)
{
    if (!result)
    {
        throw std::runtime_error(result.error().message);
    }
}

const BenchmarkFiles & benchmarkFiles()
{
    static const BenchmarkFiles files;
    return files;
}

} // namespace

TEST_CASE("TexMapsReader benchmarks", "[benchmark][texmaps]")
{
    const auto & files = benchmarkFiles();
    const auto readerResult = uomul::TexMapsReader::open(files.smallIndex(), files.smallData());
    requireSuccess(readerResult);
    const auto & reader = *readerResult;

    BENCHMARK("TexMaps/small/open-reader")
    {
        auto result = uomul::TexMapsReader::open(files.smallIndex(), files.smallData());
        requireSuccess(result);
        return result->textureCount();
    };

    BENCHMARK("TexMaps/small/read-metadata")
    {
        return reader.textureCount();
    };

    BENCHMARK("TexMaps/one-texture/read-small")
    {
        const auto result = reader.readTexture(sampleTextureId);
        requireSuccess(result);
        return result->dimension;
    };

    BENCHMARK("TexMaps/all-textures/read-small")
    {
        std::uint64_t checksum = 0;
        for (std::uint32_t id = 0; id < reader.textureCount(); id++)
        {
            const auto result = reader.readTexture(id);
            requireSuccess(result);
            checksum += result->pixels.size();
        }
        return checksum;
    };
}

TEST_CASE("TexMapsReader large texture benchmarks", "[benchmark][texmaps]")
{
    const auto & files = benchmarkFiles();
    const auto readerResult = uomul::TexMapsReader::open(files.largeIndex(), files.largeData());
    requireSuccess(readerResult);
    const auto & reader = *readerResult;

    BENCHMARK("TexMaps/large/open-reader")
    {
        auto result = uomul::TexMapsReader::open(files.largeIndex(), files.largeData());
        requireSuccess(result);
        return result->textureCount();
    };

    BENCHMARK("TexMaps/one-texture/read-large")
    {
        const auto result = reader.readTexture(5);
        requireSuccess(result);
        return result->dimension;
    };

    BENCHMARK("TexMaps/all-textures/read-large")
    {
        std::uint64_t checksum = 0;
        for (std::uint32_t id = 0; id < reader.textureCount(); id++)
        {
            const auto result = reader.readTexture(id);
            requireSuccess(result);
            checksum += result->pixels.size();
        }
        return checksum;
    };
}

TEST_CASE("TexMapsReader mixed texture benchmarks", "[benchmark][texmaps]")
{
    const auto & files = benchmarkFiles();
    const auto readerResult = uomul::TexMapsReader::open(files.mixedIndex(), files.mixedData());
    requireSuccess(readerResult);
    const auto & reader = *readerResult;

    BENCHMARK("TexMaps/mixed/open-reader")
    {
        auto result = uomul::TexMapsReader::open(files.mixedIndex(), files.mixedData());
        requireSuccess(result);
        return result->textureCount();
    };

    BENCHMARK("TexMaps/all-textures/read-mixed")
    {
        std::uint64_t checksum = 0;
        for (std::uint32_t id = 0; id < reader.textureCount(); id++)
        {
            const auto result = reader.readTexture(id);
            requireSuccess(result);
            checksum += result->pixels.size();
        }
        return checksum;
    };
}

TEST_CASE("TexMapsEditor benchmarks", "[benchmark][texmaps]")
{
    const auto & files = benchmarkFiles();

    BENCHMARK("TexMaps/small/create-and-stage-copy")
    {
        const auto outputIndex = files.directory() / "create-small-index.idx";
        const auto outputData = files.directory() / "create-small-data.mul";
        auto editor = uomul::TexMapsEditor::create(
            files.smallIndex(), files.smallData(), outputIndex, outputData, {.overwriteOutput = true});
        requireSuccess(editor);
        editor->abort();
        return 1;
    };

    BENCHMARK("TexMaps/large/create-and-stage-copy")
    {
        const auto outputIndex = files.directory() / "create-large-index.idx";
        const auto outputData = files.directory() / "create-large-data.mul";
        auto editor = uomul::TexMapsEditor::create(
            files.largeIndex(), files.largeData(), outputIndex, outputData, {.overwriteOutput = true});
        requireSuccess(editor);
        editor->abort();
        return 1;
    };

    BENCHMARK("TexMaps/createEmpty")
    {
        const auto outputIndex = files.directory() / "empty-index.idx";
        const auto outputData = files.directory() / "empty-data.mul";
        auto editor = uomul::TexMapsEditor::createEmpty(outputIndex, outputData, {.overwriteOutput = true});
        requireSuccess(editor);
        editor->abort();
        return uomul::textureCount;
    };

    BENCHMARK("TexMaps/one-texture/edit-small")
    {
        const auto outputIndex = files.directory() / "edit-small-index.idx";
        const auto outputData = files.directory() / "edit-small-data.mul";
        auto editor = uomul::TexMapsEditor::create(
            files.smallIndex(), files.smallData(), outputIndex, outputData, {.overwriteOutput = true});
        requireSuccess(editor);

        uomul::TextureImage image;
        image.dimension = uomul::textureSmallDimension;
        image.pixels.resize(uomul::textureSmallPixelCount, editedTextureSample);
        requireSuccess(editor->editTexture(sampleTextureId, image));
        editor->abort();
        return 1;
    };

    BENCHMARK("TexMaps/batch/edit-textures")
    {
        const auto outputIndex = files.directory() / "batch-edit-index.idx";
        const auto outputData = files.directory() / "batch-edit-data.mul";
        auto editor = uomul::TexMapsEditor::createEmpty(outputIndex, outputData, {.overwriteOutput = true});
        requireSuccess(editor);

        uomul::TextureImage image;
        image.dimension = uomul::textureSmallDimension;
        image.pixels.resize(uomul::textureSmallPixelCount, editedTextureSample);
        for (std::uint32_t id = 0; id < texturesPerBatch; id++)
        {
            requireSuccess(editor->editTexture(id, image));
        }
        editor->abort();
        return texturesPerBatch;
    };

    BENCHMARK("TexMaps/full/commit-staged-copy")
    {
        const auto outputIndex = files.directory() / "commit-index.idx";
        const auto outputData = files.directory() / "commit-data.mul";
        auto editor = uomul::TexMapsEditor::create(
            files.smallIndex(), files.smallData(), outputIndex, outputData, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->commit());
        return std::filesystem::file_size(outputData);
    };
}

TEST_CASE("TexMapsEditor deduplication benchmarks", "[benchmark][texmaps]")
{
    const auto & files = benchmarkFiles();

    BENCHMARK("TexMaps/dedup/create-and-deduplicate")
    {
        const auto outputIndex = files.directory() / "dedup-index.idx";
        const auto outputData = files.directory() / "dedup-data.mul";
        auto editor = uomul::TexMapsEditor::createEmpty(outputIndex, outputData, {.overwriteOutput = true});
        requireSuccess(editor);

        uomul::TextureImage image;
        image.dimension = uomul::textureSmallDimension;
        image.pixels.resize(uomul::textureSmallPixelCount, editedTextureSample);

        // Add 8 identical textures
        for (std::uint32_t id = 0; id < mixedTextureFixtureCount; id++)
        {
            requireSuccess(editor->editTexture(id, image));
        }

        const auto result = editor->deduplicateTextures();
        requireSuccess(result);
        editor->abort();
        return result->duplicateEntries;
    };
}
