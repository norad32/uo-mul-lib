#pragma once

#include "gui/metadata/document_contracts.h"
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace uo_mule::gui
{

struct DataProfile;

using DocumentId = std::uint64_t;

struct DocumentSourceIdentity
{
    std::vector<std::filesystem::path> canonicalPaths;

    bool operator==(const DocumentSourceIdentity &) const = default;
};

[[nodiscard]] std::filesystem::path canonicalDocumentPath(const std::filesystem::path & path);
[[nodiscard]] DocumentSourceIdentity makeDocumentSourceIdentity(std::span<const std::filesystem::path> paths);
[[nodiscard]] DocumentSourceIdentity makeDocumentSourceIdentity(std::initializer_list<std::filesystem::path> paths);
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
    bool dirty = false;
};

} // namespace uo_mule::gui
