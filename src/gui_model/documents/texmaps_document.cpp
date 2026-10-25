#include "gui_model/documents/texmaps_document.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/texmaps/texmaps.h"
#include <cstddef>
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
        case TexmapsDocumentIntentKind::replace :
        {
            const std::vector<TexmapsTextureEdit> edits =
                intent.textureEdits.empty()
                    ? std::vector<TexmapsTextureEdit>{{.textureId = intent.textureId, .texture = intent.texture}}
                    : intent.textureEdits;
            result = replaceTextures(edits);
            break;
        }
        case TexmapsDocumentIntentKind::clear :
        {
            const std::vector<std::uint32_t> textureIds =
                intent.textureIds.empty() ? std::vector<std::uint32_t>{intent.textureId} : intent.textureIds;
            result = clearTextures(textureIds);
            break;
        }
        case TexmapsDocumentIntentKind::revert :
        {
            const std::vector<std::uint32_t> textureIds =
                intent.textureIds.empty() ? std::vector<std::uint32_t>{intent.textureId} : intent.textureIds;
            result = revertTextures(textureIds);
            break;
        }
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
    if (readOnly())
    {
        if (editor_)
        {
            editor_->abort();
            editor_.reset();
        }
        stagedTextures_.clear();
        stagedEmptySlots_.clear();
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

std::expected<std::optional<uomul::TextureImage>, uomul::Error> TexmapsDocument::committedTexture(std::uint32_t textureId) const
{
    if (!reader_)
    {
        return std::unexpected(makeDocumentError(sourceIndexPath_, "Texmap Pair source is not available"));
    }
    if (textureId >= reader_->textureCount())
    {
        return std::optional<uomul::TextureImage>{};
    }

    auto populated = reader_->hasTexture(textureId);
    if (!populated)
    {
        return std::unexpected(populated.error());
    }
    if (!*populated)
    {
        return std::optional<uomul::TextureImage>{};
    }

    auto texture = reader_->readTexture(textureId);
    if (!texture)
    {
        return std::unexpected(texture.error());
    }
    return std::optional<uomul::TextureImage>{std::move(*texture)};
}

std::expected<std::optional<uomul::TextureImage>, uomul::Error> TexmapsDocument::effectiveTexture(std::uint32_t textureId) const
{
    if (auto valid = validateTextureId(textureId); !valid)
    {
        return std::unexpected(valid.error());
    }
    if (const auto staged = stagedTextures_.find(textureId); staged != stagedTextures_.end())
    {
        return std::optional<uomul::TextureImage>{staged->second};
    }
    if (stagedEmptySlots_.contains(textureId))
    {
        return std::optional<uomul::TextureImage>{};
    }
    return committedTexture(textureId);
}

std::expected<bool, uomul::Error> TexmapsDocument::hasTexture(std::uint32_t textureId) const
{
    auto texture = effectiveTexture(textureId);
    if (!texture)
    {
        return std::unexpected(texture.error());
    }
    return texture->has_value();
}

bool TexmapsDocument::hasStagedTexture(std::uint32_t textureId) const noexcept
{
    return stagedTextures_.contains(textureId) || stagedEmptySlots_.contains(textureId);
}

std::expected<void, uomul::Error> TexmapsDocument::validateTextureId(std::uint32_t textureId) const
{
    if (textureId >= uomul::textureCount)
    {
        return std::unexpected(makeDocumentError(sourceIndexPath_, "texture ID is outside the supported Texmap Pair range"));
    }
    return {};
}

std::expected<void, uomul::Error> TexmapsDocument::validateTexture(const uomul::TextureImage & texture) const
{
    std::size_t expectedPixels = 0;
    if (texture.dimension == uomul::textureSmallDimension)
    {
        expectedPixels = uomul::textureSmallPixelCount;
    }
    else if (texture.dimension == uomul::textureLargeDimension)
    {
        expectedPixels = uomul::textureLargePixelCount;
    }
    else
    {
        return std::unexpected(makeDocumentError(sourceDataPath_, "texture dimension must be 64 or 128 pixels"));
    }

    if (texture.pixels.size() != expectedPixels)
    {
        return std::unexpected(makeDocumentError(sourceDataPath_, "texture pixel count does not match its dimension"));
    }
    return {};
}

std::expected<void, uomul::Error> TexmapsDocument::ensureEditing()
{
    if (editor_)
    {
        return {};
    }
    return beginEditing(sourceIndexPath_, sourceDataPath_, true);
}

uomul::TexMapsEditor * TexmapsDocument::activeEditor() noexcept
{
    return editor_ ? &*editor_ : nullptr;
}

void TexmapsDocument::updateDirtyState() noexcept
{
    state_.setDirty(!stagedTextures_.empty() || !stagedEmptySlots_.empty());
}

void TexmapsDocument::abortEditorIfClean() noexcept
{
    if (!state_.dirty() && editor_)
    {
        editor_->abort();
        editor_.reset();
    }
}

void TexmapsDocument::finishMutation() noexcept
{
    updateDirtyState();
    abortEditorIfClean();
    state_.touch();
}

std::expected<void, uomul::Error> TexmapsDocument::validateReplacementEdits(const std::vector<TexmapsTextureEdit> & edits) const
{
    for (const auto & edit : edits)
    {
        if (auto valid = validateTextureId(edit.textureId); !valid)
        {
            return std::unexpected(valid.error());
        }
        if (auto valid = validateTexture(edit.texture); !valid)
        {
            return std::unexpected(valid.error());
        }
    }
    return {};
}

std::expected<void, uomul::Error> TexmapsDocument::replaceTexture(const TexmapsTextureEdit & edit)
{
    auto current = effectiveTexture(edit.textureId);
    if (!current)
    {
        return std::unexpected(current.error());
    }
    auto committed = committedTexture(edit.textureId);
    if (!committed)
    {
        return std::unexpected(committed.error());
    }
    if (current->has_value() && current->value() == edit.texture)
    {
        return {};
    }

    if (auto started = ensureEditing(); !started)
    {
        return started;
    }
    auto * editor = activeEditor();
    if (editor == nullptr)
    {
        return std::unexpected(makeDocumentError(sourceIndexPath_, "Texmap Pair editor is not available"));
    }
    if (auto staged = editor->editTexture(edit.textureId, edit.texture); !staged)
    {
        return std::unexpected(staged.error());
    }

    if (committed->has_value() && committed->value() == edit.texture)
    {
        stagedTextures_.erase(edit.textureId);
        stagedEmptySlots_.erase(edit.textureId);
    }
    else
    {
        stagedTextures_.insert_or_assign(edit.textureId, edit.texture);
        stagedEmptySlots_.erase(edit.textureId);
    }
    updateDirtyState();
    return {};
}

std::expected<void, uomul::Error> TexmapsDocument::replaceTextures(const std::vector<TexmapsTextureEdit> & edits)
{
    if (readOnly())
    {
        return fail(makeDocumentError(sourceIndexPath_, "configured Texmap Pair source is read-only"));
    }
    if (!reader_)
    {
        return fail(makeDocumentError(sourceIndexPath_, "Texmap Pair source is not available"));
    }
    if (edits.empty())
    {
        return fail(makeDocumentError(sourceIndexPath_, "Texmap replacement cannot be empty"));
    }
    if (auto valid = validateReplacementEdits(edits); !valid)
    {
        return fail(valid.error());
    }

    for (const auto & edit : edits)
    {
        if (auto replaced = replaceTexture(edit); !replaced)
        {
            updateDirtyState();
            abortEditorIfClean();
            return fail(replaced.error());
        }
    }

    finishMutation();
    return {};
}

std::expected<void, uomul::Error> TexmapsDocument::clearTextures(const std::vector<std::uint32_t> & textureIds)
{
    if (readOnly())
    {
        return fail(makeDocumentError(sourceIndexPath_, "configured Texmap Pair source is read-only"));
    }
    if (!reader_)
    {
        return fail(makeDocumentError(sourceIndexPath_, "Texmap Pair source is not available"));
    }
    for (const auto textureId : textureIds)
    {
        if (auto valid = validateTextureId(textureId); !valid)
        {
            return fail(valid.error());
        }
    }

    for (const auto textureId : textureIds)
    {
        auto current = effectiveTexture(textureId);
        if (!current)
        {
            return fail(current.error());
        }
        if (!current->has_value())
        {
            continue;
        }
        auto committed = committedTexture(textureId);
        if (!committed)
        {
            return fail(committed.error());
        }

        if (auto started = ensureEditing(); !started)
        {
            return started;
        }
        auto * editor = activeEditor();
        if (editor == nullptr)
        {
            return fail(makeDocumentError(sourceIndexPath_, "Texmap Pair editor is not available"));
        }
        if (auto staged = editor->clearTexture(textureId); !staged)
        {
            updateDirtyState();
            abortEditorIfClean();
            return fail(staged.error());
        }

        stagedTextures_.erase(textureId);
        if (committed->has_value())
        {
            stagedEmptySlots_.insert(textureId);
        }
        else
        {
            stagedEmptySlots_.erase(textureId);
        }
        updateDirtyState();
    }

    finishMutation();
    return {};
}

std::expected<void, uomul::Error> TexmapsDocument::revertTextures(const std::vector<std::uint32_t> & textureIds)
{
    if (readOnly())
    {
        return fail(makeDocumentError(sourceIndexPath_, "configured Texmap Pair source is read-only"));
    }
    if (!reader_)
    {
        return fail(makeDocumentError(sourceIndexPath_, "Texmap Pair source is not available"));
    }
    for (const auto textureId : textureIds)
    {
        if (auto valid = validateTextureId(textureId); !valid)
        {
            return fail(valid.error());
        }
    }

    for (const auto textureId : textureIds)
    {
        if (!hasStagedTexture(textureId))
        {
            continue;
        }
        auto committed = committedTexture(textureId);
        if (!committed)
        {
            return fail(committed.error());
        }
        if (auto started = ensureEditing(); !started)
        {
            return started;
        }

        auto * editor = activeEditor();
        if (editor == nullptr)
        {
            return fail(makeDocumentError(sourceIndexPath_, "Texmap Pair editor is not available"));
        }
        std::expected<void, uomul::Error> staged;
        if (committed->has_value())
        {
            staged = editor->editTexture(textureId, committed->value());
        }
        else
        {
            staged = editor->clearTexture(textureId);
        }
        if (!staged)
        {
            updateDirtyState();
            abortEditorIfClean();
            return fail(staged.error());
        }
        stagedTextures_.erase(textureId);
        stagedEmptySlots_.erase(textureId);
        updateDirtyState();
    }

    finishMutation();
    return {};
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
    stagedTextures_.clear();
    stagedEmptySlots_.clear();
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
    stagedTextures_.clear();
    stagedEmptySlots_.clear();
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
