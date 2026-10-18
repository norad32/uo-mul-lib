#include "../map_test_helpers.h"
#include "uomul/texmaps/texmaps.h"
#include "uomul/mul_common/mul_common.h"
#include <array>
#include <cstddef>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <vector>

namespace
{

using uomul::test::TestDirectory;

constexpr std::uint32_t byteMask = std::numeric_limits<std::uint8_t>::max();
constexpr unsigned bitsPerByte = std::numeric_limits<unsigned char>::digits;
constexpr unsigned bitsPerWord = std::numeric_limits<std::uint32_t>::digits;
constexpr std::uint16_t smallTextureSample = 0x1234;
constexpr std::uint16_t largeTextureSample = 0xABCD;
constexpr std::uint16_t editedTextureSample = 0x9876;
constexpr std::uint16_t sourceTextureSample = 0x3456;
constexpr std::uint32_t indexLookupFieldOffset = 8;
constexpr char invalidLookupByte = 7;

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

TEST_CASE("TexMapsReader_open_Should_DecodeTexture_If_IndexAndPixelsAreLittleEndian")
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
        file.put(invalidLookupByte);
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

TEST_CASE("TexMapsEditor_createEmpty_Should_EditDeduplicateAndCommitTexturePair")
{
    const TestDirectory directory;
    const auto index = directory.path() / "out.idx";
    const auto data = directory.path() / "out.mul";
    auto editor = uomul::TexMapsEditor::createEmpty(index, data);
    REQUIRE(editor);
    uomul::TextureImage image;
    image.dimension = uomul::textureSmallDimension;
    image.pixels.resize(uomul::textureSmallPixelCount, editedTextureSample);
    REQUIRE(editor->editTexture(2, image));
    REQUIRE(editor->editTexture(7, image));
    const auto report = editor->deduplicateTextures();
    REQUIRE(report);
    REQUIRE(report->entriesScanned == uomul::textureCount);
    REQUIRE(report->uniquePayloads == 1);
    REQUIRE(report->duplicateEntries == 1);
    REQUIRE(report->outputDataBytes == uomul::textureSmallByteCount);
    REQUIRE(editor->commit());

    auto reader = uomul::TexMapsReader::open(index, data);
    REQUIRE(reader);
    REQUIRE(reader->textureCount() == uomul::textureCount);
    REQUIRE(reader->readTexture(2) == image);
    REQUIRE(reader->readTexture(7) == image);
}

TEST_CASE("TexMapsEditor_commit_Should_RollBackPublishedIndex_If_DataOutputAppearsDuringEdit")
{
    const TestDirectory directory;
    const auto sourceIndex = directory.path() / "source.idx";
    const auto sourceData = directory.path() / "source.mul";
    const auto outputIndex = directory.path() / "output.idx";
    const auto outputData = directory.path() / "output.mul";
    writeTexture(sourceIndex, sourceData, 0, uomul::textureSmallDimension, sourceTextureSample);

    auto editor = uomul::TexMapsEditor::create(sourceIndex, sourceData, outputIndex, outputData);
    REQUIRE(editor);
    {
        std::ofstream externalData(outputData, std::ios::binary);
        externalData.put('x');
    }

    const auto result = editor->commit();

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::commitFailed);
    REQUIRE_FALSE(std::filesystem::exists(outputIndex));
    REQUIRE(uomul::test::readBytes(outputData) == "x");
}

TEST_CASE("TexMapsEditor_commit_Should_ReplaceBothOutputs_If_OverwriteIsEnabled")
{
    const TestDirectory directory;
    const auto index = directory.path() / "output.idx";
    const auto data = directory.path() / "output.mul";
    writeTexture(index, data, 0, uomul::textureSmallDimension, sourceTextureSample);

    auto editor = uomul::TexMapsEditor::createEmpty(index, data, {.overwriteOutput = true});
    REQUIRE(editor);
    REQUIRE(editor->commit());

    auto reader = uomul::TexMapsReader::open(index, data);
    REQUIRE(reader);
    REQUIRE(reader->textureCount() == uomul::textureCount);
    REQUIRE_FALSE(reader->hasTexture(0).value());
    REQUIRE(std::filesystem::file_size(data) == 0);
}

TEST_CASE("TexMapsEditor_create_Should_PreserveSourcePairOnAbortAndCommitCopyThrough")
{
    const TestDirectory directory;
    const auto sourceIndex = directory.path() / "source.idx";
    const auto sourceData = directory.path() / "source.mul";
    const auto outputIndex = directory.path() / "output.idx";
    const auto outputData = directory.path() / "output.mul";
    writeTexture(sourceIndex, sourceData, 0, uomul::textureSmallDimension, sourceTextureSample);
    const auto sourceIndexBytes = uomul::test::readBytes(sourceIndex);
    const auto sourceDataBytes = uomul::test::readBytes(sourceData);
    {
        auto editor = uomul::TexMapsEditor::create(sourceIndex, sourceData, outputIndex, outputData);
        REQUIRE(editor);
        editor->abort();
    }
    REQUIRE_FALSE(std::filesystem::exists(outputIndex));
    REQUIRE_FALSE(std::filesystem::exists(outputData));
    REQUIRE(uomul::test::readBytes(sourceIndex) == sourceIndexBytes);
    REQUIRE(uomul::test::readBytes(sourceData) == sourceDataBytes);

    auto editor = uomul::TexMapsEditor::create(sourceIndex, sourceData, outputIndex, outputData);
    REQUIRE(editor);
    REQUIRE(editor->commit());
    REQUIRE(uomul::test::readBytes(outputIndex) == sourceIndexBytes);
    REQUIRE(uomul::test::readBytes(outputData) == sourceDataBytes);
}
