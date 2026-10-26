#include "gui_model/documents/document_contracts.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("DocumentContracts_Should_ExposeInitialDocumentKinds")
{
    REQUIRE(uo_mule::gui::documentKindName(uo_mule::gui::DocumentKind::map) == "Map");
    REQUIRE(uo_mule::gui::documentKindName(uo_mule::gui::DocumentKind::radarcol) == "Radarcol");
    REQUIRE(uo_mule::gui::documentKindName(uo_mule::gui::DocumentKind::texmapPair) == "Texmap Pair");
}

TEST_CASE("DocumentContracts_Should_DistinguishMetadataOwnership")
{
    REQUIRE(uo_mule::gui::DocumentSourceKind::standalone != uo_mule::gui::DocumentSourceKind::configuredMetadata);
}

TEST_CASE("DocumentContracts_Should_RecordProfileFallbackAndOpenPolicies")
{
    REQUIRE(uo_mule::gui::document_contract::profileChangesRebindOpenDocuments);
    REQUIRE(uo_mule::gui::document_contract::mapEditsAreStaged);
    REQUIRE_FALSE(uo_mule::gui::document_contract::mapRequiresGlobalEditMode);
    REQUIRE_FALSE(uo_mule::gui::document_contract::genericOpenMutatesProfile);
    REQUIRE(uo_mule::gui::RadarcolSourcePolicy::embeddedDefault != uo_mule::gui::RadarcolSourcePolicy::externalConfigured);
    REQUIRE(uo_mule::gui::RadarcolSourcePolicy::embeddedDefaultWithWarning !=
            uo_mule::gui::RadarcolSourcePolicy::embeddedDefault);
    REQUIRE(uo_mule::gui::document_contract::unsupportedMulPolicy == uo_mule::gui::UnsupportedMulPolicy::rejectWithDiagnostic);
    REQUIRE(uo_mule::gui::document_contract::genericOpenLabel == "File > Open...");
}
