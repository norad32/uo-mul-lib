#include "gui_model/documents/tiledata_document.h"
#include "gui_model/documents/document_identity.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/tiledata/tiledata.h"
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

namespace uo_mule::gui
{

TiledataDocument::TiledataDocument(DocumentId id,
                                   std::filesystem::path sourcePath,
                                   uomul::TiledataLayout layout,
                                   std::optional<uomul::TiledataReader> reader,
                                   std::string error) :
    id_(id), sourcePath_(std::move(sourcePath)), layout_(layout), reader_(std::move(reader)), error_(std::move(error))
{
}

std::expected<TiledataDocument, uomul::Error>
TiledataDocument::open(DocumentId id, const std::filesystem::path & sourcePath, uomul::TiledataLayout layout)
{
    auto reader = uomul::TiledataReader::open(sourcePath, layout);
    if (!reader)
    {
        return std::unexpected(reader.error());
    }
    return TiledataDocument{id, sourcePath, layout, std::move(*reader), {}};
}

TiledataDocument
TiledataDocument::invalid(DocumentId id, std::filesystem::path sourcePath, uomul::TiledataLayout layout, std::string error)
{
    return TiledataDocument{id, std::move(sourcePath), layout, std::nullopt, std::move(error)};
}

} // namespace uo_mule::gui
