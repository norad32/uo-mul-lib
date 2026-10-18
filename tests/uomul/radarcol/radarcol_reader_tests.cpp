#include "../map_test_helpers.h"
#include "uomul/radarcol/radarcol.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <utility>

namespace
{

using uomul::test::TestDirectory;

void writeColors(const std::filesystem::path & path, std::uint32_t count)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        const auto value = static_cast<std::uint16_t>((index * 17U) & 0x7FFFU);
        output.put(static_cast<char>(value & 0xFF));
        output.put(static_cast<char>(value >> 8));
    }
}

} // namespace

TEST_CASE("RadarColReader_open_Should_ReadMapStaticAndRawColors_If_TableUsesLittleEndianWords")
{
    const TestDirectory directory;
    const auto path = directory.path() / "radarcol.mul";
    writeColors(path, uomul::radarColorCount);

    auto reader = uomul::RadarColReader::open(path);

    REQUIRE(reader);
    REQUIRE(reader->colorCount() == uomul::radarColorCount);
    REQUIRE(reader->readMapColor(1) == 17);
    REQUIRE(reader->readStaticColor(0) == static_cast<uomul::RadarColor>((uomul::radarStaticColorOffset * 17U) & 0x7FFFU));
    REQUIRE(reader->readRawColor(uomul::radarColorCount - 1) ==
            static_cast<uomul::RadarColor>(((uomul::radarColorCount - 1) * 17U) & 0x7FFFU));
}

