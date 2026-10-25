#pragma once

#include "gui_model/documents/document_identity.h"
#include "gui_model/documents/staged_document_state.h"
#include "uomul/texmaps/texmaps.h"
#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace uo_mule::gui
{

enum class TexmapsDocumentIntentKind : std::uint8_t
{
    replace,
    clear,
    revert,
    save,
    discard,
    saveAs,
};

struct TexmapsTextureEdit
{
    std::uint32_t textureId = 0;
    uomul::TextureImage texture;
};

struct TexmapsDocumentIntent
{
    TexmapsDocumentIntentKind kind = TexmapsDocumentIntentKind::save;
    std::filesystem::path outputIndexPath;
    std::filesystem::path outputDataPath;
    bool overwriteOutput = false;
    std::uint32_t textureId = 0;
    uomul::TextureImage texture;
    std::vector<TexmapsTextureEdit> textureEdits;
    std::vector<std::uint32_t> textureIds;
};

struct TexmapsDocument
{
    [[nodiscard]] static std::expected<TexmapsDocument, uomul::Error>
    open(DocumentId id, const std::filesystem::path & indexPath, const std::filesystem::path & dataPath);
    [[nodiscard]] static TexmapsDocument
    invalid(DocumentId id, std::filesystem::path indexPath, std::filesystem::path dataPath, std::string error);

    [[nodiscard]] DocumentId id() const noexcept { return id_; }
    [[nodiscard]] const std::filesystem::path & sourceIndexPath() const noexcept { return sourceIndexPath_; }
    [[nodiscard]] const std::filesystem::path & sourceDataPath() const noexcept { return sourceDataPath_; }
    [[nodiscard]] const std::filesystem::path & outputIndexPath() const noexcept { return outputIndexPath_; }
    [[nodiscard]] const std::filesystem::path & outputDataPath() const noexcept { return outputDataPath_; }
    [[nodiscard]] DocumentSourceIdentity sourceIdentity() const
    {
        return makeRoleAwareDocumentSourceIdentity({
            {.role = SourceRole::index, .path = sourceIndexPath_},
            {.role = SourceRole::data, .path = sourceDataPath_},
        });
    }
    [[nodiscard]] const std::optional<uomul::TexMapsReader> & reader() const noexcept { return reader_; }
    [[nodiscard]] std::uint64_t displayRevision() const noexcept { return state_.revision(); }
    [[nodiscard]] const std::string & error() const noexcept { return state_.error(); }
    [[nodiscard]] bool editing() const noexcept { return editor_.has_value(); }
    [[nodiscard]] bool dirty() const noexcept { return state_.dirty(); }
    [[nodiscard]] bool overwriteOutput() const noexcept { return overwriteOutput_; }
    [[nodiscard]] std::expected<std::optional<uomul::TextureImage>, uomul::Error>
    effectiveTexture(std::uint32_t textureId) const;
    [[nodiscard]] std::expected<bool, uomul::Error> hasTexture(std::uint32_t textureId) const;
    [[nodiscard]] bool hasStagedTexture(std::uint32_t textureId) const noexcept;

    void requestIntent(TexmapsDocumentIntent intent) { pendingIntent_.request(std::move(intent)); }
    [[nodiscard]] std::expected<void, uomul::Error> processPendingIntent();
    [[nodiscard]] std::expected<void, uomul::Error> handleIntent(const TexmapsDocumentIntent & intent);

private:
    TexmapsDocument(DocumentId id,
                    std::filesystem::path indexPath,
                    std::filesystem::path dataPath,
                    std::optional<uomul::TexMapsReader> reader,
                    std::string error);
    [[nodiscard]] std::expected<void, uomul::Error> beginEditing(const std::filesystem::path & outputIndexPath,
                                                                 const std::filesystem::path & outputDataPath,
                                                                 bool outputOverwrite);
    [[nodiscard]] std::expected<void, uomul::Error> ensureEditing();
    [[nodiscard]] uomul::TexMapsEditor * activeEditor() noexcept;
    [[nodiscard]] std::expected<std::optional<uomul::TextureImage>, uomul::Error>
    committedTexture(std::uint32_t textureId) const;
    [[nodiscard]] std::expected<void, uomul::Error> replaceTextures(const std::vector<TexmapsTextureEdit> & edits);
    [[nodiscard]] std::expected<void, uomul::Error>
    validateReplacementEdits(const std::vector<TexmapsTextureEdit> & edits) const;
    [[nodiscard]] std::expected<void, uomul::Error> replaceTexture(const TexmapsTextureEdit & edit);
    [[nodiscard]] std::expected<void, uomul::Error> clearTextures(const std::vector<std::uint32_t> & textureIds);
    [[nodiscard]] std::expected<void, uomul::Error> revertTextures(const std::vector<std::uint32_t> & textureIds);
    [[nodiscard]] std::expected<void, uomul::Error> validateTextureId(std::uint32_t textureId) const;
    [[nodiscard]] std::expected<void, uomul::Error> validateTexture(const uomul::TextureImage & texture) const;
    void updateDirtyState() noexcept;
    void abortEditorIfClean() noexcept;
    void finishMutation() noexcept;
    [[nodiscard]] std::expected<void, uomul::Error> save();
    [[nodiscard]] std::expected<void, uomul::Error> discard();
    [[nodiscard]] std::expected<void, uomul::Error>
    saveAs(const std::filesystem::path & outputIndexPath, const std::filesystem::path & outputDataPath, bool outputOverwrite);
    [[nodiscard]] std::expected<void, uomul::Error> fail(uomul::Error error);

    DocumentId id_ = 0;
    std::filesystem::path sourceIndexPath_;
    std::filesystem::path sourceDataPath_;
    std::filesystem::path outputIndexPath_;
    std::filesystem::path outputDataPath_;
    std::optional<uomul::TexMapsReader> reader_;
    std::optional<uomul::TexMapsEditor> editor_;
    StagedDocumentState state_;
    bool overwriteOutput_ = false;
    std::map<std::uint32_t, uomul::TextureImage> stagedTextures_;
    std::set<std::uint32_t> stagedEmptySlots_;
    PendingDocumentIntent<TexmapsDocumentIntent> pendingIntent_;
};

} // namespace uo_mule::gui
