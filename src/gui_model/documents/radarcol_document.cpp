#include "gui_model/documents/radarcol_document.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/radarcol/radarcol.h"
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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
    id_(id),
    sourcePath_(std::move(sourcePath)),
    outputPath_(sourcePath_),
    reader_(std::move(reader)),
    error_(std::move(error)),
    access_(access)
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
        case RadarcolDocumentIntentKind::editColors :
            result = editColors(intent.colorEdits);
            break;
        case RadarcolDocumentIntentKind::revertColors :
        {
            std::vector<std::uint32_t> indices;
            indices.reserve(intent.colorEdits.size());
            for (const auto & edit : intent.colorEdits)
            {
                indices.push_back(edit.colorIndex);
            }
            result = revertColors(indices);
            break;
        }
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
        stagedColors_.clear();
        dirtyState_ = false;
        displayRevision_++;
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

std::optional<uomul::RadarColor> RadarcolDocument::rawColor(std::uint32_t index) const
{
    if (const auto staged = stagedColors_.find(index); staged != stagedColors_.end())
    {
        return staged->second;
    }

    if (!reader_)
    {
        return std::nullopt;
    }

    const auto value = reader_->readRawColor(index);
    return value ? std::optional<uomul::RadarColor>(*value) : std::nullopt;
}

bool RadarcolDocument::hasStagedColor(std::uint32_t index) const noexcept
{
    return stagedColors_.contains(index);
}

std::expected<void, uomul::Error> RadarcolDocument::editColors(const std::vector<RadarcolColorEdit> & edits)
{
    if (readOnly())
    {
        return fail(makeDocumentError(sourcePath_, "configured Radarcol source is read-only"));
    }
    if (!reader_)
    {
        return fail(makeDocumentError(sourcePath_, "Radarcol source is not available"));
    }
    if (edits.empty())
    {
        return fail(makeDocumentError(sourcePath_, "Radarcol color edit cannot be empty"));
    }

    for (const auto & edit : edits)
    {
        if (edit.colorIndex >= reader_->colorCount())
        {
            return fail(makeDocumentError(sourcePath_, "Radarcol color index is outside the source table"));
        }
        if (edit.color > uomul::RadarColorConverter::colorMask)
        {
            return fail(makeDocumentError(sourcePath_, "Radarcol color must be a 15-bit RGB555 value"));
        }
    }

    bool startedEditor = false;
    if (!editor_)
    {
        auto staged = uomul::RadarColEditor::create(sourcePath_, sourcePath_, {.overwriteOutput = true});
        if (!staged)
        {
            return fail(staged.error());
        }
        editor_.emplace(std::move(*staged));
        outputPath_ = sourcePath_;
        startedEditor = true;
    }

    for (const auto & edit : edits)
    {
        const auto current = rawColor(edit.colorIndex);
        if (!current || *current == edit.color)
        {
            continue;
        }
        if (auto staged = editor_->editRawColor(edit.colorIndex, edit.color); !staged)
        {
            return fail(staged.error());
        }
        if (auto committed = reader_->readRawColor(edit.colorIndex); committed && *committed == edit.color)
        {
            stagedColors_.erase(edit.colorIndex);
        }
        else
        {
            stagedColors_.insert_or_assign(edit.colorIndex, edit.color);
        }
    }

    dirtyState_ = !stagedColors_.empty();
    if (!dirtyState_ && startedEditor)
    {
        editor_->abort();
        editor_.reset();
    }
    displayRevision_++;
    return {};
}

std::expected<void, uomul::Error> RadarcolDocument::revertColors(const std::vector<std::uint32_t> & indices)
{
    if (readOnly())
    {
        return fail(makeDocumentError(sourcePath_, "configured Radarcol source is read-only"));
    }
    if (!reader_)
    {
        return fail(makeDocumentError(sourcePath_, "Radarcol source is not available"));
    }

    for (const auto index : indices)
    {
        if (index >= reader_->colorCount())
        {
            return fail(makeDocumentError(sourcePath_, "Radarcol color index is outside the source table"));
        }
    }

    for (const auto index : indices)
    {
        if (!stagedColors_.contains(index))
        {
            continue;
        }
        const auto committed = reader_->readRawColor(index);
        if (!committed)
        {
            return fail(committed.error());
        }
        if (!editor_)
        {
            return fail(makeDocumentError(sourcePath_, "Radarcol editing has not started"));
        }
        if (auto reverted = editor_->editRawColor(index, *committed); !reverted)
        {
            return fail(reverted.error());
        }
        stagedColors_.erase(index);
    }

    dirtyState_ = !stagedColors_.empty();
    if (!dirtyState_ && editor_)
    {
        editor_->abort();
        editor_.reset();
    }
    displayRevision_++;
    return {};
}

std::expected<void, uomul::Error> RadarcolDocument::save()
{
    if (!editor_)
    {
        return fail(makeDocumentError(outputPath_, "Radarcol has no staged changes"));
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
        stagedColors_.clear();
        dirtyState_ = false;
        displayRevision_++;
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
        stagedColors_.clear();
        dirtyState_ = false;
        displayRevision_++;
        return fail(opened.error());
    }

    sourcePath_ = outputPath_;
    reader_ = std::move(*opened);
    editor_.reset();
    stagedColors_.clear();
    dirtyState_ = false;
    displayRevision_++;
    return {};
}

std::expected<void, uomul::Error> RadarcolDocument::discard()
{
    if (editor_)
    {
        editor_->abort();
        editor_.reset();
    }
    stagedColors_.clear();
    dirtyState_ = false;
    displayRevision_++;
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
