#pragma once

#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "uomul/tiledata/tiledata.h"
#include <expected>
#include <filesystem>
#include <optional>
#include <string>

namespace uo_mule::gui
{

struct TiledataDocument
{
    [[nodiscard]] static std::expected<TiledataDocument, uomul::Error>
    open(DocumentId id, const std::filesystem::path & sourcePath, uomul::TiledataLayout layout);
    [[nodiscard]] static TiledataDocument
    invalid(DocumentId id, std::filesystem::path sourcePath, uomul::TiledataLayout layout, std::string error);

    [[nodiscard]] DocumentId id() const noexcept { return id_; }
    [[nodiscard]] const std::filesystem::path & sourcePath() const noexcept { return sourcePath_; }
    [[nodiscard]] uomul::TiledataLayout layout() const noexcept { return layout_; }
    [[nodiscard]] const std::optional<uomul::TiledataReader> & reader() const noexcept { return reader_; }
    [[nodiscard]] const std::string & error() const noexcept { return error_; }
    [[nodiscard]] static constexpr bool readOnly() noexcept { return true; }
    [[nodiscard]] static constexpr bool dirty() noexcept { return false; }

private:
    TiledataDocument(DocumentId id,
                     std::filesystem::path sourcePath,
                     uomul::TiledataLayout layout,
                     std::optional<uomul::TiledataReader> reader,
                     std::string error);

    DocumentId id_ = 0;
    std::filesystem::path sourcePath_;
    uomul::TiledataLayout layout_ = uomul::TiledataLayout::extended64;
    std::optional<uomul::TiledataReader> reader_;
    std::string error_;
};

} // namespace uo_mule::gui
