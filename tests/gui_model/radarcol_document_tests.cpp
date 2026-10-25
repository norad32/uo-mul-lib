#include "gui_model/documents/radarcol_document.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/radarcol/radarcol.h"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <system_error>

namespace
{

std::atomic<std::uint64_t> testDirectoryCounter = 0;

class TestDirectory
{
public:
    TestDirectory()
    {
        const auto sequence = testDirectoryCounter.fetch_add(1);
        path_ = std::filesystem::temp_directory_path() / ("radarcol-document-tests-" + std::to_string(sequence));
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

[[nodiscard]] std::expected<void, uomul::Error> createRadarcol(const std::filesystem::path & path)
{
    auto editor = uomul::RadarColEditor::createEmpty(path, {.overwriteOutput = true});
    if (!editor)
    {
        return std::unexpected(editor.error());
    }
    return editor->commit();
}

[[nodiscard]] std::uint16_t readColor(const uo_mule::gui::RadarcolDocument & document, std::uint32_t index)
{
    return document.rawColor(index).value_or(0U);
}

[[nodiscard]] std::uint16_t readCommittedColor(const uo_mule::gui::RadarcolDocument & document, std::uint32_t index)
{
    if (!document.reader())
    {
        return 0U;
    }
    return document.reader()->readRawColor(index).value_or(0U);
}

} // namespace

TEST_CASE("radarcolDocument_Should_LazilyStageBatchAndRefreshReaderOnSave")
{
    const TestDirectory directory;
    const auto sourcePath = directory.path() / "source.mul";
    REQUIRE(createRadarcol(sourcePath));

    auto document = uo_mule::gui::RadarcolDocument::open(
        7U, sourcePath); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE(document);
    REQUIRE_FALSE(document->editing());
    REQUIRE_FALSE(document->dirty());
    REQUIRE(readColor(*document, 3U) == 0U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    REQUIRE(document->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::editColors,
        .colorEdits = {{.colorIndex = 3U,
                        .color = 0x1234U}}, // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
        .outputPath = {},
        .overwriteOutput = false,
    }));
    REQUIRE(document->editing());
    REQUIRE(document->dirty());
    REQUIRE(document->hasStagedColor(3U));            // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE(readColor(*document, 3U) == 0x1234U);     // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE(readCommittedColor(*document, 3U) == 0U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    REQUIRE(document->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::save,
        .colorEdits = {},
        .outputPath = {},
        .overwriteOutput = false,
    }));
    REQUIRE_FALSE(document->editing());
    REQUIRE_FALSE(document->dirty());
    REQUIRE_FALSE(document->hasStagedColor(3U));  // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE(readColor(*document, 3U) == 0x1234U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
}

TEST_CASE("radarcolDocument_Should_RetainChanges_If_SaveCannotReopenOutput")
{
    const TestDirectory directory;
    const auto sourcePath = directory.path() / "source.mul";
    const auto missingOutput = directory.path() / "missing" / "output.mul";
    REQUIRE(createRadarcol(sourcePath));

    auto document = uo_mule::gui::RadarcolDocument::open(
        13U, sourcePath); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE(document);
    REQUIRE(document->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::editColors,
        .colorEdits = {{.colorIndex = 3U,
                        .color = 0x1234U}}, // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
        .outputPath = {},
        .overwriteOutput = false,
    }));

    document->setOutputPath(missingOutput);
    REQUIRE_FALSE(document->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::save,
        .colorEdits = {},
        .outputPath = {},
        .overwriteOutput = false,
    }));
    REQUIRE(document->dirty());
    REQUIRE(document->editing() == false);
    REQUIRE(document->hasStagedColor(3U)); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    document->setOutputPath(sourcePath);
    REQUIRE(document->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::save,
        .colorEdits = {},
        .outputPath = {},
        .overwriteOutput = false,
    }));
    REQUIRE_FALSE(document->dirty());
    REQUIRE_FALSE(document->hasStagedColor(3U));  // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE(readColor(*document, 3U) == 0x1234U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
}

TEST_CASE("radarcolDocument_Should_StageMultipleColorsAndDiscardThem")
{
    const TestDirectory directory;
    const auto sourcePath = directory.path() / "source.mul";
    REQUIRE(createRadarcol(sourcePath));

    auto document = uo_mule::gui::RadarcolDocument::open(
        8U, sourcePath); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE(document);
    REQUIRE(document->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::editColors,
        .colorEdits =
            {
                {.colorIndex = 4U, .color = 0x4321U}, // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
                {.colorIndex = 5U, .color = 0x2345U}, // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
            },
        .outputPath = {},
        .overwriteOutput = false,
    }));
    REQUIRE(readColor(*document, 4U) == 0x4321U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE(readColor(*document, 5U) == 0x2345U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    REQUIRE(document->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::discard,
        .colorEdits = {},
        .outputPath = {},
        .overwriteOutput = false,
    }));
    REQUIRE_FALSE(document->editing());
    REQUIRE_FALSE(document->dirty());
    REQUIRE_FALSE(document->hasStagedColor(4U)); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE(readColor(*document, 4U) == 0U);
    REQUIRE(readColor(*document, 5U) == 0U);
}

TEST_CASE("radarcolDocument_Should_KeepNoOpEditsClean")
{
    const TestDirectory directory;
    const auto sourcePath = directory.path() / "source.mul";
    REQUIRE(createRadarcol(sourcePath));

    auto document = uo_mule::gui::RadarcolDocument::open(
        9U, sourcePath); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE(document);
    REQUIRE(document->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::editColors,
        .colorEdits = {{.colorIndex = 0U, .color = 0U}},
        .outputPath = {},
        .overwriteOutput = false,
    }));
    REQUIRE_FALSE(document->editing());
    REQUIRE_FALSE(document->dirty());
}

TEST_CASE("radarcolDocument_Should_RejectInvalidAndOutOfRangeEdits")
{
    const TestDirectory directory;
    const auto sourcePath = directory.path() / "source.mul";
    REQUIRE(createRadarcol(sourcePath));

    auto document = uo_mule::gui::RadarcolDocument::open(
        11U, sourcePath); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE(document);
    REQUIRE_FALSE(document->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::editColors,
        .colorEdits = {{.colorIndex = uomul::radarColorCount, .color = 0U}},
        .outputPath = {},
        .overwriteOutput = false,
    }));
    REQUIRE_FALSE(document->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::editColors,
        .colorEdits = {{.colorIndex = 0U,
                        .color = 0x8000U}}, // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
        .outputPath = {},
        .overwriteOutput = false,
    }));
    REQUIRE_FALSE(document->editing());
    REQUIRE_FALSE(document->dirty());
}

TEST_CASE("radarcolDocument_Should_RejectSaveAsOutputCollision")
{
    const TestDirectory directory;
    const auto sourcePath = directory.path() / "source.mul";
    const auto outputPath = directory.path() / "output.mul";
    REQUIRE(createRadarcol(sourcePath));
    REQUIRE(createRadarcol(outputPath));

    auto document = uo_mule::gui::RadarcolDocument::open(
        12U, sourcePath); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE(document);
    REQUIRE_FALSE(document->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::saveAs,
        .colorEdits = {},
        .outputPath = outputPath,
        .overwriteOutput = false,
    }));
    REQUIRE(document->sourcePath() == sourcePath);
}
