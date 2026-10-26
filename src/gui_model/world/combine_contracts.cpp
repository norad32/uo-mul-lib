#include "gui_model/world/combine_contracts.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/source/source_identity.h"
#include <string>
#include <utility>

namespace uo_mule::gui
{
namespace
{

[[nodiscard]] CombineResult rejected(CombineRejectionReason reason, std::string diagnostic)
{
    CombineResult result;
    result.rejection = reason;
    result.diagnostic = std::move(diagnostic);
    return result;
}

} // namespace

CombineResult evaluateCombine(const CombineRequest & request)
{
    if (request.map.kind != DocumentKind::map || request.statics.kind != DocumentKind::statics)
    {
        return rejected(CombineRejectionReason::unsupportedDocumentKind, "Combine requires a Map and a Statics document");
    }
    if (request.mapFacetKey.empty())
    {
        return rejected(CombineRejectionReason::missingMapFacet, "Map facet key is missing");
    }
    if (request.staticsFacetKey.empty())
    {
        return rejected(CombineRejectionReason::missingStaticsFacet, "Statics facet key is missing");
    }
    if (request.map.source.path(SourceRole::primary) == nullptr)
    {
        return rejected(CombineRejectionReason::missingMapSource, "Map file is not available");
    }
    if (request.statics.source.path(SourceRole::primary) == nullptr)
    {
        return rejected(CombineRejectionReason::missingStaticsSource, "Statics file is not available");
    }
    if (request.mapFacetKey != request.staticsFacetKey)
    {
        return rejected(CombineRejectionReason::facetMismatch, "Map and Statics facet keys must match exactly");
    }
    if (request.mapDimensions != request.staticsDimensions)
    {
        return rejected(CombineRejectionReason::dimensionsMismatch, "Map and Statics dimensions must match exactly");
    }

    DocumentIdentity combined;
    combined.kind = DocumentKind::combined;
    combined.source = makeRoleAwareDocumentSourceIdentity({
        {.role = SourceRole::primary, .path = *request.map.source.path(SourceRole::primary)},
        {.role = SourceRole::companion, .path = *request.statics.source.path(SourceRole::primary)},
    });
    return {.accepted = true, .rejection = CombineRejectionReason::none, .diagnostic = {}, .combinedIdentity = combined};
}

SplitResult evaluateSplit(const SplitRequest & request)
{
    if (request.combined.kind != DocumentKind::combined)
    {
        return {.accepted = false, .diagnostic = "Split requires a Combined document", .metadata = {}};
    }
    if (request.metadata.map.kind != DocumentKind::map || request.metadata.statics.kind != DocumentKind::statics)
    {
        return {.accepted = false, .diagnostic = "Split metadata must contain Map and Statics documents", .metadata = {}};
    }
    if (request.metadata.map.facetKey.empty() || request.metadata.statics.facetKey.empty() ||
        request.metadata.map.facetKey != request.metadata.statics.facetKey ||
        request.metadata.map.dimensions != request.metadata.statics.dimensions)
    {
        return {.accepted = false, .diagnostic = "Split metadata has incompatible facet metadata", .metadata = {}};
    }
    return {.accepted = true, .diagnostic = {}, .metadata = request.metadata};
}

} // namespace uo_mule::gui
