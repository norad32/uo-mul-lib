#pragma once

#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include <optional>
#include <string>

namespace uo_mule::gui
{

enum class CombineRejectionReason : std::uint8_t
{
    none,
    missingMapFacet,
    missingStaticsFacet,
    missingMapSource,
    missingStaticsSource,
    facetMismatch,
    dimensionsMismatch,
    unsupportedDocumentKind,
};

struct CombineRequest
{
    DocumentIdentity map;
    DocumentIdentity statics;
    std::string mapFacetKey;
    std::string staticsFacetKey;
    uomul::Dimensions mapDimensions{};
    uomul::Dimensions staticsDimensions{};
};

struct CombineResult
{
    bool accepted = false;
    CombineRejectionReason rejection = CombineRejectionReason::none;
    std::string diagnostic;
    DocumentIdentity combinedIdentity;
};

struct SplitDocumentMetadata
{
    DocumentKind kind = DocumentKind::map;
    DocumentIdentity source;
    std::string displayName;
    std::string facetKey;
    uomul::Dimensions dimensions{};
};

struct SplitMetadata
{
    SplitDocumentMetadata map;
    SplitDocumentMetadata statics;
};

struct SplitRequest
{
    DocumentIdentity combined;
    SplitMetadata metadata;
};

struct SplitResult
{
    bool accepted = false;
    std::string diagnostic;
    SplitMetadata metadata;
};

[[nodiscard]] CombineResult evaluateCombine(const CombineRequest & request);
[[nodiscard]] SplitResult evaluateSplit(const SplitRequest & request);

} // namespace uo_mule::gui
