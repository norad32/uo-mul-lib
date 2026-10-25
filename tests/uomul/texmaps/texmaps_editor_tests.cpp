#include "../test_helpers.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/texmaps/texmaps.h"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <csignal> // NOLINT(misc-include-cleaner): sighandler_t and SIGXFSZ defined here
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <string>
#include <sys/resource.h>
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
constexpr std::uint16_t editedTextureSample = 0x9876;
constexpr std::uint16_t sourceTextureSample = 0x3456;
constexpr std::uint32_t fileSizeLimitBytes = 100; // Very small to force write failure
constexpr std::size_t invalidDimensionValue = 100;

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

[[nodiscard]] std::vector<std::filesystem::path> stagingFilesFor(const std::filesystem::path & directory,
                                                                 const std::string & outputName)
{
    std::vector<std::filesystem::path> stagingFiles;
    for (const auto & entry : std::filesystem::directory_iterator(directory))
    {
        if (entry.path().filename().string().starts_with(outputName + ".uomul-tmp-"))
        {
            stagingFiles.push_back(entry.path());
        }
    }
    return stagingFiles;
}

class FileSizeLimitGuard
{
public:
    // NOLINTBEGIN(misc-include-cleaner)
    explicit FileSizeLimitGuard(std::uint64_t limitBytes) : original_{}, originalHandler_(::signal(SIGXFSZ, SIG_IGN))
    // NOLINTEND(misc-include-cleaner)
    {
        CHECK(::getrlimit(RLIMIT_FSIZE, &original_) == 0);
        struct rlimit limited = original_;
        limited.rlim_cur = limitBytes;
        CHECK(::setrlimit(RLIMIT_FSIZE, &limited) == 0);
    }

    ~FileSizeLimitGuard()
    {
        struct rlimit current{};
        CHECK(::getrlimit(RLIMIT_FSIZE, &current) == 0);
        current.rlim_cur = original_.rlim_cur;
        CHECK(::setrlimit(RLIMIT_FSIZE, &current) == 0);
        // NOLINTNEXTLINE(misc-include-cleaner)
        CHECK(::signal(SIGXFSZ, originalHandler_) != SIG_ERR);
    }

    FileSizeLimitGuard(const FileSizeLimitGuard &) = delete;
    FileSizeLimitGuard & operator=(const FileSizeLimitGuard &) = delete;
    FileSizeLimitGuard(FileSizeLimitGuard &&) = delete;
    FileSizeLimitGuard & operator=(FileSizeLimitGuard &&) = delete;

private:
    struct rlimit original_{};
    sighandler_t originalHandler_ = SIG_DFL; // NOLINT(misc-include-cleaner)
};

} // namespace

