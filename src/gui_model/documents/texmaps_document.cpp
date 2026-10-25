#include "gui_model/documents/texmaps_document.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/texmaps/texmaps.h"
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

namespace uo_mule::gui
{

namespace
{

[[nodiscard]] uomul::Error makeDocumentError(const std::filesystem::path & path, std::string message)
{
    return {.code = uomul::ErrorCode::io, .path = path, .message = std::move(message)};
}

} // namespace

TexmapsDocument::TexmapsDocument(DocumentId id,
                                 std::filesystem::path indexPath,
                                 std::filesystem::path dataPath,
                                 std::optional<uomul::TexMapsReader> reader,
                                 DocumentAccess access,
                                 std::string error) :
    id_(id),
    sourceIndexPath_(std::move(indexPath)),
    sourceDataPath_(std::move(dataPath)),
    outputIndexPath_(sourceIndexPath_),
    outputDataPath_(sourceDataPath_),
    reader_(std::move(reader))
{
    state_.setAccess(access);
    state_.setError(std::move(error));
}

std::expected<TexmapsDocument, uomul::Error> TexmapsDocument::open(DocumentId id,
                                                                   const std::filesystem::path & indexPath,
                                                                   const std::filesystem::path & dataPath,
                                                                   DocumentAccess access)
{
    auto reader = uomul::TexMapsReader::open(indexPath, dataPath);
    if (!reader)
    {
        return std::unexpected(reader.error());
    }
    return TexmapsDocument{id, indexPath, dataPath, std::move(*reader), access, {}};
}

TexmapsDocument TexmapsDocument::invalid(DocumentId id,
                                         std::filesystem::path indexPath,
                                         std::filesystem::path dataPath,
                                         DocumentAccess access,
                                         std::string error)
{
    return TexmapsDocument{id, std::move(indexPath), std::move(dataPath), std::nullopt, access, std::move(error)};
}

std::expected<void, uomul::Error> TexmapsDocument::handleIntent(const TexmapsDocumentIntent & intent)
{
    std::expected<void, uomul::Error> result;
    switch (intent.kind)
    {
        case TexmapsDocumentIntentKind::save :
            result = save();
            break;
        case TexmapsDocumentIntentKind::discard :
            result = discard();
            break;
        case TexmapsDocumentIntentKind::saveAs :
            result = saveAs(intent.outputIndexPath, intent.outputDataPath, intent.overwriteOutput);
            break;
    }

    if (!result)
    {
        state_.setError(result.error().message);
    }
    else
    {
        state_.clearError();
    }
    return result;
}

void TexmapsDocument::setAccess(DocumentAccess value) noexcept
{
    state_.setAccess(value);
    if (readOnly() && editor_)
    {
        editor_->abort();
        editor_.reset();
        outputIndexPath_ = sourceIndexPath_;
        outputDataPath_ = sourceDataPath_;
        state_.resetChanges();
    }
}

std::expected<void, uomul::Error> TexmapsDocument::processPendingIntent()
{
    const auto intent = pendingIntent_.take();
    if (!intent)
    {
        return {};
    }
    return handleIntent(*intent);
}

std::expected<void, uomul::Error> TexmapsDocument::beginEditing(const std::filesystem::path & outputIndexPath,
                                                                const std::filesystem::path & outputDataPath,
                                                                bool outputOverwrite)
{
    if (readOnly())
    {
        return fail(makeDocumentError(sourceIndexPath_, "configured Texmap Pair source is read-only"));
    }
    if (!reader_)
    {
        return fail(makeDocumentError(sourceIndexPath_, "Texmap Pair source is not available"));
    }
    if (outputIndexPath.empty() || outputDataPath.empty())
    {
        return fail(makeDocumentError(outputIndexPath.empty() ? outputIndexPath : outputDataPath,
                                      "Texmap Pair output paths cannot be empty"));
    }

    auto staged = uomul::TexMapsEditor::create(
        sourceIndexPath_, sourceDataPath_, outputIndexPath, outputDataPath, {.overwriteOutput = outputOverwrite});
    if (!staged)
    {
        return fail(staged.error());
    }

    outputIndexPath_ = outputIndexPath;
    outputDataPath_ = outputDataPath;
    overwriteOutput_ = outputOverwrite;
    editor_.emplace(std::move(*staged));
    state_.setDirty(false);
    return {};
}

std::expected<void, uomul::Error> TexmapsDocument::save()
{
    if (!editor_)
    {
        return fail(makeDocumentError(outputIndexPath_, "Texmap Pair has no staged changes"));
    }

    auto indexInPlace = uomul::detail::MulCommon::pathsExactlyMatch(sourceIndexPath_, outputIndexPath_);
    if (!indexInPlace)
    {
        return fail(indexInPlace.error());
    }
    auto dataInPlace = uomul::detail::MulCommon::pathsExactlyMatch(sourceDataPath_, outputDataPath_);
    if (!dataInPlace)
    {
        return fail(dataInPlace.error());
    }

    const bool reopenReader = *indexInPlace || *dataInPlace;
    if (reopenReader)
    {
        reader_.reset();
    }

    if (auto committed = editor_->commit(); !committed)
    {
        if (reopenReader)
        {
            auto restored = uomul::TexMapsReader::open(sourceIndexPath_, sourceDataPath_);
            if (restored)
            {
                reader_ = std::move(*restored);
            }
        }
        return fail(committed.error());
    }

    auto opened = uomul::TexMapsReader::open(outputIndexPath_, outputDataPath_);
    if (!opened)
    {
        editor_.reset();
        return fail(opened.error());
    }

    sourceIndexPath_ = outputIndexPath_;
    sourceDataPath_ = outputDataPath_;
    reader_ = std::move(*opened);
    editor_.reset();
    overwriteOutput_ = false;
    state_.resetChanges();
    return {};
}

std::expected<void, uomul::Error> TexmapsDocument::discard()
{
    if (editor_)
    {
        editor_->abort();
        editor_.reset();
    }
    outputIndexPath_ = sourceIndexPath_;
    outputDataPath_ = sourceDataPath_;
    overwriteOutput_ = false;
    state_.resetChanges();
    return {};
}

std::expected<void, uomul::Error> TexmapsDocument::saveAs(const std::filesystem::path & outputIndexPath,
                                                          const std::filesystem::path & outputDataPath,
                                                          bool outputOverwrite)
{
    if (editor_ && state_.dirty())
    {
        if (auto saved = save(); !saved)
        {
            return saved;
        }
    }
    if (editor_)
    {
        editor_->abort();
        editor_.reset();
    }
    if (auto started = beginEditing(outputIndexPath, outputDataPath, outputOverwrite); !started)
    {
        return started;
    }
    return save();
}

std::expected<void, uomul::Error> TexmapsDocument::fail(uomul::Error documentError)
{
    state_.setError(documentError.message);
    return std::unexpected(std::move(documentError));
}

} // namespace uo_mule::gui
