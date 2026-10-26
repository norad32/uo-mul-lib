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
#include <utility>
#include <vector>

namespace
{

std::atomic<std::uint64_t> testDirectoryCounter = 0;
constexpr uo_mule::gui::DocumentId firstDocumentId = 7;
constexpr uo_mule::gui::DocumentId invalidDocumentId = 8;
constexpr uo_mule::gui::DocumentId saveAsDocumentId = 9;
constexpr uo_mule::gui::DocumentId aliasDocumentId = 11;
constexpr std::uint32_t firstTextureId = 2;
constexpr std::uint32_t secondTextureId = 7;
constexpr std::uint32_t committedTextureId = 3;
constexpr std::uint32_t emptyTextureId = 4;
constexpr std::uint16_t smallTextureSample = 0x1234;
constexpr std::uint16_t largeTextureSample = 0xABCD;
constexpr std::uint16_t replacementTextureSample = 0x5678;
constexpr uomul::TextureDimension invalidTextureDimension = 32;

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

[[nodiscard]] std::expected<void, uomul::Error> createSet(const std::filesystem::path & indexPath,
                                                          const std::filesystem::path & dataPath)
{
    auto editor = uomul::TexMapsEditor::createEmpty(indexPath, dataPath);
    if (!editor)
    {
        return std::unexpected(editor.error());
    }
    return editor->commit();
}

[[nodiscard]] std::expected<void, uomul::Error>
createSetWithTextures(const std::filesystem::path & indexPath,
                      const std::filesystem::path & dataPath,
                      const std::vector<uo_mule::gui::TexmapsTextureEdit> & edits)
{
    auto editor = uomul::TexMapsEditor::createEmpty(indexPath, dataPath);
    if (!editor)
    {
        return std::unexpected(editor.error());
    }
    for (const auto & edit : edits)
    {
        if (auto staged = editor->editTexture(edit.textureId, edit.texture); !staged)
        {
            return std::unexpected(staged.error());
        }
    }
    return editor->commit();
}

[[nodiscard]] uomul::TextureImage makeTexture(uomul::TextureDimension dimension, std::uint16_t sample)
{
    const auto pixelCount =
        dimension == uomul::textureLargeDimension ? uomul::textureLargePixelCount : uomul::textureSmallPixelCount;
    return {.dimension = dimension, .pixels = std::vector<std::uint16_t>(pixelCount, sample)};
}

[[nodiscard]] uo_mule::gui::TexmapsDocumentIntent replaceIntent(std::uint32_t textureId, const uomul::TextureImage & texture)
{
    uo_mule::gui::TexmapsDocumentIntent intent;
    intent.kind = uo_mule::gui::TexmapsDocumentIntentKind::replace;
    intent.textureId = textureId;
    intent.texture = texture;
    return intent;
}

[[nodiscard]] uo_mule::gui::TexmapsDocumentIntent replaceBatchIntent(std::vector<uo_mule::gui::TexmapsTextureEdit> edits)
{
    uo_mule::gui::TexmapsDocumentIntent intent;
    intent.kind = uo_mule::gui::TexmapsDocumentIntentKind::replace;
    intent.textureEdits = std::move(edits);
    return intent;
}

[[nodiscard]] uo_mule::gui::TexmapsDocumentIntent idsIntent(uo_mule::gui::TexmapsDocumentIntentKind kind,
                                                            std::vector<std::uint32_t> textureIds)
{
    uo_mule::gui::TexmapsDocumentIntent intent;
    intent.kind = kind;
    intent.textureIds = std::move(textureIds);
    return intent;
}

[[nodiscard]] uo_mule::gui::TexmapsDocumentIntent lifecycleIntent(uo_mule::gui::TexmapsDocumentIntentKind kind,
                                                                  std::filesystem::path outputIndexPath = {},
                                                                  std::filesystem::path outputDataPath = {},
                                                                  bool overwriteOutput = false)
{
    uo_mule::gui::TexmapsDocumentIntent intent;
    intent.kind = kind;
    intent.outputIndexPath = std::move(outputIndexPath);
    intent.outputDataPath = std::move(outputDataPath);
    intent.overwriteOutput = overwriteOutput;
    return intent;
}

[[nodiscard]] uo_mule::gui::DocumentSummary setSummary(const std::filesystem::path & indexPath,
                                                       const std::filesystem::path & dataPath)
{
    return {
        .identity =
            {
                .kind = uo_mule::gui::DocumentKind::texmaps,
                .source = uo_mule::gui::makeRoleAwareDocumentSourceIdentity({
                    {.role = uo_mule::gui::SourceRole::index, .path = indexPath},
                    {.role = uo_mule::gui::SourceRole::data, .path = dataPath},
                }),
            },
        .displayName = indexPath.filename().string(),
        .sourceKind = uo_mule::gui::DocumentSourceKind::standalone,
        .validation = uo_mule::gui::DocumentValidationState::unknown,
        .facetKey = std::nullopt,
        .dimensions = std::nullopt,
        .dirty = false,
    };
}

void requireOpenedSet(const uo_mule::gui::TexmapsDocument & document,
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
    REQUIRE(record->texmaps.has_value());
    if (record->texmaps)
    {
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
    REQUIRE(record->texmaps.has_value());
    if (record->texmaps)
    {
    }
}

void requireEffectiveTexture(const uo_mule::gui::TexmapsDocument & document,
                             std::uint32_t textureId,
                             const uomul::TextureImage & expected)
{
    const auto texture = document.effectiveTexture(textureId);
    REQUIRE(texture);
    if (texture && texture->has_value())
    {
        REQUIRE(texture->value() == expected);
    }
}

void requireEffectiveEmpty(const uo_mule::gui::TexmapsDocument & document, std::uint32_t textureId)
{
    const auto texture = document.effectiveTexture(textureId);
    REQUIRE(texture);
    if (texture)
    {
        REQUIRE_FALSE(texture->has_value());
    }
}

void requireReaderHasTexture(const uo_mule::gui::TexmapsDocument & document, std::uint32_t textureId)
{
    REQUIRE(document.reader());
    if (document.reader())
    {
        REQUIRE(document.reader()->hasTexture(textureId).value_or(false));
    }
}

void requireStagedSet(uo_mule::gui::TexmapsDocument & document,
                      const uomul::TextureImage & small,
                      const uomul::TextureImage & large)
{
    REQUIRE(document.handleIntent(replaceBatchIntent({
        {.textureId = firstTextureId, .texture = small},
        {.textureId = secondTextureId, .texture = large},
    })));
    REQUIRE(document.dirty());
    REQUIRE(document.editing());
    REQUIRE(document.hasStagedTexture(firstTextureId));
    REQUIRE(document.hasStagedTexture(secondTextureId));
    requireEffectiveTexture(document, firstTextureId, small);
    requireEffectiveTexture(document, secondTextureId, large);
}

void requireSavedSet(uo_mule::gui::TexmapsDocument & document,
                     const uomul::TextureImage & small,
                     const uomul::TextureImage & large)
{
    REQUIRE(document.handleIntent(lifecycleIntent(uo_mule::gui::TexmapsDocumentIntentKind::save)));
    REQUIRE_FALSE(document.dirty());
    REQUIRE_FALSE(document.editing());
    requireReaderHasTexture(document, firstTextureId);
    requireReaderHasTexture(document, secondTextureId);
    requireEffectiveTexture(document, firstTextureId, small);
    requireEffectiveTexture(document, secondTextureId, large);
}

} // namespace

TEST_CASE("texmapsDocument_Should_OpenValidatedRoleAwareSetWithoutCreatingStagedOutput")
{
    const TestDirectory directory;
    const auto indexPath = directory.path() / "texidx.mul";
    const auto dataPath = directory.path() / "texmaps.mul";
    REQUIRE(createSet(indexPath, dataPath));

    auto document = uo_mule::gui::TexmapsDocument::open(firstDocumentId, indexPath, dataPath);
    REQUIRE(document);
    if (document)
    {
        requireOpenedSet(*document, indexPath, dataPath);
    }
}

TEST_CASE("texmapsDocument_Should_PreserveInvalidSetErrors")
{
    const TestDirectory directory;
    const auto indexPath = directory.path() / "texidx.mul";
    const auto dataPath = directory.path() / "missing.mul";
    const auto document =
        uo_mule::gui::TexmapsDocument::invalid(invalidDocumentId, indexPath, dataPath, "texture data is missing");

    REQUIRE_FALSE(document.reader());
    REQUIRE(document.error() == "texture data is missing");
    REQUIRE(document.sourceIndexPath() == indexPath);
    REQUIRE(document.sourceDataPath() == dataPath);
}

TEST_CASE("texmapsDocument_Should_SaveAsBothSetFilesAndUpdateIdentity")
{
    const TestDirectory directory;
    const auto sourceIndex = directory.path() / "source.idx";
    const auto sourceData = directory.path() / "source.mul";
    const auto outputIndex = directory.path() / "output.idx";
    const auto outputData = directory.path() / "output.mul";
    REQUIRE(createSet(sourceIndex, sourceData));

    auto document = uo_mule::gui::TexmapsDocument::open(saveAsDocumentId, sourceIndex, sourceData);
    REQUIRE(document);
    REQUIRE(document->handleIntent({
        .kind = uo_mule::gui::TexmapsDocumentIntentKind::saveAs,
        .outputIndexPath = outputIndex,
        .outputDataPath = outputData,
        .overwriteOutput = false,
        .textureId = 0,
        .texture = {},
        .textureEdits = {},
        .textureIds = {},
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

TEST_CASE("texmapsDocument_Should_RejectSetAliases")
{
    const TestDirectory directory;
    const auto sourceIndex = directory.path() / "source.idx";
    const auto sourceData = directory.path() / "source.mul";
    REQUIRE(createSet(sourceIndex, sourceData));

    auto editable = uo_mule::gui::TexmapsDocument::open(aliasDocumentId, sourceIndex, sourceData);
    REQUIRE(editable);
    REQUIRE_FALSE(editable->handleIntent({
        .kind = uo_mule::gui::TexmapsDocumentIntentKind::saveAs,
        .outputIndexPath = sourceData,
        .outputDataPath = sourceIndex,
        .overwriteOutput = true,
        .textureId = 0,
        .texture = {},
        .textureEdits = {},
        .textureIds = {},
    }));
    REQUIRE(editable->sourceIndexPath() == sourceIndex);
    REQUIRE(editable->sourceDataPath() == sourceData);
}

TEST_CASE("texmapsDocument_Should_RejectExistingSaveAsSetWithoutOverwrite")
{
    const TestDirectory directory;
    const auto sourceIndex = directory.path() / "source.idx";
    const auto sourceData = directory.path() / "source.mul";
    const auto outputIndex = directory.path() / "output.idx";
    const auto outputData = directory.path() / "output.mul";
    REQUIRE(createSet(sourceIndex, sourceData));
    REQUIRE(createSet(outputIndex, outputData));

    auto document = uo_mule::gui::TexmapsDocument::open(saveAsDocumentId, sourceIndex, sourceData);
    REQUIRE(document);
    REQUIRE_FALSE(document->handleIntent({
        .kind = uo_mule::gui::TexmapsDocumentIntentKind::saveAs,
        .outputIndexPath = outputIndex,
        .outputDataPath = outputData,
        .overwriteOutput = false,
        .textureId = 0,
        .texture = {},
        .textureEdits = {},
        .textureIds = {},
    }));
    REQUIRE(document->sourceIndexPath() == sourceIndex);
    REQUIRE(document->sourceDataPath() == sourceData);
}

TEST_CASE("texmapsDocument_Should_StageSmallAndLargeReplacementsAndSaveThem")
{
    const TestDirectory directory;
    const auto indexPath = directory.path() / "source.idx";
    const auto dataPath = directory.path() / "source.mul";
    REQUIRE(createSet(indexPath, dataPath));

    auto document = uo_mule::gui::TexmapsDocument::open(firstDocumentId, indexPath, dataPath);
    REQUIRE(document);
    const auto small = makeTexture(uomul::textureSmallDimension, smallTextureSample);
    const auto large = makeTexture(uomul::textureLargeDimension, largeTextureSample);
    requireStagedSet(*document, small, large);
    requireSavedSet(*document, small, large);
}

TEST_CASE("texmapsDocument_Should_ClearRevertDiscardAndKeepNoOpsClean")
{
    const TestDirectory directory;
    const auto indexPath = directory.path() / "source.idx";
    const auto dataPath = directory.path() / "source.mul";
    const auto committed = makeTexture(uomul::textureSmallDimension, smallTextureSample);
    REQUIRE(createSetWithTextures(indexPath, dataPath, {{.textureId = committedTextureId, .texture = committed}}));

    auto document = uo_mule::gui::TexmapsDocument::open(saveAsDocumentId, indexPath, dataPath);
    REQUIRE(document);
    REQUIRE(document->handleIntent(
        replaceIntent(committedTextureId, makeTexture(uomul::textureSmallDimension, replacementTextureSample))));
    REQUIRE(document->dirty());
    REQUIRE(document->handleIntent(idsIntent(uo_mule::gui::TexmapsDocumentIntentKind::revert, {committedTextureId})));
    REQUIRE_FALSE(document->dirty());
    REQUIRE_FALSE(document->hasStagedTexture(committedTextureId));
    requireEffectiveTexture(*document, committedTextureId, committed);

    REQUIRE(document->handleIntent(idsIntent(uo_mule::gui::TexmapsDocumentIntentKind::clear, {committedTextureId})));
    REQUIRE(document->dirty());
    REQUIRE(document->hasStagedTexture(committedTextureId));
    requireEffectiveEmpty(*document, committedTextureId);
    REQUIRE(document->handleIntent(idsIntent(uo_mule::gui::TexmapsDocumentIntentKind::revert, {committedTextureId})));
    REQUIRE_FALSE(document->dirty());
    requireEffectiveTexture(*document, committedTextureId, committed);

    REQUIRE(document->handleIntent(idsIntent(uo_mule::gui::TexmapsDocumentIntentKind::clear, {emptyTextureId})));
    REQUIRE_FALSE(document->dirty());
    REQUIRE_FALSE(document->editing());
    REQUIRE(document->handleIntent(replaceIntent(committedTextureId, committed)));
    REQUIRE_FALSE(document->dirty());
    REQUIRE_FALSE(document->editing());

    REQUIRE(document->handleIntent(
        replaceIntent(committedTextureId, makeTexture(uomul::textureSmallDimension, replacementTextureSample))));
    REQUIRE(document->dirty());
    REQUIRE(document->handleIntent({.kind = uo_mule::gui::TexmapsDocumentIntentKind::discard,
                                    .outputIndexPath = {},
                                    .outputDataPath = {},
                                    .overwriteOutput = false,
                                    .textureId = 0,
                                    .texture = {},
                                    .textureEdits = {},
                                    .textureIds = {}}));
    REQUIRE_FALSE(document->dirty());
    REQUIRE_FALSE(document->editing());
    requireEffectiveTexture(*document, committedTextureId, committed);
}

TEST_CASE("texmapsDocument_Should_RejectInvalidTextureEditsBeforeStaging")
{
    const TestDirectory directory;
    const auto indexPath = directory.path() / "source.idx";
    const auto dataPath = directory.path() / "source.mul";
    REQUIRE(createSet(indexPath, dataPath));
    auto document = uo_mule::gui::TexmapsDocument::open(aliasDocumentId, indexPath, dataPath);
    REQUIRE(document);

    const auto invalidDimension = makeTexture(invalidTextureDimension, smallTextureSample);
    REQUIRE_FALSE(document->handleIntent(replaceIntent(firstTextureId, invalidDimension)));
    REQUIRE_FALSE(document->dirty());
    REQUIRE_FALSE(document->editing());

    auto invalidPixels = makeTexture(uomul::textureSmallDimension, smallTextureSample);
    invalidPixels.pixels.pop_back();
    REQUIRE_FALSE(document->handleIntent(replaceIntent(firstTextureId, invalidPixels)));
    REQUIRE_FALSE(document->dirty());
    REQUIRE_FALSE(document->editing());

    REQUIRE_FALSE(document->handleIntent(replaceIntent(uomul::textureCount, invalidDimension)));
    REQUIRE_FALSE(document->dirty());
    REQUIRE_FALSE(document->editing());
}

TEST_CASE("texmapsDocument_Should_SaveAsStagedSetAndUpdateEffectiveReader")
{
    const TestDirectory directory;
    const auto sourceIndex = directory.path() / "source.idx";
    const auto sourceData = directory.path() / "source.mul";
    const auto outputIndex = directory.path() / "output.idx";
    const auto outputData = directory.path() / "output.mul";
    REQUIRE(createSet(sourceIndex, sourceData));
    auto document = uo_mule::gui::TexmapsDocument::open(saveAsDocumentId, sourceIndex, sourceData);
    REQUIRE(document);
    const auto texture = makeTexture(uomul::textureLargeDimension, largeTextureSample);
    REQUIRE(document->handleIntent(replaceIntent(firstTextureId, texture)));
    REQUIRE(document->handleIntent({.kind = uo_mule::gui::TexmapsDocumentIntentKind::saveAs,
                                    .outputIndexPath = outputIndex,
                                    .outputDataPath = outputData,
                                    .overwriteOutput = false,
                                    .textureId = 0,
                                    .texture = {},
                                    .textureEdits = {},
                                    .textureIds = {}}));

    REQUIRE(document->sourceIndexPath() == outputIndex);
    REQUIRE(document->sourceDataPath() == outputData);
    REQUIRE_FALSE(document->dirty());
    REQUIRE_FALSE(document->hasStagedTexture(firstTextureId));
    requireEffectiveTexture(*document, firstTextureId, texture);
}

TEST_CASE("documentManager_Should_AttachEditableStandaloneAndReadOnlyConfiguredtexmapss")
{
    const TestDirectory directory;
    const auto indexPath = directory.path() / "texidx.mul";
    const auto dataPath = directory.path() / "texmaps.mul";
    REQUIRE(createSet(indexPath, dataPath));

    uo_mule::gui::DocumentManager documents;
    const auto standaloneId = documents.open(setSummary(indexPath, dataPath));
    REQUIRE(standaloneId != 0U);
    requireStandaloneTexmaps(documents.find(standaloneId));

    uo_mule::gui::DataProfile profile;
    profile.files[uo_mule::gui::DataFileRole::texidx] = {.path = indexPath};
    profile.files[uo_mule::gui::DataFileRole::texmaps] = {.path = dataPath};
    documents.updateMetadataSources(profile);

    requireConfiguredTexmaps(documents.find(standaloneId));
}
