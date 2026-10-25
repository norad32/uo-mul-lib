#include "gui/workflows/texmap_export.h"
#include "gui/workflows/texmap_replacement.h"
#include "gui_model/documents/texmaps_document.h"
#include "gui_model/model/assets/asset_export_contracts.h"
#include "gui_model/model/assets/asset_window_contracts.h"
#include "image_codec/image_codec.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/texmaps/texmaps.h"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace
{

std::atomic<std::uint64_t> testDirectoryCounter = 0;
constexpr std::uint32_t firstTextureId = 10U;
constexpr std::uint32_t fallbackTextureId = 11U;
constexpr std::uint32_t duplicateTextureId = 12U;
constexpr std::uint32_t invalidFallbackTextureId = 13U;
constexpr std::uint32_t invalidTextureId = 14U;
constexpr std::uint32_t nestedTextureId = 15U;
constexpr std::uint32_t invalidImageDimension = 32U;
constexpr std::uint8_t bright = 0xE0U;
constexpr std::uint8_t dim = 0x20U;
constexpr std::uint8_t invalidPixel = 0x40U;

class TestDirectory
{
public:
    TestDirectory()
    {
        const auto sequence = testDirectoryCounter.fetch_add(1);
        path_ = std::filesystem::temp_directory_path() / ("texmap-replacement-tests-" + std::to_string(sequence));
        std::filesystem::create_directories(path_);
    }

    TestDirectory(const TestDirectory &) = delete;
    TestDirectory & operator=(const TestDirectory &) = delete;
    TestDirectory(TestDirectory &&) = delete;
    TestDirectory & operator=(TestDirectory &&) = delete;

    ~TestDirectory()
    {
        std::error_code ignoredError;
        std::filesystem::remove_all(path_, ignoredError);
    }

    [[nodiscard]] const std::filesystem::path & path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

[[nodiscard]] std::vector<std::uint8_t> makePixels(std::uint8_t red, std::uint8_t green, std::uint8_t blue)
{
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(uomul::textureSmallDimension) * uomul::textureSmallDimension * 3U,
                                     0U);
    for (std::size_t offset = 0; offset < pixels.size(); offset += 3U)
    {
        pixels.at(offset) = red;
        pixels.at(offset + 1U) = green;
        pixels.at(offset + 2U) = blue;
    }
    return pixels;
}

void writeBmp(const std::filesystem::path & path, const std::vector<std::uint8_t> & pixels)
{
    REQUIRE(uo_mule::image::writeBmp(path,
                                     uomul::textureSmallDimension,
                                     uomul::textureSmallDimension,
                                     std::span<const std::uint8_t>(pixels.data(), pixels.size())));
}

void writePng(const std::filesystem::path & path, const std::vector<std::uint8_t> & pixels)
{
    REQUIRE(uo_mule::image::writePng(path,
                                     uomul::textureSmallDimension,
                                     uomul::textureSmallDimension,
                                     std::span<const std::uint8_t>(pixels.data(), pixels.size())));
}

[[nodiscard]] std::expected<uo_mule::gui::TexmapsDocument, uomul::Error>
makeDocument(const std::filesystem::path & directory, const std::vector<uo_mule::gui::TexmapsTextureEdit> & edits)
{
    const auto indexPath = directory / "texidx.mul";
    const auto dataPath = directory / "texmaps.mul";
    auto editor = uomul::TexMapsEditor::createEmpty(indexPath, dataPath);
    if (!editor)
    {
        return std::unexpected(editor.error());
    }
    for (const auto & edit : edits)
    {
        if (auto result = editor->editTexture(edit.textureId, edit.texture); !result)
        {
            return std::unexpected(result.error());
        }
    }
    if (auto result = editor->commit(); !result)
    {
        return std::unexpected(result.error());
    }
    return uo_mule::gui::TexmapsDocument::open(1, indexPath, dataPath);
}

} // namespace