TEST_CASE("RadarColReader_open_Should_AcceptEmptyTable_If_FileContainsNoColors")
{
    const TestDirectory directory;
    const auto path = directory.path() / "empty.mul";
    writeColors(path, 0);

    const auto reader = uomul::RadarColReader::open(path);

    REQUIRE(reader);
    REQUIRE(reader->colorCount() == 0);
    const auto unavailable = reader->readRawColor(0);
    REQUIRE_FALSE(unavailable);
    REQUIRE(unavailable.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("RadarColReader_open_Should_RejectMissingFile_If_PathDoesNotExist")
{
    const TestDirectory directory;

    const auto reader = uomul::RadarColReader::open(directory.path() / "missing.mul");

    REQUIRE_FALSE(reader);
    REQUIRE(reader.error().code == uomul::ErrorCode::io);
}

TEST_CASE("RadarColReader_open_Should_AcceptMaximumTableSize_If_FileContainsExactlyLimit")
{
    const TestDirectory directory;
    const auto path = directory.path() / "maximum.mul";
    writeColors(path, uomul::radarColorCount);

    const auto reader = uomul::RadarColReader::open(path);

    REQUIRE(reader);
    REQUIRE(reader->colorCount() == uomul::radarColorCount);
    REQUIRE(reader->readRawColor(uomul::radarColorCount - 1));
}

TEST_CASE("RadarColReader_open_Should_RejectNonRegularFile_If_PathNamesDirectory")
{
    const TestDirectory directory;

    const auto reader = uomul::RadarColReader::open(directory.path());

    REQUIRE_FALSE(reader);
    REQUIRE(reader.error().code == uomul::ErrorCode::io);
}

TEST_CASE("RadarColReader_open_Should_RejectPartialColor_If_FileSizeIsOdd")
{
    const TestDirectory directory;
    const auto path = directory.path() / "odd.mul";
    {
        std::ofstream output(path, std::ios::binary);
        output.put('x');
    }

    const auto reader = uomul::RadarColReader::open(path);

    REQUIRE_FALSE(reader);
    REQUIRE(reader.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("RadarColReader_open_Should_RejectOversizedTable_If_ColorCountExceedsLimit")
{
    const TestDirectory directory;
    const auto path = directory.path() / "oversized.mul";
    writeColors(path, uomul::radarColorCount + 1);

    const auto reader = uomul::RadarColReader::open(path);

    REQUIRE_FALSE(reader);
    REQUIRE(reader.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("RadarColReader_readMapColor_Should_RejectInvalidId_If_IdExceedsMapColorCount")
{
    const TestDirectory directory;
    const auto path = directory.path() / "valid.mul";
    writeColors(path, 2);
    const auto reader = uomul::RadarColReader::open(path);
    REQUIRE(reader);

    const auto color = reader->readMapColor(uomul::radarMapColorCount);

    REQUIRE_FALSE(color);
    REQUIRE(color.error().code == uomul::ErrorCode::invalidCoordinate);
}

TEST_CASE("RadarColReader_readStaticColor_Should_RejectInvalidId_If_IdExceedsMapColorCount")
{
    const TestDirectory directory;
    const auto path = directory.path() / "valid.mul";
    writeColors(path, 2);
    const auto reader = uomul::RadarColReader::open(path);
    REQUIRE(reader);

    const auto color = reader->readStaticColor(uomul::radarMapColorCount);

    REQUIRE_FALSE(color);
    REQUIRE(color.error().code == uomul::ErrorCode::invalidCoordinate);
}

TEST_CASE("RadarColReader_readRawColor_Should_RejectInvalidIndex_If_IndexExceedsTableLimit")
{
    const TestDirectory directory;
    const auto path = directory.path() / "valid.mul";
    writeColors(path, 2);
    const auto reader = uomul::RadarColReader::open(path);
    REQUIRE(reader);

    const auto color = reader->readRawColor(uomul::radarColorCount);

    REQUIRE_FALSE(color);
    REQUIRE(color.error().code == uomul::ErrorCode::invalidCoordinate);
}

TEST_CASE("RadarColReader_readRawColor_Should_ReportUnavailableColor_If_IndexExceedsPrefix")
{
    const TestDirectory directory;
    const auto path = directory.path() / "partial.mul";
    writeColors(path, 4);
    const auto reader = uomul::RadarColReader::open(path);
    REQUIRE(reader);

    REQUIRE(reader->readRawColor(3) == 51);
    const auto unavailable = reader->readRawColor(4);

    REQUIRE_FALSE(unavailable);
    REQUIRE(unavailable.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("RadarColReader_readRawColor_Should_ReportReadFailure_If_FileIsTruncatedAfterOpening")
{
    const TestDirectory directory;
    const auto path = directory.path() / "radarcol.mul";
    writeColors(path, 2);
    const auto reader = uomul::RadarColReader::open(path);
    REQUIRE(reader);
    std::filesystem::resize_file(path, 0);

    const auto color = reader->readRawColor(0);

    REQUIRE_FALSE(color);
    REQUIRE(color.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("RadarColReader_moveConstructor_Should_KeepReaderUsable_If_ReaderIsMoved")
{
    const TestDirectory directory;
    const auto path = directory.path() / "radarcol.mul";
    writeColors(path, 2);
    auto opened = uomul::RadarColReader::open(path);
    REQUIRE(opened);

    uomul::RadarColReader reader(std::move(*opened));

    REQUIRE(reader.colorCount() == 2);
    REQUIRE(reader.readRawColor(1) == 17);
}

TEST_CASE("RadarColReader_moveAssignment_Should_ReplaceReaderState_If_AnotherReaderIsMovedIntoIt")
{
    const TestDirectory directory;
    const auto firstPath = directory.path() / "first.mul";
    const auto secondPath = directory.path() / "second.mul";
    writeColors(firstPath, 2);
    writeColors(secondPath, 3);
    auto first = uomul::RadarColReader::open(firstPath);
    auto second = uomul::RadarColReader::open(secondPath);
    REQUIRE(first);
    REQUIRE(second);

    *first = std::move(*second);

    REQUIRE(first->colorCount() == 3);
    REQUIRE(first->readRawColor(2) == 34);
}

TEST_CASE("RadarColReader_readRawColor_Should_ReportInactiveReader_If_ReaderWasMovedFrom")
{
    const TestDirectory directory;
    const auto path = directory.path() / "radarcol.mul";
    writeColors(path, 1);
    auto opened = uomul::RadarColReader::open(path);
    REQUIRE(opened);
    uomul::RadarColReader reader(std::move(*opened));

    const auto color = opened->readRawColor(0);

    REQUIRE_FALSE(color);
    REQUIRE(color.error().code == uomul::ErrorCode::io);
    REQUIRE(opened->colorCount() == 0);
}