TEST_CASE("TexMapsEditor_createEmpty_Should_CreateCompleteMapWithDefaultTile_If_DimensionsAreValid")
{
    const TestDirectory directory;
    const auto index = directory.path() / "empty.idx";
    const auto data = directory.path() / "empty.mul";

    auto editor = uomul::TexMapsEditor::createEmpty(index, data);
    REQUIRE(editor);
    REQUIRE(editor->commit());

    const auto reader = uomul::TexMapsReader::open(index, data);
    REQUIRE(reader);
    REQUIRE(reader->textureCount() == uomul::textureCount);
    REQUIRE(reader->hasTexture(0) == false);
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

TEST_CASE("TexMapsEditor_create_Should_EditInPlace_IfOutputsNameTheSourcePair")
{
    const TestDirectory directory;
    const auto index = directory.path() / "source.idx";
    const auto data = directory.path() / "source.mul";
    writeTexture(index, data, 0, uomul::textureSmallDimension, sourceTextureSample);

    auto editor = uomul::TexMapsEditor::create(index, data, index, data);
    REQUIRE(editor);

    uomul::TextureImage image;
    image.dimension = uomul::textureSmallDimension;
    image.pixels.resize(uomul::textureSmallPixelCount, editedTextureSample);
    REQUIRE(editor->editTexture(0, image));
    REQUIRE(editor->commit());

    const auto reader = uomul::TexMapsReader::open(index, data);
    REQUIRE(reader);
    REQUIRE(reader->readTexture(0) == image);
}

TEST_CASE("TexMapsEditor_create_Should_RejectMissingSourceIndex")
{
    const TestDirectory directory;
    const auto sourceIndex = directory.path() / "missing.idx";
    const auto sourceData = directory.path() / "source.mul";
    const auto outputIndex = directory.path() / "output.idx";
    const auto outputData = directory.path() / "output.mul";
    {
        const std::ofstream data(sourceData, std::ios::binary);
    }

    const auto editor = uomul::TexMapsEditor::create(sourceIndex, sourceData, outputIndex, outputData);

    REQUIRE_FALSE(editor);
    REQUIRE(editor.error().code == uomul::ErrorCode::io);
}

TEST_CASE("TexMapsEditor_create_Should_RejectMissingSourceData")
{
    const TestDirectory directory;
    const auto sourceIndex = directory.path() / "source.idx";
    const auto sourceData = directory.path() / "missing.mul";
    const auto outputIndex = directory.path() / "output.idx";
    const auto outputData = directory.path() / "output.mul";
    {
        std::ofstream index(sourceIndex, std::ios::binary);
        writeIndexEntry(index, 0, 0, 0);
    }

    const auto editor = uomul::TexMapsEditor::create(sourceIndex, sourceData, outputIndex, outputData);

    REQUIRE_FALSE(editor);
    REQUIRE(editor.error().code == uomul::ErrorCode::io);
}

TEST_CASE("TexMapsEditor_createEmpty_Should_RejectFileSizeLimit_If_StagingFileCannotBeResized")
{
    const TestDirectory directory;
    const auto index = directory.path() / "output.idx";
    const auto data = directory.path() / "output.mul";

    {
        const FileSizeLimitGuard guard(0);
        const auto editor = uomul::TexMapsEditor::createEmpty(index, data);
        REQUIRE_FALSE(editor);
        REQUIRE(editor.error().code == uomul::ErrorCode::io);
    }

    REQUIRE_FALSE(std::filesystem::exists(data));
    REQUIRE_FALSE(std::filesystem::exists(index));
}

TEST_CASE("TexMapsEditor_editTexture_Should_StageLargeTexture_If_CoordinateIsValid")
{
    const TestDirectory directory;
    const auto index = directory.path() / "output.idx";
    const auto data = directory.path() / "output.mul";
    auto editor = uomul::TexMapsEditor::createEmpty(index, data);
    REQUIRE(editor);

    uomul::TextureImage largeImage;
    largeImage.dimension = uomul::textureLargeDimension;
    largeImage.pixels.resize(uomul::textureLargePixelCount, largeTextureSample);

    REQUIRE(editor->editTexture(0, largeImage));
    REQUIRE(editor->commit());

    auto reader = uomul::TexMapsReader::open(index, data);
    REQUIRE(reader);
    REQUIRE(reader->readTexture(0) == largeImage);
}

TEST_CASE("TexMapsEditor_editTexture_Should_RejectOutOfRangeCoordinate")
{
    const TestDirectory directory;
    const auto index = directory.path() / "output.idx";
    const auto data = directory.path() / "output.mul";
    auto editor = uomul::TexMapsEditor::createEmpty(index, data);
    REQUIRE(editor);

    uomul::TextureImage image;
    image.dimension = uomul::textureSmallDimension;
    image.pixels.resize(uomul::textureSmallPixelCount, smallTextureSample);

    const auto result = editor->editTexture(uomul::textureCount, image);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::invalidCoordinate);
}

TEST_CASE("TexMapsEditor_editTexture_Should_RejectInvalidDimensions")
{
    const TestDirectory directory;
    const auto index = directory.path() / "output.idx";
    const auto data = directory.path() / "output.mul";
    auto editor = uomul::TexMapsEditor::createEmpty(index, data);
    REQUIRE(editor);

    uomul::TextureImage badImage;
    badImage.dimension = invalidDimensionValue; // Invalid dimension
    badImage.pixels.resize(invalidDimensionValue, smallTextureSample);

    const auto result = editor->editTexture(0, badImage);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::invalidDimensions);
}

