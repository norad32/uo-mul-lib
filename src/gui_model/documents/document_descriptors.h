#pragma once

#include "gui_model/documents/document_identity.h"
#include <expected>
#include <string>

namespace uo_mule::gui
{

struct DocumentCreationRequest
{
    DocumentKind kind = DocumentKind::unavailable;
    DocumentSourceIdentity source;
    std::string displayName;
    DocumentAccess access = DocumentAccess::readOnly;
    DocumentValidationState validation = DocumentValidationState::unknown;
    std::optional<std::string> facetKey;
    std::optional<uomul::Dimensions> dimensions;
};

enum class DocumentCreationError : std::uint8_t
{
    unavailableKind,
    missingSource,
    unsupportedSourceShape,
};

struct DocumentDescriptor
{
    DocumentKind kind = DocumentKind::unavailable;
    std::string stableId;
    std::vector<SourceRole> requiredRoles;
};

class DocumentDescriptorRegistry
{
public:
    DocumentDescriptorRegistry();

    [[nodiscard]] const DocumentDescriptor * find(DocumentKind kind) const noexcept;
    [[nodiscard]] std::expected<DocumentSummary, DocumentCreationError>
    createSummary(const DocumentCreationRequest & request) const;

private:
    std::vector<DocumentDescriptor> descriptors_;
};

} // namespace uo_mule::gui