namespace
{

void requireProviderDragPayload()
{
    using uo_mule::gui::assets::AssetDragPayloadSource;
    using uo_mule::gui::assets::AssetSelection;

    const AssetSelection selection{.selected = {{.value = "23"}}, .anchor = {}, .anchorIndex = {}};
    const auto payload = uo_mule::gui::makeTexmapDragPayload(selection, true, AssetDragPayloadSource::provider);
    REQUIRE(payload);
    if (!payload)
    {
        return;
    }
    const auto & value = payload->values.at(0);
    REQUIRE(payload->type == uo_mule::gui::assets::texmapValueDragPayloadType);
    REQUIRE(value.item == 23U);
    REQUIRE(value.source == AssetDragPayloadSource::provider);
}

void requireDocumentDragPayload()
{
    using uo_mule::gui::assets::AssetDragPayloadSource;
    using uo_mule::gui::assets::AssetItemId;
    using uo_mule::gui::assets::AssetSelection;

    const AssetSelection selection{.selected = {{.value = "23"}}, .anchor = {}, .anchorIndex = {}};
    const auto payload = uo_mule::gui::makeTexmapDragPayload(selection, true, AssetDragPayloadSource::document, 42);
    REQUIRE(payload);
    if (!payload)
    {
        return;
    }
    const auto & value = payload->values.at(0);
    REQUIRE(value.sourceDocumentId == 42U);
    REQUIRE(value.source == AssetDragPayloadSource::document);

    const auto multiple =
        AssetSelection{.selected = {AssetItemId{.value = "23"}, AssetItemId{.value = "24"}}, .anchor = {}, .anchorIndex = {}};
    REQUIRE_FALSE(uo_mule::gui::makeTexmapDragPayload(multiple, true, AssetDragPayloadSource::provider));
}

void requireDragPayloadRejectsUnavailableSource()
{
    using uo_mule::gui::assets::AssetDragPayloadSource;
    using uo_mule::gui::assets::AssetSelection;

    const AssetSelection selection{.selected = {{.value = "23"}}, .anchor = {}, .anchorIndex = {}};
    REQUIRE_FALSE(uo_mule::gui::makeTexmapDragPayload(selection, false, AssetDragPayloadSource::provider));
}

} // namespace

TEST_CASE("texmapReplacementIds_Should_PrioritizeDecimalAndSupportHexadecimal", "[gui][texmap-replacement]")
{
    REQUIRE(uo_mule::gui::parseTexmapImageId("10") == 10U);
    REQUIRE(uo_mule::gui::parseTexmapImageId("0x10") == 16U);
    REQUIRE(uo_mule::gui::parseTexmapImageId("10A") == 266U);
    REQUIRE(uo_mule::gui::parseTexmapImageId("0X2a") == 42U);
    REQUIRE_FALSE(uo_mule::gui::parseTexmapImageId("0x"));
    REQUIRE_FALSE(uo_mule::gui::parseTexmapImageId("not-an-id"));
}

TEST_CASE("texmapReplacementScanner_Should_HandlePrecedenceFallbackDuplicatesAndNoOps", "[gui][texmap-replacement]")
{
    const TestDirectory directory;
    const auto folder = directory.path() / "images";
    std::filesystem::create_directories(folder / "nested");

    const auto red = makePixels(bright, dim, dim);
    const auto blue = makePixels(dim, dim, bright);
    const auto green = makePixels(dim, bright, dim);
    writeBmp(folder / "10.BMP", red);
    writePng(folder / "0xA.png", blue);
    writePng(folder / "11.PNG", green);
    {
        std::ofstream corrupt(folder / "11.bmp", std::ios::binary);
        corrupt << "not a bitmap";
    }
    writeBmp(folder / "12.bmp", red);
    writeBmp(folder / "12.BMP", blue);
    {
        const auto invalidPixels = std::vector<std::uint8_t>(
            static_cast<std::size_t>(invalidImageDimension) * invalidImageDimension * 3U, invalidPixel);
        REQUIRE(uo_mule::image::writeBmp(folder / "13.bmp", invalidImageDimension, invalidImageDimension, invalidPixels));
    }
    writePng(folder / "13.png", green);
    {
        const auto invalidPixels = std::vector<std::uint8_t>(
            static_cast<std::size_t>(invalidImageDimension) * invalidImageDimension * 3U, invalidPixel);
        REQUIRE(uo_mule::image::writeBmp(folder / "14.bmp", invalidImageDimension, invalidImageDimension, invalidPixels));
    }
    writeBmp(folder / "nested" / "15.bmp", red);

    const auto current = uo_mule::image::decode(folder / "10.BMP");
    REQUIRE(current);
    const auto currentTexture = uo_mule::image::toTexture(*current);
    REQUIRE(currentTexture);
    auto document = makeDocument(directory.path(), {{.textureId = firstTextureId, .texture = *currentTexture}});
    REQUIRE(document);

    const auto scan = uo_mule::gui::scanTexmapReplacementFolder(
        folder,
        {firstTextureId, fallbackTextureId, duplicateTextureId, invalidFallbackTextureId, invalidTextureId, nestedTextureId},
        *document);
    REQUIRE(scan);
    REQUIRE(scan->rows.size() == 6U);

    const auto & firstRow = scan->rows.at(0);
    const auto & fallbackRow = scan->rows.at(1);
    const auto & duplicateRow = scan->rows.at(2);
    const auto & invalidFallbackRow = scan->rows.at(3);
    const auto & invalidRow = scan->rows.at(4);
    const auto & nestedRow = scan->rows.back();
    REQUIRE(firstRow.state == uo_mule::gui::TexmapReplacementRowState::valid);
    REQUIRE(firstRow.sourcePath.filename() == "10.BMP");
    REQUIRE(firstRow.noOp);
    REQUIRE(fallbackRow.state == uo_mule::gui::TexmapReplacementRowState::valid);
    REQUIRE(fallbackRow.sourcePath.filename() == "11.PNG");
    REQUIRE(duplicateRow.state == uo_mule::gui::TexmapReplacementRowState::ambiguous);
    REQUIRE(invalidFallbackRow.state == uo_mule::gui::TexmapReplacementRowState::valid);
    REQUIRE(invalidFallbackRow.sourcePath.filename() == "13.png");
    REQUIRE(invalidRow.state == uo_mule::gui::TexmapReplacementRowState::invalid);
    REQUIRE(nestedRow.state == uo_mule::gui::TexmapReplacementRowState::missing);
}