TEST_CASE("TexMapsEditor_editTexture_Should_RejectMismatchedPixelCount")
{
    const TestDirectory directory;
    const auto index = directory.path() / "output.idx";
    const auto data = directory.path() / "output.mul";
    auto editor = uomul::TexMapsEditor::createEmpty(index, data);
    REQUIRE(editor);

    uomul::TextureImage badImage;
    badImage.dimension = uomul::textureSmallDimension;
    badImage.pixels.resize(invalidDimensionValue, smallTextureSample); // Wrong count

    const auto result = editor->editTexture(0, badImage);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::invalidDimensions);
}

TEST_CASE("TexMapsEditor_editTexture_Should_ExtendIndex_If_EditingBeyondSourceEntries")
{
    const TestDirectory directory;
    const auto sourceIndex = directory.path() / "source.idx";
    const auto sourceData = directory.path() / "source.mul";
    const auto outputIndex = directory.path() / "output.idx";
    const auto outputData = directory.path() / "output.mul";

    writeTexture(sourceIndex, sourceData, 0, uomul::textureSmallDimension, sourceTextureSample);

    auto editor = uomul::TexMapsEditor::create(sourceIndex, sourceData, outputIndex, outputData);
    REQUIRE(editor);

    uomul::TextureImage image;
    image.dimension = uomul::textureSmallDimension;
    image.pixels.resize(uomul::textureSmallPixelCount, editedTextureSample);

    REQUIRE(editor->editTexture(5, image));
    REQUIRE(editor->commit());

    auto reader = uomul::TexMapsReader::open(outputIndex, outputData);
    REQUIRE(reader);
    REQUIRE(reader->textureCount() == 6);
    REQUIRE(reader->hasTexture(0).value());
    REQUIRE_FALSE(reader->hasTexture(4).value());
    REQUIRE(reader->readTexture(5) == image);
}

TEST_CASE("TexMapsEditor_editTexture_Should_ReturnInactiveError_If_EditorWasAborted")
{
    const TestDirectory directory;
    const auto index = directory.path() / "output.idx";
    const auto data = directory.path() / "output.mul";
    auto editor = uomul::TexMapsEditor::createEmpty(index, data);
    REQUIRE(editor);

    editor->abort();

    uomul::TextureImage image;
    image.dimension = uomul::textureSmallDimension;
    image.pixels.resize(uomul::textureSmallPixelCount, smallTextureSample);

    const auto result = editor->editTexture(0, image);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::commitFailed);
}

TEST_CASE("TexMapsEditor_clearTexture_Should_EmptySlot_If_CoordinateIsValid")
{
    const TestDirectory directory;
    const auto index = directory.path() / "output.idx";
    const auto data = directory.path() / "output.mul";
    auto editor = uomul::TexMapsEditor::createEmpty(index, data);
    REQUIRE(editor);

    uomul::TextureImage image;
    image.dimension = uomul::textureSmallDimension;
    image.pixels.resize(uomul::textureSmallPixelCount, editedTextureSample);

    REQUIRE(editor->editTexture(0, image));
    REQUIRE(editor->clearTexture(0));
    REQUIRE(editor->commit());

    auto reader = uomul::TexMapsReader::open(index, data);
    REQUIRE(reader);
    REQUIRE_FALSE(reader->hasTexture(0).value());
}

TEST_CASE("TexMapsEditor_clearTexture_Should_RejectOutOfRangeCoordinate")
{
    const TestDirectory directory;
    const auto index = directory.path() / "output.idx";
    const auto data = directory.path() / "output.mul";
    auto editor = uomul::TexMapsEditor::createEmpty(index, data);
    REQUIRE(editor);

    const auto result = editor->clearTexture(uomul::textureCount);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::invalidCoordinate);
}

