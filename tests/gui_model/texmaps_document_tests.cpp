#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/documents/map_document.h"
#include "gui_model/documents/texmaps_document.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/source/source_identity.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/texmaps/texmaps.h"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>

namespace
{

std::atomic<std::uint64_t> testDirectoryCounter = 0;
constexpr uo_mule::gui::DocumentId firstDocumentId = 7;
constexpr uo_mule::gui::DocumentId invalidDocumentId = 8;
constexpr uo_mule::gui::DocumentId saveAsDocumentId = 9;
constexpr uo_mule::gui::DocumentId readOnlyDocumentId = 10;
constexpr uo_mule::gui::DocumentId aliasDocumentId = 11;

class TestDirectory
{
public:
    TestDirectory()
    {
        const auto sequence = testDirectoryCounter.fetch_add(1);
        path_ = std::filesystem::temp_directory_path() / ("texmaps-document-tests-" + std::to_string(sequence));
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

[[nodiscard]] std::expected<void, uomul::Error> createPair(const std::filesystem::path & indexPath,
                                                           const std::filesystem::path & dataPath)
{
    auto editor = uomul::TexMapsEditor::createEmpty(indexPath, dataPath);
    if (!editor)
    {
        return std::unexpected(editor.error());
    }
    return editor->commit();
}

[[nodiscard]] uo_mule::gui::DocumentSummary pairSummary(const std::filesystem::path & indexPath,
                                                        const std::filesystem::path & dataPath)
{
    return {
        .identity =
            {
                .kind = uo_mule::gui::DocumentKind::texmapPair,
                .source = uo_mule::gui::makeRoleAwareDocumentSourceIdentity({
                    {.role = uo_mule::gui::SourceRole::index, .path = indexPath},
                    {.role = uo_mule::gui::SourceRole::data, .path = dataPath},
                }),
            },
        .displayName = indexPath.filename().string(),
        .sourceKind = uo_mule::gui::DocumentSourceKind::standalone,
        .access = uo_mule::gui::DocumentAccess::editable,
        .validation = uo_mule::gui::DocumentValidationState::unknown,
        .facetKey = std::nullopt,
        .dimensions = std::nullopt,
        .dirty = false,
    };
}

void requireOpenedPair(const uo_mule::gui::TexmapsDocument & document,
                       const std::filesystem::path & indexPath,
                       const std::filesystem::path & dataPath)
{
    REQUIRE(document.reader());
    if (document.reader())
    {
        REQUIRE(document.reader()->textureCount() == uomul::textureCount);
    }
    REQUIRE(document.sourceIndexPath() == indexPath);
    REQUIRE(document.sourceDataPath() == dataPath);
    REQUIRE(document.sourceIdentity() == uo_mule::gui::makeRoleAwareDocumentSourceIdentity({
                                             {.role = uo_mule::gui::SourceRole::index, .path = indexPath},
                                             {.role = uo_mule::gui::SourceRole::data, .path = dataPath},
                                         }));
    REQUIRE_FALSE(document.editing());
    REQUIRE_FALSE(document.dirty());
}

void requireStandaloneTexmaps(const uo_mule::gui::DocumentRecord * record)
{
    REQUIRE(record != nullptr);
    if (record == nullptr)
    {
        return;
    }
    REQUIRE(record->summary.access == uo_mule::gui::DocumentAccess::editable);
    REQUIRE(record->texmaps.has_value());
    if (record->texmaps)
    {
        REQUIRE_FALSE(record->texmaps->readOnly());
    }
}

void requireConfiguredTexmaps(const uo_mule::gui::DocumentRecord * record)
{
    REQUIRE(record != nullptr);
    if (record == nullptr)
    {
        return;
    }
    REQUIRE(record->summary.sourceKind == uo_mule::gui::DocumentSourceKind::configuredMetadata);
    REQUIRE(record->summary.access == uo_mule::gui::DocumentAccess::readOnly);
    REQUIRE(record->texmaps.has_value());
    if (record->texmaps)
    {
        REQUIRE(record->texmaps->readOnly());
    }
}

} // namespace

TEST_CASE("texmapsDocument_Should_OpenValidatedRoleAwarePairWithoutCreatingStagedOutput")
{
    const TestDirectory directory;
    const auto indexPath = directory.path() / "texidx.mul";
    const auto dataPath = directory.path() / "texmaps.mul";
    REQUIRE(createPair(indexPath, dataPath));

    auto document = uo_mule::gui::TexmapsDocument::open(firstDocumentId, indexPath, dataPath);
    REQUIRE(document);
    if (document)
    {
        requireOpenedPair(*document, indexPath, dataPath);
    }
}

TEST_CASE("texmapsDocument_Should_PreserveInvalidPairErrors")
{
    const TestDirectory directory;
    const auto indexPath = directory.path() / "texidx.mul";
    const auto dataPath = directory.path() / "missing.mul";
    const auto document = uo_mule::gui::TexmapsDocument::invalid(
        invalidDocumentId, indexPath, dataPath, uo_mule::gui::DocumentAccess::editable, "texture data is missing");

    REQUIRE_FALSE(document.reader());
    REQUIRE(document.error() == "texture data is missing");
    REQUIRE(document.sourceIndexPath() == indexPath);
    REQUIRE(document.sourceDataPath() == dataPath);
}

TEST_CASE("texmapsDocument_Should_SaveAsBothPairFilesAndUpdateIdentity")
{
    const TestDirectory directory;
    const auto sourceIndex = directory.path() / "source.idx";
    const auto sourceData = directory.path() / "source.mul";
    const auto outputIndex = directory.path() / "output.idx";
    const auto outputData = directory.path() / "output.mul";
    REQUIRE(createPair(sourceIndex, sourceData));

    auto document = uo_mule::gui::TexmapsDocument::open(saveAsDocumentId, sourceIndex, sourceData);
    REQUIRE(document);
    REQUIRE(document->handleIntent({
        .kind = uo_mule::gui::TexmapsDocumentIntentKind::saveAs,
        .outputIndexPath = outputIndex,
        .outputDataPath = outputData,
        .overwriteOutput = false,
    }));

    REQUIRE(std::filesystem::exists(outputIndex));
    REQUIRE(std::filesystem::exists(outputData));
    REQUIRE(document->sourceIndexPath() == outputIndex);
    REQUIRE(document->sourceDataPath() == outputData);
    REQUIRE(document->sourceIdentity() == uo_mule::gui::makeRoleAwareDocumentSourceIdentity({
                                              {.role = uo_mule::gui::SourceRole::index, .path = outputIndex},
                                              {.role = uo_mule::gui::SourceRole::data, .path = outputData},
                                          }));
    REQUIRE_FALSE(document->editing());
    REQUIRE_FALSE(document->dirty());
}

TEST_CASE("texmapsDocument_Should_RejectReadOnlySaveAsAndPairAliases")
{
    const TestDirectory directory;
    const auto sourceIndex = directory.path() / "source.idx";
    const auto sourceData = directory.path() / "source.mul";
    const auto outputIndex = directory.path() / "output.idx";
    const auto outputData = directory.path() / "output.mul";
    REQUIRE(createPair(sourceIndex, sourceData));

    auto readOnly = uo_mule::gui::TexmapsDocument::open(
        readOnlyDocumentId, sourceIndex, sourceData, uo_mule::gui::DocumentAccess::readOnly);
    REQUIRE(readOnly);
    REQUIRE_FALSE(readOnly->handleIntent({
        .kind = uo_mule::gui::TexmapsDocumentIntentKind::saveAs,
        .outputIndexPath = outputIndex,
        .outputDataPath = outputData,
        .overwriteOutput = false,
    }));
    REQUIRE_FALSE(std::filesystem::exists(outputIndex));
    REQUIRE_FALSE(std::filesystem::exists(outputData));

    auto editable = uo_mule::gui::TexmapsDocument::open(aliasDocumentId, sourceIndex, sourceData);
    REQUIRE(editable);
    REQUIRE_FALSE(editable->handleIntent({
        .kind = uo_mule::gui::TexmapsDocumentIntentKind::saveAs,
        .outputIndexPath = sourceData,
        .outputDataPath = sourceIndex,
        .overwriteOutput = true,
    }));
    REQUIRE(editable->sourceIndexPath() == sourceIndex);
    REQUIRE(editable->sourceDataPath() == sourceData);
}

TEST_CASE("texmapsDocument_Should_RejectExistingSaveAsPairWithoutOverwrite")
{
    const TestDirectory directory;
    const auto sourceIndex = directory.path() / "source.idx";
    const auto sourceData = directory.path() / "source.mul";
    const auto outputIndex = directory.path() / "output.idx";
    const auto outputData = directory.path() / "output.mul";
    REQUIRE(createPair(sourceIndex, sourceData));
    REQUIRE(createPair(outputIndex, outputData));

    auto document = uo_mule::gui::TexmapsDocument::open(saveAsDocumentId, sourceIndex, sourceData);
    REQUIRE(document);
    REQUIRE_FALSE(document->handleIntent({
        .kind = uo_mule::gui::TexmapsDocumentIntentKind::saveAs,
        .outputIndexPath = outputIndex,
        .outputDataPath = outputData,
        .overwriteOutput = false,
    }));
    REQUIRE(document->sourceIndexPath() == sourceIndex);
    REQUIRE(document->sourceDataPath() == sourceData);
}

TEST_CASE("documentManager_Should_AttachEditableStandaloneAndReadOnlyConfiguredTexmapPairs")
{
    const TestDirectory directory;
    const auto indexPath = directory.path() / "texidx.mul";
    const auto dataPath = directory.path() / "texmaps.mul";
    REQUIRE(createPair(indexPath, dataPath));

    uo_mule::gui::DocumentManager documents;
    const auto standaloneId = documents.open(pairSummary(indexPath, dataPath));
    REQUIRE(standaloneId != 0U);
    requireStandaloneTexmaps(documents.find(standaloneId));

    uo_mule::gui::DataProfile profile;
    profile.files[uo_mule::gui::DataFileRole::texidx] = {.path = indexPath};
    profile.files[uo_mule::gui::DataFileRole::texmaps] = {.path = dataPath};
    documents.updateMetadataSources(profile);

    requireConfiguredTexmaps(documents.find(standaloneId));
}
