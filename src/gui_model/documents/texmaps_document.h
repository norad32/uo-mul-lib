#pragma once

#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/documents/staged_document_state.h"
#include "uomul/texmaps/texmaps.h"
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

namespace uo_mule::gui
{

enum class TexmapsDocumentIntentKind : std::uint8_t
{
    save,
    discard,
    saveAs,
};

struct TexmapsDocumentIntent
{
    TexmapsDocumentIntentKind kind = TexmapsDocumentIntentKind::save;
    std::filesystem::path outputIndexPath;
    std::filesystem::path outputDataPath;
    bool overwriteOutput = false;
};

struct TexmapsDocument
{
    [[nodiscard]] static std::expected<TexmapsDocument, uomul::Error> open(DocumentId id,
                                                                           const std::filesystem::path & indexPath,
                                                                           const std::filesystem::path & dataPath,
                                                                           DocumentAccess access = DocumentAccess::editable);
    [[nodiscard]] static TexmapsDocument invalid(DocumentId id,
                                                 std::filesystem::path indexPath,
                                                 std::filesystem::path dataPath,
                                                 DocumentAccess access,
                                                 std::string error);

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
    [[nodiscard]] DocumentAccess access() const noexcept { return state_.access(); }
    [[nodiscard]] bool readOnly() const noexcept { return state_.readOnly(); }
    [[nodiscard]] bool editing() const noexcept { return editor_.has_value(); }
    [[nodiscard]] bool dirty() const noexcept { return state_.dirty(); }
    [[nodiscard]] bool overwriteOutput() const noexcept { return overwriteOutput_; }

    void setAccess(DocumentAccess value) noexcept;
    void requestIntent(TexmapsDocumentIntent intent) { pendingIntent_.request(std::move(intent)); }
    [[nodiscard]] std::expected<void, uomul::Error> processPendingIntent();
    [[nodiscard]] std::expected<void, uomul::Error> handleIntent(const TexmapsDocumentIntent & intent);

private:
    TexmapsDocument(DocumentId id,
                    std::filesystem::path indexPath,
                    std::filesystem::path dataPath,
                    std::optional<uomul::TexMapsReader> reader,
                    DocumentAccess access,
                    std::string error);
    [[nodiscard]] std::expected<void, uomul::Error> beginEditing(const std::filesystem::path & outputIndexPath,
                                                                 const std::filesystem::path & outputDataPath,
                                                                 bool outputOverwrite);
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
    PendingDocumentIntent<TexmapsDocumentIntent> pendingIntent_;
};

} // namespace uo_mule::gui
