#include "../test_helpers.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/texmaps/texmaps.h"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <utility>
#include <vector>

namespace
{

using uomul::test::TestDirectory;

constexpr std::uint32_t byteMask = std::numeric_limits<std::uint8_t>::max();
constexpr unsigned bitsPerByte = std::numeric_limits<unsigned char>::digits;
constexpr unsigned bitsPerWord = std::numeric_limits<std::uint32_t>::digits;
constexpr std::uint16_t smallTextureSample = 0x1234;
constexpr std::uint16_t largeTextureSample = 0xABCD;
constexpr std::uint32_t indexLookupFieldOffset = 8;
constexpr std::uint32_t oversizeEntryCount = 0x4001;
constexpr std::uint8_t invalidLookupByte = 7;
constexpr std::uint32_t mismatchedEntryLength = 100;

void writeWord(std::ofstream & output, std::uint16_t value)
{
    output.put(static_cast<char>(value & byteMask));
    output.put(static_cast<char>(value >> bitsPerByte));
}

void writeIndexEntry(std::ofstream & output, std::uint32_t lookup, std::uint32_t length, std::uint32_t extra)
{
    const std::array<std::uint32_t, 3> fields{lookup, length, extra};
    for (const auto value : fields)
    {
        for (unsigned shift = 0; shift < bitsPerWord; shift += bitsPerByte)
        {
            output.put(static_cast<char>((value >> shift) & byteMask));
        }
    }
}

void writeTexture(const std::filesystem::path & indexPath,
                  const std::filesystem::path & dataPath,
                  std::uint32_t id,
                  std::uint16_t dimension,
                  std::uint16_t value)
{
    const auto byteCount = static_cast<std::uint32_t>(static_cast<std::size_t>(dimension) * dimension * sizeof(std::uint16_t));
    {
        std::ofstream data(dataPath, std::ios::binary | std::ios::trunc);
        for (std::uint32_t index = 0; index < byteCount / sizeof(std::uint16_t); index++)
        {
            writeWord(data, static_cast<std::uint16_t>(value + index));
        }
    }
    std::ofstream index(indexPath, std::ios::binary | std::ios::trunc);
    for (std::uint32_t slot = 0; slot <= id; slot++)
    {
        if (slot == id)
        {
            writeIndexEntry(index, 0, byteCount, dimension == uomul::textureLargeDimension ? 1U : 0U);
        }
        else
        {
            writeIndexEntry(index, 0, 0, 0);
        }
    }
}

} // namespace

TEST_CASE("TexMapsReader_open_Should_DecodeSmallTexture_If_IndexAndPixelsAreLittleEndian")
{
    const TestDirectory directory;
    const auto index = directory.path() / "texidx.mul";
    const auto data = directory.path() / "texmaps.mul";
    writeTexture(index, data, 3, uomul::textureSmallDimension, smallTextureSample);

    auto reader = uomul::TexMapsReader::open(index, data);

    REQUIRE(reader);
    REQUIRE(reader->textureCount() == 4);
    REQUIRE(reader->hasTexture(3) == true);
    const auto texture = reader->readTexture(3);
    REQUIRE(texture);
    REQUIRE(texture->dimension == uomul::textureSmallDimension);
    REQUIRE(texture->pixels.size() == uomul::textureSmallPixelCount);
    REQUIRE(texture->pixels.front() == smallTextureSample);
}

