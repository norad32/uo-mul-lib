#include "gui_model/documents/document_identity.h"
#include "gui_model/lifecycle/document_lifecycle.h"
#include <catch2/catch_test_macros.hpp>

using uo_mule::gui::DocumentCloseDirtyPolicy;
using uo_mule::gui::DocumentLifecycleController;

constexpr uo_mule::gui::DocumentId firstActivation = 7;
constexpr uo_mule::gui::DocumentId secondActivation = 9;
constexpr uo_mule::gui::DocumentId firstClose = 11;
constexpr uo_mule::gui::DocumentId secondClose = 12;
constexpr uo_mule::gui::DocumentId independentActivation = 3;
constexpr uo_mule::gui::DocumentId independentClose = 4;
constexpr uo_mule::gui::DocumentId firstDirtyClose = 21;
constexpr uo_mule::gui::DocumentId secondDirtyClose = 22;

TEST_CASE("documentLifecycle_Should_DeferActivationAndApplyFirstRequestOnly")
{
    DocumentLifecycleController lifecycle;
    lifecycle.requestActivation(firstActivation);
    lifecycle.requestActivation(secondActivation);

    REQUIRE(lifecycle.hasPendingActivation());
    REQUIRE(lifecycle.takeActivation() == firstActivation);
    REQUIRE_FALSE(lifecycle.hasPendingActivation());
    REQUIRE_FALSE(lifecycle.takeActivation().has_value());
}

TEST_CASE("documentLifecycle_Should_DeferCloseAndApplyFirstRequestOnly")
{
    DocumentLifecycleController lifecycle;
    lifecycle.requestClose({.id = firstClose, .dirtyPolicy = DocumentCloseDirtyPolicy::prompt});
    lifecycle.requestClose({.id = secondClose, .dirtyPolicy = DocumentCloseDirtyPolicy::prompt});

    REQUIRE(lifecycle.hasPendingClose());
    const auto request = lifecycle.takeClose();
    REQUIRE(request);
    const auto closeRequest = request.value_or(uo_mule::gui::DocumentCloseRequest{});
    REQUIRE(closeRequest.id == firstClose);
    REQUIRE(closeRequest.dirtyPolicy == DocumentCloseDirtyPolicy::prompt);
    REQUIRE_FALSE(lifecycle.hasPendingClose());
}

TEST_CASE("documentLifecycle_Should_KeepActivationAndCloseQueuesIndependent")
{
    DocumentLifecycleController lifecycle;
    lifecycle.requestActivation(independentActivation);
    lifecycle.requestClose({.id = independentClose, .dirtyPolicy = DocumentCloseDirtyPolicy::prompt});

    REQUIRE(lifecycle.takeActivation() == independentActivation);
    const auto closeRequest = lifecycle.takeClose().value_or(uo_mule::gui::DocumentCloseRequest{});
    REQUIRE(closeRequest.id == independentClose);
}

TEST_CASE("documentLifecycle_Should_NotReplaceDirtyClosePolicyAfterTraversalRequest")
{
    DocumentLifecycleController lifecycle;
    lifecycle.requestClose({.id = firstDirtyClose, .dirtyPolicy = DocumentCloseDirtyPolicy::prompt});
    lifecycle.requestClose({.id = secondDirtyClose, .dirtyPolicy = DocumentCloseDirtyPolicy::prompt});

    const auto request = lifecycle.takeClose();
    REQUIRE(request.has_value());
    REQUIRE(request.value_or(uo_mule::gui::DocumentCloseRequest{}).id == firstDirtyClose);
    REQUIRE_FALSE(lifecycle.takeClose().has_value());
}