TEST_CASE("TexMapsEditor_clearTexture_Should_ExtendIndex_If_ClearingBeyondSourceEntries")
{
    const TestDirectory directory;
    const auto sourceIndex = directory.path() / "source.idx";
    const auto sourceData = directory.path() / "source.mul";
    const auto outputIndex = directory.path() / "output.idx";
    const auto outputData = directory.path() / "output.mul";

    writeTexture(sourceIndex, sourceData, 0, uomul::textureSmallDimension, sourceTextureSample);

    auto editor = uomul::TexMapsEditor::create(sourceIndex, sourceData, outputIndex, outputData);
    REQUIRE(editor);
    REQUIRE(editor->clearTexture(5));
    REQUIRE(editor->commit());

    auto reader = uomul::TexMapsReader::open(outputIndex, outputData);
    REQUIRE(reader);
    REQUIRE(reader->textureCount() == 6);
    REQUIRE(reader->hasTexture(0).value());
    REQUIRE_FALSE(reader->hasTexture(5).value());
}

TEST_CASE("TexMapsEditor_deduplicateTextures_Should_RemoveDuplicates_If_TexturesAreIdentical")
{
    const TestDirectory directory;
    const auto index = directory.path() / "output.idx";
    const auto data = directory.path() / "output.mul";
    auto editor = uomul::TexMapsEditor::createEmpty(index, data);
    REQUIRE(editor);

    uomul::TextureImage image;
    image.dimension = uomul::textureSmallDimension;
    image.pixels.resize(uomul::textureSmallPixelCount, editedTextureSample);

    REQUIRE(editor->editTexture(0, image));
    REQUIRE(editor->editTexture(1, image));
    REQUIRE(editor->editTexture(2, image));

    const auto report = editor->deduplicateTextures();
    REQUIRE(report);
    REQUIRE(report->uniquePayloads == 1);
    REQUIRE(report->duplicateEntries == 2);
}

TEST_CASE("TexMapsEditor_deduplicateTextures_Should_ReturnInactiveError_If_EditorIsInactive")
{
    const TestDirectory directory;
    const auto index = directory.path() / "output.idx";
    const auto data = directory.path() / "output.mul";
    auto editor = uomul::TexMapsEditor::createEmpty(index, data);
    REQUIRE(editor);

    editor->abort();

    const auto result = editor->deduplicateTextures();

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::commitFailed);
}

TEST_CASE("TexMapsEditor_abort_Should_DiscardChanges_If_EditorIsActive")
{
    const TestDirectory directory;
    const auto sourceIndex = directory.path() / "source.idx";
    const auto sourceData = directory.path() / "source.mul";
    const auto outputIndex = directory.path() / "output.idx";
    const auto outputData = directory.path() / "output.mul";
    writeTexture(sourceIndex, sourceData, 0, uomul::textureSmallDimension, sourceTextureSample);

    {
        auto editor = uomul::TexMapsEditor::create(sourceIndex, sourceData, outputIndex, outputData);
        REQUIRE(editor);
        uomul::TextureImage image;
        image.dimension = uomul::textureSmallDimension;
        image.pixels.resize(uomul::textureSmallPixelCount, editedTextureSample);
        REQUIRE(editor->editTexture(0, image));
        editor->abort();
        editor->abort(); // Double abort should be safe
        REQUIRE_FALSE(editor->editTexture(1, image));
        REQUIRE_FALSE(editor->commit());
    }

    REQUIRE_FALSE(std::filesystem::exists(outputIndex));
    REQUIRE_FALSE(std::filesystem::exists(outputData));
}

TEST_CASE("TexMapsEditor_commit_Should_RejectRepeatedOperations_If_EditorHasAlreadyCommitted")
{
    const TestDirectory directory;
    const auto index = directory.path() / "output.idx";
    const auto data = directory.path() / "output.mul";
    auto editor = uomul::TexMapsEditor::createEmpty(index, data);
    REQUIRE(editor);

    REQUIRE(editor->commit());

    uomul::TextureImage image;
    image.dimension = uomul::textureSmallDimension;
    image.pixels.resize(uomul::textureSmallPixelCount, smallTextureSample);
    REQUIRE_FALSE(editor->editTexture(0, image));
    REQUIRE_FALSE(editor->commit());
}

