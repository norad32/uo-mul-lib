#include "gui/documents/radarcol_document.h"
#include "gui/documents/document_contracts.h"
#include "gui/documents/document_identity.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/radarcol/radarcol.h"
#include <cstdint>
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

RadarcolDocument::RadarcolDocument(DocumentId id,
                                   std::filesystem::path sourcePath,
                                   std::optional<uomul::RadarColReader> reader,
                                   DocumentAccess access,
                                   std::string error) :
    id_(id), sourcePath_(std::move(sourcePath)), reader_(std::move(reader)), error_(std::move(error)), access_(access)
{
}

std::expected<RadarcolDocument, uomul::Error>
RadarcolDocument::open(DocumentId id, const std::filesystem::path & sourcePath, DocumentAccess access)
{
    auto reader = uomul::RadarColReader::open(sourcePath);
    if (!reader)
    {
        return std::unexpected(reader.error());
    }

    return RadarcolDocument{id, sourcePath, std::move(*reader), access, {}};
}

RadarcolDocument
RadarcolDocument::invalid(DocumentId id, std::filesystem::path sourcePath, DocumentAccess access, std::string error)
{
    return RadarcolDocument{id, std::move(sourcePath), std::nullopt, access, std::move(error)};
}

std::expected<void, uomul::Error> RadarcolDocument::handleIntent(const RadarcolDocumentIntent & intent)
{
    std::expected<void, uomul::Error> result;
    switch (intent.kind)
    {
        case RadarcolDocumentIntentKind::beginEdit :
            result = beginEditing(intent.outputPath, intent.overwriteOutput);
            break;
        case RadarcolDocumentIntentKind::editColor :
            result = editColor(intent.colorIndex, intent.color);
            break;
        case RadarcolDocumentIntentKind::save :
            result = save();
            break;
        case RadarcolDocumentIntentKind::discard :
            result = discard();
            break;
        case RadarcolDocumentIntentKind::saveAs :
            result = saveAs(intent.outputPath, intent.overwriteOutput);
            break;
    }

    if (!result)
    {
        error_ = result.error().message;
    }
    else
    {
        error_.clear();
    }
    return result;
}

void RadarcolDocument::setAccess(DocumentAccess value) noexcept
{
    access_ = value;
    if (readOnly() && editor_)
    {
        editor_->abort();
        editor_.reset();
        dirtyState_ = false;
    }
}

std::expected<void, uomul::Error> RadarcolDocument::processPendingIntent()
{
    if (!pendingIntent_)
    {
        return {};
    }
    const RadarcolDocumentIntent intent = std::move(*pendingIntent_);
    pendingIntent_.reset();
    return handleIntent(intent);
}

std::expected<void, uomul::Error> RadarcolDocument::beginEditing(const std::filesystem::path & output, bool outputOverwrite)
{
    if (readOnly())
    {
        return fail(makeDocumentError(sourcePath_, "configured Radarcol source is read-only"));
    }
    if (reader_ == std::nullopt)
    {
        return fail(makeDocumentError(sourcePath_, "Radarcol source is not available"));
    }
    if (output.empty())
    {
        return fail(makeDocumentError(output, "Radarcol output path cannot be empty"));
    }

    auto staged = uomul::RadarColEditor::create(sourcePath_, output, {.overwriteOutput = outputOverwrite});
    if (!staged)
    {
        return fail(staged.error());
    }

    outputPath_ = output;
    editor_.emplace(std::move(*staged));
    dirtyState_ = false;
    return {};
}

std::expected<void, uomul::Error> RadarcolDocument::editColor(std::uint32_t index, uomul::RadarColor color)
{
    if (!editor_)
    {
        return fail(makeDocumentError(sourcePath_, "Radarcol editing has not started"));
    }
    if (auto edited = editor_->editRawColor(index, color); !edited)
    {
        return fail(edited.error());
    }
    dirtyState_ = true;
    return {};
}

std::expected<void, uomul::Error> RadarcolDocument::save()
{
    if (!editor_)
    {
        return fail(makeDocumentError(outputPath_, "Radarcol editing has not started"));
    }
    auto inPlace = uomul::detail::MulCommon::pathsExactlyMatch(sourcePath_, outputPath_);
    if (!inPlace)
    {
        return fail(inPlace.error());
    }

    if (*inPlace)
    {
        reader_.reset();
    }

    if (auto committed = editor_->commit(); !committed)
    {
        editor_->abort();
        editor_.reset();
        dirtyState_ = false;
        if (*inPlace)
        {
            auto restored = uomul::RadarColReader::open(sourcePath_);
            if (restored)
            {
                reader_ = std::move(*restored);
            }
        }
        return fail(committed.error());
    }

    auto opened = uomul::RadarColReader::open(outputPath_);
    if (!opened)
    {
        editor_.reset();
        dirtyState_ = false;
        return fail(opened.error());
    }

    sourcePath_ = outputPath_;
    reader_ = std::move(*opened);
    editor_.reset();
    dirtyState_ = false;
    return {};
}

std::expected<void, uomul::Error> RadarcolDocument::discard()
{
    if (editor_)
    {
        editor_->abort();
        editor_.reset();
    }
    dirtyState_ = false;
    return {};
}

std::expected<void, uomul::Error> RadarcolDocument::saveAs(const std::filesystem::path & output, bool outputOverwrite)
{
    if (editor_ && dirtyState_)
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
    if (auto started = beginEditing(output, outputOverwrite); !started)
    {
        return started;
    }
    return save();
}

std::expected<void, uomul::Error> RadarcolDocument::fail(uomul::Error documentError)
{
    error_ = documentError.message;
    return std::unexpected(std::move(documentError));
}

} // namespace uo_mule::gui
