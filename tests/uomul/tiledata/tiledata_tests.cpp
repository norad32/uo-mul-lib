#include "../test_helpers.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/tiledata/tiledata.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <string>

namespace
{

using uomul::TiledataLayout;
using uomul::test::TestDirectory;

constexpr std::uint16_t mapId = 0x1234;
constexpr std::uint32_t staticId = 0x4321;
constexpr std::uint64_t legacyMapFlags = 0x23456789ULL;
constexpr std::uint64_t extendedMapFlags = 0x123456789ULL;
constexpr std::uint16_t mapTextureId = 0x4567;
constexpr std::uint64_t legacyStaticFlags = 0x3456789AULL;
constexpr std::uint64_t extendedStaticFlags = 0x23456789AULL;
constexpr std::uint8_t testWeight = 7;
constexpr std::uint8_t testQuality = 8;
constexpr std::int16_t testMiscData = -9;
constexpr std::uint8_t testUnknown2 = 10;
constexpr std::uint8_t testQuantity = 11;
constexpr std::int16_t testAnimation = -12;
constexpr std::uint8_t testUnknown3 = 13;
constexpr std::uint8_t testHue = 14;
constexpr std::uint8_t testStackingOffset = 15;
constexpr std::uint8_t testValue = 16;
constexpr std::uint8_t testHeight = 17;

void createFixture(const std::filesystem::path & path)
{
    auto editor = uomul::TiledataEditor::createEmpty(path, TiledataLayout::extended64);
    REQUIRE(editor);
    REQUIRE(editor->editMapTile(3, {.name = "grass"}));
    REQUIRE(editor->commit());
}

[[nodiscard]] uomul::MapTileRecord mapRecord(uomul::TiledataLayout layout)
{
    return {
        .flags =
            static_cast<uomul::TiledataFlag>(layout == uomul::TiledataLayout::legacy32 ? legacyMapFlags : extendedMapFlags),
        .textureId = mapTextureId,
        .name = "test-map",
    };
}

[[nodiscard]] uomul::StaticTileRecord staticRecord(uomul::TiledataLayout layout)
{
    return {
        .flags = static_cast<uomul::TiledataFlag>(layout == uomul::TiledataLayout::legacy32 ? legacyStaticFlags
                                                                                            : extendedStaticFlags),
        .weight = testWeight,
        .quality = testQuality,
        .miscData = testMiscData,
        .unknown2 = testUnknown2,
        .quantity = testQuantity,
        .animation = testAnimation,
        .unknown3 = testUnknown3,
        .hue = testHue,
        .stackingOffset = testStackingOffset,
        .value = testValue,
        .height = testHeight,
        .name = "test-static",
    };
}

[[nodiscard]] bool roundTripLayout(const TestDirectory & directory, uomul::TiledataLayout layout)
{
    const auto path = directory.path() / (layout == TiledataLayout::legacy32 ? "legacy.mul" : "extended.mul");
    auto editor = uomul::TiledataEditor::createEmpty(path, layout);
    if (!editor || !editor->editMapTile(mapId, mapRecord(layout)) || !editor->editStaticTile(staticId, staticRecord(layout)) ||
        !editor->commit())
    {
        return false;
    }

    const auto reader = uomul::TiledataReader::open(path, layout);
    return reader && reader->layout() == layout && reader->mapTileCount() == uomul::tiledataMapTileCount &&
           reader->staticTileCount() == uomul::tiledataStaticTileCount && reader->readMapTile(mapId) == mapRecord(layout) &&
           reader->readStaticTile(staticId) == staticRecord(layout);
}

TEST_CASE("TiledataEditor_createEmpty_Should_RoundTripBothRecordKinds_If_LayoutIsSelected", "[uomul][tiledata][editor]")
{
    const TestDirectory directory;
    REQUIRE(roundTripLayout(directory, TiledataLayout::legacy32));
    REQUIRE(roundTripLayout(directory, TiledataLayout::extended64));
}

TEST_CASE("TiledataFlag_Should_ComposeAndTestTypedFlags", "[uomul][tiledata]")
{
    const auto flags = uomul::TiledataFlag::impassable | uomul::TiledataFlag::surface;

    REQUIRE(uomul::hasTiledataFlag(flags, uomul::TiledataFlag::impassable));
    REQUIRE(uomul::hasTiledataFlag(flags, uomul::TiledataFlag::surface));
    REQUIRE_FALSE(uomul::hasTiledataFlag(flags, uomul::TiledataFlag::wet));
}

TEST_CASE("TiledataReader_open_Should_ReadRealExtendedFixture", "[uomul][tiledata][reader]")
{
    const TestDirectory directory;
    const auto path = directory.path() / "tiledata.mul";
    createFixture(path);

    const auto reader = uomul::TiledataReader::open(path, TiledataLayout::extended64);

    REQUIRE(reader);
    REQUIRE(reader->layout() == TiledataLayout::extended64);
    REQUIRE(reader->mapTileCount() == 16384);
    REQUIRE(reader->staticTileCount() == 65536);
    REQUIRE(reader->readMapTile(3)->name == "grass");
}

TEST_CASE("TiledataReader_read_Should_RejectOutOfRangeIds", "[uomul][tiledata][reader]")
{
    const TestDirectory directory;
    const auto path = directory.path() / "tiledata.mul";
    auto editor = uomul::TiledataEditor::createEmpty(path, TiledataLayout::extended64);
    REQUIRE(editor);
    REQUIRE(editor->commit());
    const auto reader = uomul::TiledataReader::open(path, TiledataLayout::extended64);
    REQUIRE(reader);

    REQUIRE(reader->readMapTile(uomul::tiledataMapTileCount).error().code == uomul::ErrorCode::invalidCoordinate);
    REQUIRE(reader->readStaticTile(uomul::tiledataStaticTileCount).error().code == uomul::ErrorCode::invalidCoordinate);
}

TEST_CASE("TiledataReader_open_Should_RejectWrongLayout", "[uomul][tiledata][reader]")
{
    const TestDirectory directory;
    const auto path = directory.path() / "tiledata.mul";
    createFixture(path);

    const auto reader = uomul::TiledataReader::open(path, TiledataLayout::legacy32);

    REQUIRE_FALSE(reader);
    REQUIRE(reader.error().code == uomul::ErrorCode::truncatedInput);
}

TEST_CASE("TiledataEditor_create_Should_PreserveSourceUntilCommit", "[uomul][tiledata][editor]")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    auto sourceEditor = uomul::TiledataEditor::createEmpty(source, TiledataLayout::extended64);
    REQUIRE(sourceEditor);
    REQUIRE(sourceEditor->editMapTile(0, mapRecord(TiledataLayout::extended64)));
    REQUIRE(sourceEditor->commit());
    const auto sourceBytes = uomul::test::readBytes(source);

    {
        auto editor = uomul::TiledataEditor::create(source, output, TiledataLayout::extended64);
        REQUIRE(editor);
        REQUIRE(editor->editMapTile(0, {.name = "changed"}));
        editor->abort();
    }

    REQUIRE_FALSE(std::filesystem::exists(output));
    REQUIRE(uomul::test::readBytes(source) == sourceBytes);
}

TEST_CASE("TiledataEditor_edit_Should_RejectNameLongerThanFixedField", "[uomul][tiledata][editor]")
{
    const TestDirectory directory;
    const auto path = directory.path() / "tiledata.mul";
    auto editor = uomul::TiledataEditor::createEmpty(path, TiledataLayout::extended64);
    REQUIRE(editor);

    const auto result = editor->editMapTile(0, {.name = std::string(uomul::tiledataNameByteCount + 1, 'x')});

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::invalidFormat);
}

} // namespace
