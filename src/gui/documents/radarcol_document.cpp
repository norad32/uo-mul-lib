#include "gui/documents/radarcol_document.h"
#include "gui/documents/document_identity.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/radarcol/radarcol.h"
#include <expected>
#include <filesystem>
#include <utility>

namespace uo_mule::gui
{

std::expected<RadarcolDocument, uomul::Error> RadarcolDocument::open(DocumentId id,
                                                                      std::filesystem::path sourcePath)
{
    auto reader = uomul::RadarColReader::open(sourcePath);
    if (!reader)
    {
        return std::unexpected(reader.error());
    }

    return RadarcolDocument{.id = id, .sourcePath = std::move(sourcePath), .reader = std::move(*reader), .error = {}};
}

} // namespace uo_mule::gui
