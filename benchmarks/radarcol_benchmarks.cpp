#include "uomul/radarcol/radarcol.h"
#include <array>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unistd.h>

namespace
{

constexpr std::size_t fixtureByteCount = static_cast<std::size_t>(uomul::radarColorCount) * sizeof(uomul::RadarColor);
constexpr std::size_t radarColorByteCount = sizeof(uomul::RadarColor);
constexpr std::uint32_t byteMask = 0xFFU;
constexpr std::uint32_t byteShift = 8U;
constexpr std::uint32_t rgb24ChannelPattern = 0x010101U;
constexpr std::uint32_t packedColorSuffix = 0xA5U;
constexpr std::uint32_t sampleMapColorIndex = 0x1234;
constexpr uomul::RadarColor sampleEditedColor = 0x2345;
constexpr std::uint32_t colorsPerBatch = 256;

class BenchmarkFiles
{
public:
    BenchmarkFiles()
    {
        static std::uint64_t sequence = 0;
        directory_ = std::filesystem::temp_directory_path() /
                     ("uomul-radarcol-benchmark-" + std::to_string(::getpid()) + "-" + std::to_string(sequence++));
        std::filesystem::create_directories(directory_);
        base_ = directory_ / "base.mul";
        writeTable();
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

private:
    void writeTable() const
    {
        std::array<char, fixtureByteCount> bytes{};
        for (std::uint32_t index = 0; index < uomul::radarColorCount; index++)
        {
            const auto value = static_cast<std::uint32_t>((index * 17U) & uomul::RadarColorConverter::colorMask);
            const auto offset = static_cast<std::size_t>(index) * radarColorByteCount;
            bytes.at(offset) = static_cast<char>(value & byteMask);
            bytes.at(offset + 1) = static_cast<char>((value >> byteShift) & byteMask);
        }
        std::ofstream output(base_, std::ios::binary | std::ios::trunc);
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        output.flush();
        if (!output)
        {
            throw std::runtime_error("could not write full-size radar color fixture");
        }
    }

    std::filesystem::path directory_;
    std::filesystem::path base_;
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

std::uint64_t rgb888Checksum()
{
    std::uint64_t checksum = 0;
    for (std::uint32_t index = 0; index < uomul::radarColorCount; index++)
    {
        const auto red = static_cast<std::uint8_t>(index);
        const auto green = static_cast<std::uint8_t>(index >> 3U);
        const auto blue = static_cast<std::uint8_t>(index >> 7U);
        checksum += uomul::RadarColorConverter::fromRgb888(red, green, blue);
    }
    return checksum;
}

std::uint64_t rgb24Checksum()
{
    std::uint64_t checksum = 0;
    for (std::uint32_t index = 0; index < uomul::radarColorCount; index++)
    {
        checksum += uomul::RadarColorConverter::fromRgb24(index * rgb24ChannelPattern);
    }
    return checksum;
}

std::uint64_t argb8888Checksum()
{
    std::uint64_t checksum = 0;
    for (std::uint32_t index = 0; index < uomul::radarColorCount; index++)
    {
        checksum += uomul::RadarColorConverter::fromArgb8888((index << byteShift) | packedColorSuffix);
    }
    return checksum;
}

std::uint64_t rgba8888Checksum()
{
    std::uint64_t checksum = 0;
    for (std::uint32_t index = 0; index < uomul::radarColorCount; index++)
    {
        checksum += uomul::RadarColorConverter::fromRgba8888((index << byteShift) | packedColorSuffix);
    }
    return checksum;
}

std::uint64_t rgb565Checksum()
{
    std::uint64_t checksum = 0;
    for (std::uint32_t index = 0; index < uomul::radarColorCount; index++)
    {
        checksum += uomul::RadarColorConverter::fromRgb565(static_cast<std::uint16_t>(index * 2U));
    }
    return checksum;
}

std::uint64_t argb1555Checksum()
{
    std::uint64_t checksum = 0;
    for (std::uint32_t index = 0; index < uomul::radarColorCount; index++)
    {
        checksum += uomul::RadarColorConverter::fromArgb1555(static_cast<std::uint16_t>(index * 3U));
    }
    return checksum;
}

std::uint64_t rgb555Checksum()
{
    std::uint64_t checksum = 0;
    for (std::uint32_t index = 0; index < uomul::radarColorCount; index++)
    {
        checksum += uomul::RadarColorConverter::fromRgb555(static_cast<std::uint16_t>(index * 3U));
    }
    return checksum;
}

} // namespace

TEST_CASE("RadarColReader benchmarks", "[benchmark][radarcol]")
{
    const auto & files = benchmarkFiles();
    const auto readerResult = uomul::RadarColReader::open(files.base());
    REQUIRE(readerResult);
    const auto & reader = *readerResult;

    BENCHMARK("RadarCol/full/open-reader")
    {
        auto result = uomul::RadarColReader::open(files.base());
        requireSuccess(result);
        return result->colorCount();
    };

    BENCHMARK("RadarCol/full/read-metadata")
    {
        return reader.colorCount();
    };

    BENCHMARK("RadarCol/one-map-color/read")
    {
        const auto result = reader.readMapColor(sampleMapColorIndex);
        requireSuccess(result);
        return *result;
    };

    BENCHMARK("RadarCol/one-static-color/read")
    {
        const auto result = reader.readStaticColor(sampleMapColorIndex);
        requireSuccess(result);
        return *result;
    };

    BENCHMARK("RadarCol/one-raw-color/read")
    {
        const auto result = reader.readRawColor(uomul::radarStaticColorOffset + sampleMapColorIndex);
        requireSuccess(result);
        return *result;
    };

    BENCHMARK("RadarCol/full/read-map-colors")
    {
        std::uint64_t checksum = 0;
        for (std::uint32_t index = 0; index < uomul::radarMapColorCount; index++)
        {
            const auto result = reader.readMapColor(static_cast<std::uint16_t>(index));
            requireSuccess(result);
            checksum += *result;
        }
        return checksum;
    };

    BENCHMARK("RadarCol/full/read-static-colors")
    {
        std::uint64_t checksum = 0;
        for (std::uint32_t index = 0; index < uomul::radarMapColorCount; index++)
        {
            const auto result = reader.readStaticColor(static_cast<std::uint16_t>(index));
            requireSuccess(result);
            checksum += *result;
        }
        return checksum;
    };

    BENCHMARK("RadarCol/full/read-raw-colors")
    {
        std::uint64_t checksum = 0;
        for (std::uint32_t index = 0; index < uomul::radarColorCount; index++)
        {
            const auto result = reader.readRawColor(index);
            requireSuccess(result);
            checksum += *result;
        }
        return checksum;
    };
}

TEST_CASE("RadarColEditor benchmarks", "[benchmark][radarcol]")
{
    const auto & files = benchmarkFiles();

    BENCHMARK("RadarCol/full/create-and-stage-copy")
    {
        const auto output = files.directory() / "create-output.mul";
        auto editor = uomul::RadarColEditor::create(files.base(), output, {.overwriteOutput = true});
        requireSuccess(editor);
        editor->abort();
        return fixtureByteCount;
    };

    BENCHMARK("RadarCol/full/create-empty")
    {
        const auto output = files.directory() / "empty-output.mul";
        auto editor = uomul::RadarColEditor::createEmpty(output, {.overwriteOutput = true});
        requireSuccess(editor);
        editor->abort();
        return fixtureByteCount;
    };

    BENCHMARK("RadarCol/one-map-color/edit")
    {
        const auto output = files.directory() / "one-map-color-output.mul";
        auto editor = uomul::RadarColEditor::create(files.base(), output, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->editMapColor(sampleMapColorIndex, sampleEditedColor));
        editor->abort();
        return fixtureByteCount;
    };

    BENCHMARK("RadarCol/one-static-color/edit")
    {
        const auto output = files.directory() / "one-static-color-output.mul";
        auto editor = uomul::RadarColEditor::create(files.base(), output, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->editStaticColor(sampleMapColorIndex, sampleEditedColor));
        editor->abort();
        return fixtureByteCount;
    };

    BENCHMARK("RadarCol/256-colors/edit")
    {
        const auto output = files.directory() / "batch-edit-output.mul";
        auto editor = uomul::RadarColEditor::create(files.base(), output, {.overwriteOutput = true});
        requireSuccess(editor);
        for (std::uint32_t index = 0; index < colorsPerBatch; index++)
        {
            requireSuccess(editor->editRawColor(index, static_cast<uomul::RadarColor>(index)));
        }
        editor->abort();
        return colorsPerBatch;
    };

    BENCHMARK("RadarCol/full/commit-staged-copy")
    {
        const auto output = files.directory() / "commit-output.mul";
        auto editor = uomul::RadarColEditor::create(files.base(), output, {.overwriteOutput = true});
        requireSuccess(editor);
        requireSuccess(editor->commit());
        return std::filesystem::file_size(output);
    };
}

TEST_CASE("RadarColorConverter benchmarks", "[benchmark][radarcol]")
{
    BENCHMARK("RadarCol/full/convert-rgb888")
    {
        return rgb888Checksum();
    };

    BENCHMARK("RadarCol/full/convert-rgb24")
    {
        return rgb24Checksum();
    };

    BENCHMARK("RadarCol/full/convert-argb8888")
    {
        return argb8888Checksum();
    };

    BENCHMARK("RadarCol/full/convert-rgba8888")
    {
        return rgba8888Checksum();
    };

    BENCHMARK("RadarCol/full/convert-rgb565")
    {
        return rgb565Checksum();
    };

    BENCHMARK("RadarCol/full/convert-argb1555")
    {
        return argb1555Checksum();
    };

    BENCHMARK("RadarCol/full/convert-rgb555")
    {
        return rgb555Checksum();
    };
}
