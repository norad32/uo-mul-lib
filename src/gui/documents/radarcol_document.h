#pragma once

#include "gui/documents/document_contracts.h"
#include "gui/documents/document_identity.h"
#include "uomul/radarcol/radarcol.h"
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

namespace uo_mule::gui
{

enum class RadarcolDocumentIntentKind : std::uint8_t
{
    beginEdit,
    editColor,
    save,
    discard,
    saveAs,
};

struct RadarcolDocumentIntent
{
    RadarcolDocumentIntentKind kind = RadarcolDocumentIntentKind::beginEdit;
    std::uint32_t colorIndex = 0;
    uomul::RadarColor color = 0;
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
    [[nodiscard]] const std::string & error() const noexcept { return error_; }
    [[nodiscard]] DocumentAccess access() const noexcept { return access_; }
    [[nodiscard]] bool readOnly() const noexcept { return access_ == DocumentAccess::readOnly; }
    [[nodiscard]] bool editing() const noexcept { return editor_.has_value(); }
    [[nodiscard]] bool dirty() const noexcept { return dirtyState_; }
    [[nodiscard]] bool overwriteOutput() const noexcept { return overwriteOutput_; }
    void setAccess(DocumentAccess value) noexcept;
    void setOutputPath(std::filesystem::path path) { outputPath_ = std::move(path); }
    void setOverwriteOutput(bool value) noexcept { overwriteOutput_ = value; }
    void requestIntent(RadarcolDocumentIntent intent) { pendingIntent_ = std::move(intent); }
    [[nodiscard]] std::expected<void, uomul::Error> processPendingIntent();
    [[nodiscard]] std::expected<void, uomul::Error> handleIntent(const RadarcolDocumentIntent & intent);

private:
    RadarcolDocument(DocumentId id,
                     std::filesystem::path sourcePath,
                     std::optional<uomul::RadarColReader> reader,
                     DocumentAccess access,
                     std::string error);
    [[nodiscard]] std::expected<void, uomul::Error> beginEditing(const std::filesystem::path & output, bool outputOverwrite);
    [[nodiscard]] std::expected<void, uomul::Error> editColor(std::uint32_t index, uomul::RadarColor color);
    [[nodiscard]] std::expected<void, uomul::Error> save();
    [[nodiscard]] std::expected<void, uomul::Error> discard();
    [[nodiscard]] std::expected<void, uomul::Error> saveAs(const std::filesystem::path & output, bool outputOverwrite);
    [[nodiscard]] std::expected<void, uomul::Error> fail(uomul::Error error);
    DocumentId id_ = 0;
    std::filesystem::path sourcePath_;
    std::filesystem::path outputPath_;
    std::optional<uomul::RadarColReader> reader_;
    std::optional<uomul::RadarColEditor> editor_;
    std::string error_;
    DocumentAccess access_ = DocumentAccess::editable;
    bool dirtyState_ = false;
    bool overwriteOutput_ = false;
    std::optional<RadarcolDocumentIntent> pendingIntent_;
};

} // namespace uo_mule::gui