TEST_CASE("texmapDragPayload_Should_RequireOneAvailableSourceTexture", "[gui][texmap-drag-drop]")
{
    requireProviderDragPayload();
    requireDocumentDragPayload();
    requireDragPayloadRejectsUnavailableSource();
}

TEST_CASE("texmapExport_Should_NameEncodeAndReportRecoverableFailures", "[gui][texmap-export]")
{
    const TestDirectory directory;
    const auto output = directory.path() / "exported";
    REQUIRE(std::filesystem::create_directories(output));

    const auto small = uomul::TextureImage{
        .dimension = uomul::textureSmallDimension,
        .pixels = std::vector<std::uint16_t>(uomul::textureSmallPixelCount, 0x7FFFU),
    };
    const auto large = uomul::TextureImage{
        .dimension = uomul::textureLargeDimension,
        .pixels = std::vector<std::uint16_t>(uomul::textureLargePixelCount, 0x001FU),
    };
    const std::vector<uo_mule::gui::TexmapExportItem> items = {
        {.textureId = 7U, .texture = small},
        {.textureId = 0xABCDU, .texture = large},
    };

    REQUIRE(uo_mule::gui::texmapExportFilename(7U, uo_mule::gui::assets::AssetExportFormat::bmp) == "0x0007.bmp");
    REQUIRE(uo_mule::gui::texmapExportFilename(0xABCDU, uo_mule::gui::assets::AssetExportFormat::png) == "0xABCD.png");

    uo_mule::gui::TexmapExportOptions options{
        .format = uo_mule::gui::assets::AssetExportFormat::bmp,
        .destination = uo_mule::gui::assets::AssetExportDestination::directory,
        .outputPath = output,
        .overwrite = false,
    };
    const auto report = uo_mule::gui::exportTexmaps(items, options);
    REQUIRE(report);
    REQUIRE(report->exported == 2U);
    REQUIRE(report->failures.empty());
    REQUIRE(std::filesystem::is_regular_file(output / "0x0007.bmp"));
    REQUIRE(std::filesystem::is_regular_file(output / "0xABCD.bmp"));

    const auto overwrite = uo_mule::gui::exportTexmaps(items, options);
    REQUIRE_FALSE(overwrite);
    REQUIRE(overwrite.error().message == "overwrite confirmation required");

    options.overwrite = true;
    const auto overwritten = uo_mule::gui::exportTexmaps(items, options);
    REQUIRE(overwritten);
    REQUIRE(overwritten->exported == 2U);

    const std::vector<uo_mule::gui::TexmapExportItem> mixed = {
        {.textureId = 7U, .texture = small},
        {.textureId = 8U, .texture = {.dimension = 32, .pixels = {}}},
    };
    options.overwrite = true;
    const auto partial = uo_mule::gui::exportTexmaps(mixed, options);
    REQUIRE(partial);
    REQUIRE(partial->exported == 1U);
    REQUIRE(partial->failures.size() == 1U);
}
