#include "gui_model/documents/radarcol_document.h"
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
                                   std::string error) :
    id_(id), sourcePath_(std::move(sourcePath)), outputPath_(sourcePath_), reader_(std::move(reader))
{
    state_.setError(std::move(error));
}

std::expected<RadarcolDocument, uomul::Error> RadarcolDocument::open(DocumentId id, const std::filesystem::path & sourcePath)
{
    auto reader = uomul::RadarColReader::open(sourcePath);
    if (!reader)
    {
        return std::unexpected(reader.error());
    }

    return RadarcolDocument{id, sourcePath, std::move(*reader), {}};
}

RadarcolDocument RadarcolDocument::invalid(DocumentId id, std::filesystem::path sourcePath, std::string error)
{
    return RadarcolDocument{id, std::move(sourcePath), std::nullopt, std::move(error)};
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
        state_.setError(result.error().message);
    }
    else
    {
        state_.clearError();
    }
    return result;
}

std::expected<void, uomul::Error> RadarcolDocument::processPendingIntent()
{
    const auto intent = pendingIntent_.take();
    if (!intent)
    {
        return {};
    }
    return handleIntent(*intent);
}

std::expected<void, uomul::Error> RadarcolDocument::beginEditing(const std::filesystem::path & output, bool outputOverwrite)
{
    if (reader_ == std::nullopt)
    {
        return fail(makeDocumentError(sourcePath_, "Radarcol file is not available"));
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
    state_.setDirty(false);
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
    if (!reader_)
    {
        return fail(makeDocumentError(sourcePath_, "Radarcol file is not available"));
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

    state_.setDirty(!stagedColors_.empty());
    if (!state_.dirty() && startedEditor)
    {
        editor_->abort();
        editor_.reset();
    }
    state_.touch();
    return {};
}

std::expected<void, uomul::Error> RadarcolDocument::revertColors(const std::vector<std::uint32_t> & indices)
{
    if (!reader_)
    {
        return fail(makeDocumentError(sourcePath_, "Radarcol file is not available"));
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

    state_.setDirty(!stagedColors_.empty());
    if (!state_.dirty() && editor_)
    {
        editor_->abort();
        editor_.reset();
    }
    state_.touch();
    return {};
}

std::expected<void, uomul::Error> RadarcolDocument::save()
{
    if (!editor_ && !stagedColors_.empty())
    {
        auto staged = uomul::RadarColEditor::create(sourcePath_, outputPath_, {.overwriteOutput = overwriteOutput_});
        if (!staged)
        {
            return fail(staged.error());
        }
        editor_.emplace(std::move(*staged));
        for (const auto & [index, color] : stagedColors_)
        {
            if (auto edited = editor_->editRawColor(index, color); !edited)
            {
                editor_->abort();
                editor_.reset();
                return fail(edited.error());
            }
        }
    }

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
        // The underlying editor has already closed or aborted its transaction
        // on commit failure. Keep the logical changes so the user can retry or
        // discard them explicitly.
        editor_.reset();
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
        return fail(opened.error());
    }

    sourcePath_ = outputPath_;
    reader_ = std::move(*opened);
    editor_.reset();
    stagedColors_.clear();
    state_.resetChanges();
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
    state_.resetChanges();
    return {};
}

std::expected<void, uomul::Error> RadarcolDocument::saveAs(const std::filesystem::path & output, bool outputOverwrite)
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
    if (auto started = beginEditing(output, outputOverwrite); !started)
    {
        return started;
    }
    return save();
}

std::expected<void, uomul::Error> RadarcolDocument::fail(uomul::Error documentError)
{
    state_.setError(documentError.message);
    return std::unexpected(std::move(documentError));
}

} // namespace uo_mule::gui
