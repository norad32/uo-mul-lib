#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_descriptors.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/source/source_identity.h"
#include <catch2/catch_test_macros.hpp>
#include <optional>

using uo_mule::gui::DocumentCreationError;
using uo_mule::gui::DocumentDescriptorRegistry;
using uo_mule::gui::DocumentKind;
using uo_mule::gui::DocumentValidationState;
using uo_mule::gui::makeDocumentSourceIdentity;
using uo_mule::gui::SourceRole;

TEST_CASE("documentDescriptors_Should_CreateValidRadarcolSummary")
{
    const DocumentDescriptorRegistry registry;
    const auto summary = registry.createSummary({
        .kind = DocumentKind::radarcol,
        .source = makeDocumentSourceIdentity({"radarcol.mul"}),
        .displayName = "radarcol.mul",
        .validation = DocumentValidationState::valid,
        .facetKey = std::nullopt,
        .dimensions = std::nullopt,
    });

    REQUIRE(summary.has_value());
    REQUIRE(summary->identity.kind == DocumentKind::radarcol);
    REQUIRE(summary->identity.source.path(SourceRole::primary) != nullptr);
}

TEST_CASE("documentDescriptors_Should_RejectIncompletetexmaps")
{
    const DocumentDescriptorRegistry registry;
    const auto summary = registry.createSummary({
        .kind = DocumentKind::texmaps,
        .source = makeDocumentSourceIdentity({"texidx.mul"}),
        .displayName = "texidx.mul",
        .facetKey = std::nullopt,
        .dimensions = std::nullopt,
    });

    REQUIRE_FALSE(summary.has_value());
    REQUIRE(summary.error() == DocumentCreationError::missingSource);
}

TEST_CASE("documentDescriptors_Should_RejectUnavailableKind")
{
    const DocumentDescriptorRegistry registry;
    const auto summary = registry.createSummary({
        .kind = DocumentKind::statics,
        .source = {},
        .displayName = {},
        .validation = DocumentValidationState::unknown,
        .facetKey = std::nullopt,
        .dimensions = std::nullopt,
    });

    REQUIRE_FALSE(summary.has_value());
    REQUIRE(summary.error() == DocumentCreationError::unavailableKind);
}
