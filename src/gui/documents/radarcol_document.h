#pragma once

#include "gui/documents/document_identity.h"
#include "uomul/radarcol/radarcol.h"
#include <filesystem>
#include <expected>
#include <optional>
#include <string>

namespace uo_mule::gui
{

struct RadarcolDocument
{
    DocumentId id = 0;
    std::filesystem::path sourcePath;
    std::optional<uomul::RadarColReader> reader;
    std::string error;

    [[nodiscard]] static std::expected<RadarcolDocument, uomul::Error> open(DocumentId id,
                                                                              std::filesystem::path sourcePath);
};

} // namespace uo_mule::gui