TEST_CASE("TexMapsEditor_moveConstructor_Should_TransferActiveEditor_If_EditorIsMoved")
{
    const TestDirectory directory;
    const auto index = directory.path() / "output.idx";
    const auto data = directory.path() / "output.mul";
    auto opened = uomul::TexMapsEditor::createEmpty(index, data);
    REQUIRE(opened);

    uomul::TextureImage image;
    image.dimension = uomul::textureSmallDimension;
    image.pixels.resize(uomul::textureSmallPixelCount, smallTextureSample);
    REQUIRE(opened->editTexture(0, image));

    uomul::TexMapsEditor editor(std::move(*opened));

    REQUIRE(editor.commit());
    REQUIRE_FALSE(opened->editTexture(1, image));

    auto reader = uomul::TexMapsReader::open(index, data);
    REQUIRE(reader);
    REQUIRE(reader->readTexture(0) == image);
}

TEST_CASE("TexMapsEditor_moveAssignment_Should_TransferActiveEditor_If_DestinationIsActive")
{
    const TestDirectory directory;
    const auto firstIndex = directory.path() / "first.idx";
    const auto firstData = directory.path() / "first.mul";
    const auto secondIndex = directory.path() / "second.idx";
    const auto secondData = directory.path() / "second.mul";

    auto first = uomul::TexMapsEditor::createEmpty(firstIndex, firstData);
    auto second = uomul::TexMapsEditor::createEmpty(secondIndex, secondData);
    REQUIRE(first);
    REQUIRE(second);

    uomul::TextureImage image;
    image.dimension = uomul::textureSmallDimension;
    image.pixels.resize(uomul::textureSmallPixelCount, editedTextureSample);
    REQUIRE(first->editTexture(0, image));

    *first = std::move(*second);

    REQUIRE(first->commit());

    auto reader = uomul::TexMapsReader::open(secondIndex, secondData);
    REQUIRE(reader);
    REQUIRE(reader->textureCount() == uomul::textureCount);
    REQUIRE_FALSE(reader->hasTexture(0).value());
}

TEST_CASE("TexMapsEditor_create_Should_RejectExistingOutput_If_OverwriteIsDisabled")
{
    const TestDirectory directory;
    const auto sourceIndex = directory.path() / "source.idx";
    const auto sourceData = directory.path() / "source.mul";
    const auto outputIndex = directory.path() / "output.idx";
    const auto outputData = directory.path() / "output.mul";

    writeTexture(sourceIndex, sourceData, 0, uomul::textureSmallDimension, sourceTextureSample);

    {
        std::ofstream existing(outputIndex, std::ios::binary);
        existing.put('x');
    }

    const auto editor = uomul::TexMapsEditor::create(sourceIndex, sourceData, outputIndex, outputData);

    REQUIRE_FALSE(editor);
    REQUIRE(editor.error().code == uomul::ErrorCode::io);
}

TEST_CASE("TexMapsEditor_createEmpty_Should_RejectOutputWithEmptyFilename_If_PathNamesDirectory")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";

    const auto editor = uomul::TexMapsEditor::createEmpty(directory.path().string() + "/", source);

    REQUIRE_FALSE(editor);
    REQUIRE(editor.error().code == uomul::ErrorCode::io);
}