TEST_CASE("TexMapsReader_open_Should_AcceptLargeTexturesAndNonzeroExtraMetadata")
{
    const TestDirectory directory;
    const auto index = directory.path() / "texidx.mul";
    const auto data = directory.path() / "texmaps.mul";
    writeTexture(index, data, 0, uomul::textureLargeDimension, largeTextureSample);
    {
        std::fstream file(index, std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(indexLookupFieldOffset);
        file.put(static_cast<char>(invalidLookupByte));
    }

    auto reader = uomul::TexMapsReader::open(index, data);

    REQUIRE(reader);
    const auto texture = reader->readTexture(0);
    REQUIRE(texture);
    REQUIRE(texture->dimension == uomul::textureLargeDimension);
    REQUIRE(texture->pixels.front() == largeTextureSample);
}

TEST_CASE("TexMapsReader_open_Should_RejectMalformedIndexAndOutOfRangePayload")
{
    const TestDirectory directory;
    const auto index = directory.path() / "texidx.mul";
    const auto data = directory.path() / "texmaps.mul";
    {
        std::ofstream output(index, std::ios::binary);
        output.put('x');
    }
    {
        const std::ofstream output(data, std::ios::binary);
    }
    const auto partial = uomul::TexMapsReader::open(index, data);
    REQUIRE_FALSE(partial);
    REQUIRE(partial.error().code == uomul::ErrorCode::truncatedInput);

    writeTexture(index, data, 0, uomul::textureSmallDimension, 0);
    {
        std::fstream file(index, std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(0);
        file.put(static_cast<char>(byteMask));
        file.put(static_cast<char>(byteMask));
        file.put(static_cast<char>(byteMask));
        file.put(static_cast<char>(byteMask));
    }
    const auto outOfRange = uomul::TexMapsReader::open(index, data);
    REQUIRE_FALSE(outOfRange);
    REQUIRE(outOfRange.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("TexMapsReader_open_Should_RejectMissingIndexFile")
{
    const TestDirectory directory;
    const auto index = directory.path() / "missing.mul";
    const auto data = directory.path() / "data.mul";
    {
        const std::ofstream output(data, std::ios::binary);
    }

    const auto reader = uomul::TexMapsReader::open(index, data);

    REQUIRE_FALSE(reader);
    REQUIRE(reader.error().code == uomul::ErrorCode::io);
}

TEST_CASE("TexMapsReader_open_Should_RejectMissingDataFile")
{
    const TestDirectory directory;
    const auto index = directory.path() / "index.mul";
    const auto data = directory.path() / "missing.mul";
    {
        std::ofstream output(index, std::ios::binary);
        writeIndexEntry(output, 0, 0, 0);
    }

    const auto reader = uomul::TexMapsReader::open(index, data);

    REQUIRE_FALSE(reader);
    REQUIRE(reader.error().code == uomul::ErrorCode::io);
}

TEST_CASE("TexMapsReader_open_Should_RejectDirectoryAsDataFile")
{
    const TestDirectory directory;
    const auto index = directory.path() / "index.mul";
    const auto data = directory.path() / "data_dir";
    {
        std::ofstream output(index, std::ios::binary);
        writeIndexEntry(output, 0, 0, 0);
    }
    std::filesystem::create_directory(data);

    const auto reader = uomul::TexMapsReader::open(index, data);

    REQUIRE_FALSE(reader);
    REQUIRE(reader.error().code == uomul::ErrorCode::io);
}

TEST_CASE("TexMapsReader_open_Should_RejectIndexWithTooManyEntries")
{
    const TestDirectory directory;
    const auto index = directory.path() / "texidx.mul";
    const auto data = directory.path() / "texmaps.mul";

    {
        std::ofstream indexFile(index, std::ios::binary);
        for (std::uint32_t i = 0; i < oversizeEntryCount; i++)
        {
            writeIndexEntry(indexFile, 0, 0, 0);
        }
    }
    {
        const std::ofstream dataFile(data, std::ios::binary);
    }

    const auto reader = uomul::TexMapsReader::open(index, data);

    REQUIRE_FALSE(reader);
    REQUIRE(reader.error().code == uomul::ErrorCode::invalidFormat);
}

TEST_CASE("TexMapsReader_open_Should_RejectEntryWithMismatchedLength")
{
    const TestDirectory directory;
    const auto index = directory.path() / "texidx.mul";
    const auto data = directory.path() / "texmaps.mul";

    {
        std::ofstream indexFile(index, std::ios::binary);
        writeIndexEntry(indexFile, 0, mismatchedEntryLength, 0); // Wrong length for small texture
    }
    {
        const std::ofstream dataFile(data, std::ios::binary);
    }

    const auto reader = uomul::TexMapsReader::open(index, data);

    REQUIRE_FALSE(reader);
    REQUIRE(reader.error().code == uomul::ErrorCode::invalidFormat);
}

TEST_CASE("TexMapsReader_open_Should_AcceptEmptyIndex")
{
    const TestDirectory directory;
    const auto index = directory.path() / "texidx.mul";
    const auto data = directory.path() / "texmaps.mul";
    {
        const std::ofstream indexFile(index, std::ios::binary);
    }
    {
        const std::ofstream dataFile(data, std::ios::binary);
    }

    const auto reader = uomul::TexMapsReader::open(index, data);

    REQUIRE(reader);
    REQUIRE(reader->textureCount() == 0);
}

TEST_CASE("TexMapsReader_hasTexture_Should_RejectOutOfRangeAndEmptyTexture")
{
    const TestDirectory directory;
    const auto index = directory.path() / "texidx.mul";
    const auto data = directory.path() / "texmaps.mul";
    {
        std::ofstream output(index, std::ios::binary);
        writeIndexEntry(output, 0, 0, 0);
    }
    {
        const std::ofstream output(data, std::ios::binary);
    }
    auto reader = uomul::TexMapsReader::open(index, data);
    REQUIRE(reader);
    REQUIRE(reader->hasTexture(0) == false);
    REQUIRE_FALSE(reader->hasTexture(1));
    REQUIRE_FALSE(reader->readTexture(0));
}

TEST_CASE("TexMapsReader_readTexture_Should_FailWhenDataIsTruncatedAfterOpening")
{
    const TestDirectory directory;
    const auto index = directory.path() / "texidx.mul";
    const auto data = directory.path() / "texmaps.mul";
    writeTexture(index, data, 0, uomul::textureSmallDimension, smallTextureSample);
    auto reader = uomul::TexMapsReader::open(index, data);
    REQUIRE(reader);

    std::filesystem::resize_file(data, 0);

    const auto texture = reader->readTexture(0);
    REQUIRE_FALSE(texture);
    REQUIRE(texture.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("TexMapsReader_moveConstructor_Should_KeepReaderUsable_If_ReaderIsMoved")
{
    const TestDirectory directory;
    const auto path = directory.path() / "texidx.mul";
    const auto data = directory.path() / "texmaps.mul";
    writeTexture(path, data, 3, uomul::textureSmallDimension, smallTextureSample);
    auto opened = uomul::TexMapsReader::open(path, data);
    REQUIRE(opened);

    const uomul::TexMapsReader reader(std::move(*opened));

    REQUIRE(reader.textureCount() == 4);
    REQUIRE(reader.readTexture(3)->dimension == uomul::textureSmallDimension);
}

TEST_CASE("TexMapsReader_moveConstructor_Should_LeaveMovedFromReaderInert")
{
    const TestDirectory directory;
    const auto path = directory.path() / "texidx.mul";
    const auto data = directory.path() / "texmaps.mul";
    writeTexture(path, data, 0, uomul::textureSmallDimension, smallTextureSample);
    auto opened = uomul::TexMapsReader::open(path, data);
    REQUIRE(opened);

    const uomul::TexMapsReader moved(std::move(*opened));
    (void) moved; // Use moved to avoid unused variable warning

    REQUIRE(opened->textureCount() == 0);
    REQUIRE_FALSE(opened->hasTexture(0));
    REQUIRE_FALSE(opened->readTexture(0));
}

TEST_CASE("TexMapsReader_moveAssignment_Should_ReplaceReaderState")
{
    const TestDirectory directory;
    const auto first = directory.path() / "first_idx.mul";
    const auto firstData = directory.path() / "first_data.mul";
    const auto second = directory.path() / "second_idx.mul";
    const auto secondData = directory.path() / "second_data.mul";
    writeTexture(first, firstData, 0, uomul::textureSmallDimension, smallTextureSample);
    writeTexture(second, secondData, 0, uomul::textureLargeDimension, largeTextureSample);

    auto firstReader = uomul::TexMapsReader::open(first, firstData);
    auto secondReader = uomul::TexMapsReader::open(second, secondData);
    REQUIRE(firstReader);
    REQUIRE(secondReader);

    *firstReader = std::move(*secondReader);

    REQUIRE(firstReader->textureCount() == 1);
    const auto texture = firstReader->readTexture(0);
    REQUIRE(texture->dimension == uomul::textureLargeDimension);
}
