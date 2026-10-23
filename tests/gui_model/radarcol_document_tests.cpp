#include "gui_model/documents/document_contracts.h"
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
    const auto & reader = document.reader();
    return reader ? reader->readRawColor(index).value_or(0U) : 0U;
}

} // namespace

TEST_CASE("radarcolDocument_Should_StageColorEditsAndRefreshReaderOnSave")
{
    const TestDirectory directory;
    const auto sourcePath = directory.path() / "source.mul";
    const auto outputPath = directory.path() / "output.mul";
    REQUIRE(createRadarcol(sourcePath));

    auto document = uo_mule::gui::RadarcolDocument::open(
        7U, sourcePath); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE(document);
    REQUIRE_FALSE(document->editing());
    REQUIRE_FALSE(document->dirty());

    REQUIRE(document->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::beginEdit,
        .colorIndex = 0,
        .color = 0,
        .outputPath = outputPath,
        .overwriteOutput = false,
    }));
    REQUIRE(document->editing());
    REQUIRE_FALSE(document->dirty());
    REQUIRE(document->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::editColor,
        .colorIndex = 3,
        .color = 0x1234,
        .outputPath = {},
        .overwriteOutput = false,
    }));
    REQUIRE(document->dirty());
    REQUIRE(document->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::save,
        .colorIndex = 0,
        .color = 0,
        .outputPath = {},
        .overwriteOutput = false,
    }));
    REQUIRE_FALSE(document->editing());
    REQUIRE_FALSE(document->dirty());
    REQUIRE(document->sourcePath() == outputPath);
    REQUIRE(readColor(*document, 3U) == 0x1234U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
}

TEST_CASE("radarcolDocument_Should_DiscardStagedEditorWithoutChangingReader")
{
    const TestDirectory directory;
    const auto sourcePath = directory.path() / "source.mul";
    const auto outputPath = directory.path() / "output.mul";
    REQUIRE(createRadarcol(sourcePath));

    auto document = uo_mule::gui::RadarcolDocument::open(
        8U, sourcePath); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE(document);
    const auto original = readColor(*document, 4U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE(document->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::beginEdit,
        .colorIndex = 0,
        .color = 0,
        .outputPath = outputPath,
        .overwriteOutput = false,
    }));
    REQUIRE(document->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::editColor,
        .colorIndex = 4,
        .color = 0x4321,
        .outputPath = {},
        .overwriteOutput = false,
    }));
    REQUIRE(document->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::discard,
        .colorIndex = 0,
        .color = 0,
        .outputPath = {},
        .overwriteOutput = false,
    }));
    REQUIRE_FALSE(document->editing());
    REQUIRE_FALSE(document->dirty());
    REQUIRE(readColor(*document, 4U) == original); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE_FALSE(std::filesystem::exists(outputPath));
}

TEST_CASE("radarcolDocument_Should_RejectConfiguredSourcesAndAliasedOutputs")
{
    const TestDirectory directory;
    const auto sourcePath = directory.path() / "source.mul";
    REQUIRE(createRadarcol(sourcePath));

    auto configured = uo_mule::gui::RadarcolDocument::open(
        9U, // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
        sourcePath,
        uo_mule::gui::DocumentAccess::readOnly); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE(configured);
    REQUIRE_FALSE(configured->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::beginEdit,
        .colorIndex = 0,
        .color = 0,
        .outputPath = directory.path() / "output.mul",
        .overwriteOutput = false,
    }));
    REQUIRE_FALSE(configured->editing());

    auto standalone = uo_mule::gui::RadarcolDocument::open(
        10U, sourcePath); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE(standalone);
    REQUIRE(standalone->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::beginEdit,
        .colorIndex = 0,
        .color = 0,
        .outputPath = sourcePath,
        .overwriteOutput = false,
    }));
    REQUIRE(standalone->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::editColor,
        .colorIndex = 0,
        .color = 0x2345,
        .outputPath = {},
        .overwriteOutput = false,
    }));
    REQUIRE(standalone->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::save,
        .colorIndex = 0,
        .color = 0,
        .outputPath = {},
        .overwriteOutput = false,
    }));
    REQUIRE_FALSE(standalone->editing());
    REQUIRE(readColor(*standalone, 0U) == 0x2345U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    const auto aliasPath = directory.path() / "alias.mul";
    std::filesystem::create_hard_link(sourcePath, aliasPath);
    auto aliased = uo_mule::gui::RadarcolDocument::open(
        11U, sourcePath); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE(aliased);
    REQUIRE_FALSE(aliased->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::beginEdit,
        .colorIndex = 0,
        .color = 0,
        .outputPath = aliasPath,
        .overwriteOutput = true,
    }));
    REQUIRE_FALSE(aliased->editing());
}

TEST_CASE("radarcolDocument_Should_ResetFailedEditorAfterCommitFailure")
{
    const TestDirectory directory;
    const auto sourcePath = directory.path() / "source.mul";
    const auto outputPath = directory.path() / "output.mul";
    REQUIRE(createRadarcol(sourcePath));

    auto document = uo_mule::gui::RadarcolDocument::open(
        12U, sourcePath); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE(document);
    REQUIRE(document->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::beginEdit,
        .colorIndex = 0,
        .color = 0,
        .outputPath = outputPath,
        .overwriteOutput = false,
    }));
    REQUIRE(document->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::editColor,
        .colorIndex = 0,
        .color = 0x3456,
        .outputPath = {},
        .overwriteOutput = false,
    }));

    REQUIRE(createRadarcol(outputPath));
    REQUIRE_FALSE(document->handleIntent({
        .kind = uo_mule::gui::RadarcolDocumentIntentKind::save,
        .colorIndex = 0,
        .color = 0,
        .outputPath = {},
        .overwriteOutput = false,
    }));
    REQUIRE_FALSE(document->editing());
    REQUIRE_FALSE(document->dirty());
    REQUIRE(readColor(*document, 0U) == 0U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
}