TEST_CASE("TexMapsEditor_create_Should_RejectMalformedSourceIndex")
{
    const TestDirectory directory;
    const auto sourceIndex = directory.path() / "source.idx";
    const auto sourceData = directory.path() / "source.mul";
    const auto outputIndex = directory.path() / "output.idx";
    const auto outputData = directory.path() / "output.mul";

    {
        std::ofstream index(sourceIndex, std::ios::binary);
        index.put('x');
    }
    {
        const std::ofstream data(sourceData, std::ios::binary);
    }

    const auto editor = uomul::TexMapsEditor::create(sourceIndex, sourceData, outputIndex, outputData);

    REQUIRE_FALSE(editor);
    REQUIRE(editor.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("TexMapsEditor_create_Should_RejectOutputDirectoryAsParent_If_ParentDoesNotExist")
{
    const TestDirectory directory;
    const auto sourceIndex = directory.path() / "source.idx";
    const auto sourceData = directory.path() / "source.mul";
    const auto outputIndex = directory.path() / "nonexistent" / "output.idx";
    const auto outputData = directory.path() / "nonexistent" / "output.mul";

    writeTexture(sourceIndex, sourceData, 0, uomul::textureSmallDimension, sourceTextureSample);

    const auto editor = uomul::TexMapsEditor::create(sourceIndex, sourceData, outputIndex, outputData);

    REQUIRE_FALSE(editor);
    REQUIRE(editor.error().code == uomul::ErrorCode::io);
}

TEST_CASE("TexMapsEditor_editTexture_Should_RollbackOnWriteFailure_If_FileSizeLimitExceeded")
{
    const TestDirectory directory;
    const auto index = directory.path() / "output.idx";
    const auto data = directory.path() / "output.mul";
    auto editor = uomul::TexMapsEditor::createEmpty(index, data);
    REQUIRE(editor);

    uomul::TextureImage image;
    image.dimension = uomul::textureSmallDimension;
    image.pixels.resize(uomul::textureSmallPixelCount, editedTextureSample);

    {
        const FileSizeLimitGuard guard(fileSizeLimitBytes);
        const auto result = editor->editTexture(0, image);
        REQUIRE_FALSE(result);
        REQUIRE(result.error().code == uomul::ErrorCode::io);
    }

    // Editor should still be active
    REQUIRE(editor->commit());
}

TEST_CASE("TexMapsEditor_deduplicateTextures_Should_FailGracefully_If_StagedDataIsTruncated")
{
    const TestDirectory directory;
    const auto index = directory.path() / "output.idx";
    const auto data = directory.path() / "output.mul";
    auto editor = uomul::TexMapsEditor::createEmpty(index, data);
    REQUIRE(editor);

    uomul::TextureImage image;
    image.dimension = uomul::textureSmallDimension;
    image.pixels.resize(uomul::textureSmallPixelCount, editedTextureSample);
    REQUIRE(editor->editTexture(0, image));

    // Locate and truncate the staged data file
    const auto stagingFiles = stagingFilesFor(directory.path(), "output.mul");
    REQUIRE(stagingFiles.size() >= 1);
    std::filesystem::resize_file(stagingFiles.at(0), 0);

    const auto result = editor->deduplicateTextures();

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::truncatedInput);

    // Editor should still be usable
    uomul::TextureImage newImage;
    newImage.dimension = uomul::textureSmallDimension;
    newImage.pixels.resize(uomul::textureSmallPixelCount, largeTextureSample);
    REQUIRE(editor->editTexture(1, newImage));
    REQUIRE(editor->commit());
}

TEST_CASE("TexMapsEditor_create_Should_RejectOutputPathsAreIdentical")
{
    const TestDirectory directory;
    const auto sourceIndex = directory.path() / "source.idx";
    const auto sourceData = directory.path() / "source.mul";
    const auto outputPath = directory.path() / "output.mul";

    writeTexture(sourceIndex, sourceData, 0, uomul::textureSmallDimension, sourceTextureSample);

    // Try to create with identical index and data paths
    const auto editor = uomul::TexMapsEditor::create(sourceIndex, sourceData, outputPath, outputPath);

    REQUIRE_FALSE(editor);
    REQUIRE(editor.error().code == uomul::ErrorCode::io);
}

TEST_CASE("TexMapsEditor_createEmpty_Should_Reject_If_IndexAndDataPathsAreIdentical")
{
    const TestDirectory directory;
    const auto outputPath = directory.path() / "output.mul";

    const auto editor = uomul::TexMapsEditor::createEmpty(outputPath, outputPath);

    REQUIRE_FALSE(editor);
    REQUIRE(editor.error().code == uomul::ErrorCode::io);
}

TEST_CASE("TexMapsEditor_editTexture_Should_ReplaceExistingTexture_If_SameIdEditedTwice")
{
    const TestDirectory directory;
    const auto index = directory.path() / "output.idx";
    const auto data = directory.path() / "output.mul";
    auto editor = uomul::TexMapsEditor::createEmpty(index, data);
    REQUIRE(editor);

    uomul::TextureImage firstImage;
    firstImage.dimension = uomul::textureSmallDimension;
    firstImage.pixels.resize(uomul::textureSmallPixelCount, smallTextureSample);

    uomul::TextureImage secondImage;
    secondImage.dimension = uomul::textureSmallDimension;
    secondImage.pixels.resize(uomul::textureSmallPixelCount, largeTextureSample);

    REQUIRE(editor->editTexture(0, firstImage));
    REQUIRE(editor->editTexture(0, secondImage));
    REQUIRE(editor->commit());

    auto reader = uomul::TexMapsReader::open(index, data);
    REQUIRE(reader);
    REQUIRE(reader->readTexture(0) == secondImage);
}

TEST_CASE("TexMapsEditor_clearTexture_Should_ReturnInactiveError_If_EditorWasAborted")
{
    const TestDirectory directory;
    const auto index = directory.path() / "output.idx";
    const auto data = directory.path() / "output.mul";
    auto editor = uomul::TexMapsEditor::createEmpty(index, data);
    REQUIRE(editor);

    editor->abort();

    const auto result = editor->clearTexture(0);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::commitFailed);
}

