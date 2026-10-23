#pragma once

#include "gui_model/documents/document_contracts.h"
#include "gui_model/source/source_identity.h"
#include "uomul/map.h"
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>

namespace uo_mule::gui
{

struct DataProfile;

using DocumentId = std::uint64_t;

using DocumentSourceIdentity = SourceIdentity;

[[nodiscard]] inline std::filesystem::path canonicalDocumentPath(const std::filesystem::path & path)
{
    return canonicalSourcePath(path);
}
[[nodiscard]] inline DocumentSourceIdentity makeDocumentSourceIdentity(std::span<const std::filesystem::path> paths)
{
    return makeSourceIdentity(paths);
}
[[nodiscard]] inline DocumentSourceIdentity makeDocumentSourceIdentity(std::initializer_list<std::filesystem::path> paths)
{
    return makeSourceIdentity(paths);
}
[[nodiscard]] inline DocumentSourceIdentity makeRoleAwareDocumentSourceIdentity(std::span<const SourcePath> sources)
{
    return makeRoleAwareSourceIdentity(sources);
}
[[nodiscard]] inline DocumentSourceIdentity makeRoleAwareDocumentSourceIdentity(std::initializer_list<SourcePath> sources)
{
    return makeRoleAwareSourceIdentity(sources);
}
[[nodiscard]] std::optional<DocumentSourceIdentity> configuredDocumentSourceIdentity(const DataProfile & profile,
                                                                                     DocumentKind kind);

struct DocumentIdentity
{
    DocumentId id = 0;
    DocumentKind kind = DocumentKind::map;
    DocumentSourceIdentity source;
};

[[nodiscard]] bool sameDocumentIdentity(const DocumentIdentity & first, const DocumentIdentity & second) noexcept;

enum class DocumentValidationState : std::uint8_t
{
    unknown,
    valid,
    invalid,
};

struct DocumentSummary
{
    DocumentIdentity identity;
    std::string displayName;
    DocumentSourceKind sourceKind = DocumentSourceKind::standalone;
    DocumentAccess access = DocumentAccess::editable;
    DocumentValidationState validation = DocumentValidationState::unknown;
    std::optional<std::string> facetKey;
    std::optional<uomul::Dimensions> dimensions;
    bool dirty = false;
};

} // namespace uo_mule::gui
