#include "gui_model/documents/document_descriptors.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/source/source_identity.h"
#include <algorithm>
#include <expected>

namespace uo_mule::gui
{

DocumentDescriptorRegistry::DocumentDescriptorRegistry() :
    descriptors_{
        {.kind = DocumentKind::map, .stableId = "map", .requiredRoles = {SourceRole::primary}},
        {.kind = DocumentKind::radarcol, .stableId = "radarcol", .requiredRoles = {SourceRole::primary}},
        {.kind = DocumentKind::texmaps, .stableId = "texmaps_set", .requiredRoles = {SourceRole::index, SourceRole::data}},
        {.kind = DocumentKind::tiledata, .stableId = "tiledata", .requiredRoles = {SourceRole::primary}},
    }
{
}

const DocumentDescriptor * DocumentDescriptorRegistry::find(DocumentKind kind) const noexcept
{
    const auto descriptor = std::ranges::find(descriptors_, kind, &DocumentDescriptor::kind);
    return descriptor == descriptors_.end() ? nullptr : &*descriptor;
}

std::expected<DocumentSummary, DocumentCreationError>
DocumentDescriptorRegistry::createSummary(const DocumentCreationRequest & request) const
{
    const auto * descriptor = find(request.kind);
    if (descriptor == nullptr)
    {
        return std::unexpected(DocumentCreationError::unavailableKind);
    }
    for (const auto role : descriptor->requiredRoles)
    {
        if (request.source.path(role) == nullptr)
        {
            return std::unexpected(DocumentCreationError::missingSource);
        }
    }
    return DocumentSummary{
        .identity = {.id = 0, .kind = request.kind, .source = request.source},
        .displayName = request.displayName,
        .sourceKind = DocumentSourceKind::standalone,
        .validation = request.validation,
        .facetKey = request.facetKey,
        .dimensions = request.dimensions,
        .dirty = false,
    };
}

} // namespace uo_mule::gui