TEST_CASE("TexMapsEditor_editTexture_Should_ReturnInactiveError_If_EditorWasMovedFrom")
{
    const TestDirectory directory;
    const auto index = directory.path() / "output.idx";
    const auto data = directory.path() / "output.mul";
    auto opened = uomul::TexMapsEditor::createEmpty(index, data);
    REQUIRE(opened);

    const uomul::TexMapsEditor moved(std::move(*opened));
    (void) moved; // Use moved to avoid unused variable warning

    uomul::TextureImage image;
    image.dimension = uomul::textureSmallDimension;
    image.pixels.resize(uomul::textureSmallPixelCount, smallTextureSample);

    const auto result = opened->editTexture(0, image);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::commitFailed);
}

TEST_CASE("TexMapsEditor_deduplicateTextures_Should_HandleMixedSizesCorrectly")
{
    const TestDirectory directory;
    const auto index = directory.path() / "output.idx";
    const auto data = directory.path() / "output.mul";
    auto editor = uomul::TexMapsEditor::createEmpty(index, data);
    REQUIRE(editor);

    uomul::TextureImage smallImage;
    smallImage.dimension = uomul::textureSmallDimension;
    smallImage.pixels.resize(uomul::textureSmallPixelCount, smallTextureSample);

    uomul::TextureImage largeImage;
    largeImage.dimension = uomul::textureLargeDimension;
    largeImage.pixels.resize(uomul::textureLargePixelCount, largeTextureSample);

    REQUIRE(editor->editTexture(0, smallImage));
    REQUIRE(editor->editTexture(1, largeImage));
    REQUIRE(editor->editTexture(2, smallImage)); // Duplicate of 0

    const auto report = editor->deduplicateTextures();

    REQUIRE(report);
    REQUIRE(report->uniquePayloads == 2);
    REQUIRE(report->duplicateEntries == 1);
    REQUIRE(editor->commit());

    auto reader = uomul::TexMapsReader::open(index, data);
    REQUIRE(reader);
    REQUIRE(reader->readTexture(0) == smallImage);
    REQUIRE(reader->readTexture(1) == largeImage);
    REQUIRE(reader->readTexture(2) == smallImage);
}
