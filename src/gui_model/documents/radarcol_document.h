#pragma once

#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/documents/staged_document_state.h"
#include "uomul/radarcol/radarcol.h"
#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace uo_mule::gui
{

enum class RadarcolDocumentIntentKind : std::uint8_t
{
    editColors,
    revertColors,
    save,
    discard,
    saveAs,
};

struct RadarcolColorEdit
{
    std::uint32_t colorIndex = 0;
    uomul::RadarColor color = 0;
};

struct RadarcolDocumentIntent
{
    RadarcolDocumentIntentKind kind = RadarcolDocumentIntentKind::editColors;
    std::vector<RadarcolColorEdit> colorEdits;
    std::filesystem::path outputPath;
    bool overwriteOutput = false;
};

struct RadarcolDocument
{
    [[nodiscard]] static std::expected<RadarcolDocument, uomul::Error>
    open(DocumentId id, const std::filesystem::path & sourcePath, DocumentAccess access = DocumentAccess::editable);
    [[nodiscard]] static RadarcolDocument
    invalid(DocumentId id, std::filesystem::path sourcePath, DocumentAccess access, std::string error);
    [[nodiscard]] DocumentId id() const noexcept { return id_; }
    [[nodiscard]] const std::filesystem::path & sourcePath() const noexcept { return sourcePath_; }
    [[nodiscard]] const std::filesystem::path & outputPath() const noexcept { return outputPath_; }
    [[nodiscard]] const std::optional<uomul::RadarColReader> & reader() const noexcept { return reader_; }
    [[nodiscard]] std::optional<uomul::RadarColor> rawColor(std::uint32_t index) const;
    [[nodiscard]] bool hasStagedColor(std::uint32_t index) const noexcept;
    [[nodiscard]] std::uint64_t displayRevision() const noexcept { return state_.revision(); }
    [[nodiscard]] const std::string & error() const noexcept { return state_.error(); }
    [[nodiscard]] DocumentAccess access() const noexcept { return state_.access(); }
    [[nodiscard]] bool readOnly() const noexcept { return state_.readOnly(); }
    [[nodiscard]] bool editing() const noexcept { return editor_.has_value(); }
    [[nodiscard]] bool dirty() const noexcept { return state_.dirty(); }
    [[nodiscard]] bool overwriteOutput() const noexcept { return overwriteOutput_; }
    void setAccess(DocumentAccess value) noexcept;
    void setOutputPath(std::filesystem::path path) { outputPath_ = std::move(path); }
    void setOverwriteOutput(bool value) noexcept { overwriteOutput_ = value; }
    void requestIntent(RadarcolDocumentIntent intent) { pendingIntent_.request(std::move(intent)); }
    [[nodiscard]] std::expected<void, uomul::Error> processPendingIntent();
    [[nodiscard]] std::expected<void, uomul::Error> handleIntent(const RadarcolDocumentIntent & intent);

private:
    RadarcolDocument(DocumentId id,
                     std::filesystem::path sourcePath,
                     std::optional<uomul::RadarColReader> reader,
                     DocumentAccess access,
                     std::string error);
    [[nodiscard]] std::expected<void, uomul::Error> beginEditing(const std::filesystem::path & output, bool outputOverwrite);
    [[nodiscard]] std::expected<void, uomul::Error> editColors(const std::vector<RadarcolColorEdit> & edits);
    [[nodiscard]] std::expected<void, uomul::Error> revertColors(const std::vector<std::uint32_t> & indices);
    [[nodiscard]] std::expected<void, uomul::Error> save();
    [[nodiscard]] std::expected<void, uomul::Error> discard();
    [[nodiscard]] std::expected<void, uomul::Error> saveAs(const std::filesystem::path & output, bool outputOverwrite);
    [[nodiscard]] std::expected<void, uomul::Error> fail(uomul::Error error);
    DocumentId id_ = 0;
    std::filesystem::path sourcePath_;
    std::filesystem::path outputPath_;
    std::optional<uomul::RadarColReader> reader_;
    std::optional<uomul::RadarColEditor> editor_;
    StagedDocumentState state_;
    bool overwriteOutput_ = false;
    std::map<std::uint32_t, uomul::RadarColor> stagedColors_;
    PendingDocumentIntent<RadarcolDocumentIntent> pendingIntent_;
};

} // namespace uo_mule::gui
