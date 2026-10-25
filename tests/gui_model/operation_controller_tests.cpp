#include "gui_model/documents/map_document_state.h"
#include "gui_model/operations/operation_controller.h"
#include "gui_model/profile/data_profile.h"
#include <catch2/catch_test_macros.hpp>
#include <optional>

using uo_mule::gui::OperationController;
using uo_mule::gui::OperationKind;
using uo_mule::gui::OperationPhase;
using uo_mule::gui::OperationRequest;
using uo_mule::gui::OperationTransitionKind;
using uo_mule::gui::OutputCollisionPolicy;
using uo_mule::gui::PendingChangesAction;

TEST_CASE("OperationController_Should_RequestDirtyDecisionBeforeClose")
{
    OperationController controller;
    OperationRequest request{};
    request.kind = OperationKind::close;
    const auto transition = controller.begin(request, true, false, OutputCollisionPolicy::confirm);

    REQUIRE(transition.kind == OperationTransitionKind::showDirtyDecision);
    REQUIRE(controller.resolveDirty(PendingChangesAction::discard).kind == OperationTransitionKind::execute);
}

TEST_CASE("OperationController_Should_UseSaveThenContinueForDirtyClose")
{
    OperationController controller;
    OperationRequest request{};
    request.kind = OperationKind::quit;
    REQUIRE(controller.begin(request, true, false, OutputCollisionPolicy::confirm).kind ==
            OperationTransitionKind::showDirtyDecision);
    REQUIRE(controller.resolveDirty(PendingChangesAction::save).kind == OperationTransitionKind::showSaveDialog);
    REQUIRE(controller.completeSave(true).kind == OperationTransitionKind::execute);
}

TEST_CASE("OperationController_Should_PreservePendingSaveAfterFailure")
{
    OperationController controller;
    OperationRequest request{
        .kind = OperationKind::close,
        .documentId = std::nullopt,
        .path = {},
        .dimensions = uomul::Dimensions(),
        .initialTile = uomul::MapTile(),
    };

    REQUIRE(controller.begin(request, true, false, OutputCollisionPolicy::confirm).kind ==
            OperationTransitionKind::showDirtyDecision);
    REQUIRE(controller.resolveDirty(PendingChangesAction::save).kind == OperationTransitionKind::showSaveDialog);
    REQUIRE(controller.completeSave(false).kind == OperationTransitionKind::saveFailed);
    REQUIRE(controller.phase() == OperationPhase::awaitingSave);
}

TEST_CASE("OperationController_Should_ApplyOutputCollisionPolicy")
{
    OperationController controller;
    OperationRequest request{};
    request.kind = OperationKind::newMap;

    REQUIRE(controller.begin(request, false, true, OutputCollisionPolicy::reject).kind == OperationTransitionKind::rejected);
    REQUIRE(controller.begin(request, false, true, OutputCollisionPolicy::confirm).kind ==
            OperationTransitionKind::showOverwriteConfirmation);
    REQUIRE(controller.resolveOverwrite(false).kind == OperationTransitionKind::cancelled);

    REQUIRE(controller.begin(request, false, true, OutputCollisionPolicy::confirm).kind ==
            OperationTransitionKind::showOverwriteConfirmation);
    REQUIRE(controller.resolveOverwrite(true).kind == OperationTransitionKind::execute);
}

TEST_CASE("OperationController_Should_CancelPendingTransitions")
{
    OperationController controller;
    OperationRequest request{};
    request.kind = OperationKind::close;
    REQUIRE(controller.begin(request, true, false, OutputCollisionPolicy::confirm).kind ==
            OperationTransitionKind::showDirtyDecision);
    REQUIRE(controller.cancel().kind == OperationTransitionKind::cancelled);
    REQUIRE(controller.phase() == uo_mule::gui::OperationPhase::idle);
}

TEST_CASE("OperationController_Should_RouteMapSaveAsThroughSavePhase")
{
    OperationController controller;
    OperationRequest request{
        .kind = OperationKind::saveAs,
        .documentId = std::nullopt,
        .path = "map-copy.mul",
        .dimensions = uomul::Dimensions(),
        .initialTile = uomul::MapTile(),
    };

    REQUIRE(controller.begin(request, false, false, OutputCollisionPolicy::confirm).kind ==
            OperationTransitionKind::showSaveDialog);
    REQUIRE(controller.phase() == OperationPhase::awaitingSave);
    REQUIRE(controller.completeSave(false).kind == OperationTransitionKind::saveFailed);
    REQUIRE(controller.phase() == OperationPhase::awaitingSave);
    REQUIRE(controller.completeSave(true).kind == OperationTransitionKind::execute);
}

TEST_CASE("OperationController_Should_RouteSaveAsCollisionBeforeSave")
{
    OperationController controller;
    OperationRequest request{
        .kind = OperationKind::saveAs,
        .documentId = std::nullopt,
        .path = "map-copy.mul",
        .dimensions = uomul::Dimensions(),
        .initialTile = uomul::MapTile(),
    };

    REQUIRE(controller.begin(request, false, true, OutputCollisionPolicy::confirm).kind ==
            OperationTransitionKind::showOverwriteConfirmation);
    REQUIRE(controller.resolveOverwrite(true).kind == OperationTransitionKind::showSaveDialog);
    REQUIRE(controller.phase() == OperationPhase::awaitingSave);
}

TEST_CASE("OperationController_Should_RouteDirtySaveAsThroughDirtyDecision")
{
    OperationController controller;
    OperationRequest request{
        .kind = OperationKind::saveAs,
        .documentId = std::nullopt,
        .path = "map-copy.mul",
        .dimensions = uomul::Dimensions(),
        .initialTile = uomul::MapTile(),
    };

    REQUIRE(controller.begin(request, true, false, OutputCollisionPolicy::confirm).kind ==
            OperationTransitionKind::showDirtyDecision);
    REQUIRE(controller.resolveDirty(PendingChangesAction::save).kind == OperationTransitionKind::showSaveDialog);
    REQUIRE(controller.phase() == OperationPhase::awaitingSave);
}

TEST_CASE("OperationController_Should_RestoreDirtyDecisionAfterDiscardFailure")
{
    OperationController controller;
    OperationRequest request{
        .kind = OperationKind::close,
        .documentId = std::nullopt,
        .path = {},
        .dimensions = uomul::Dimensions(),
        .initialTile = uomul::MapTile(),
    };

    REQUIRE(controller.begin(request, true, false, OutputCollisionPolicy::confirm).kind ==
            OperationTransitionKind::showDirtyDecision);
    REQUIRE(controller.resolveDirty(PendingChangesAction::discard).kind == OperationTransitionKind::execute);
    REQUIRE(controller.failDirtyResolution().kind == OperationTransitionKind::saveFailed);
    REQUIRE(controller.phase() == OperationPhase::awaitingDirtyDecision);
    REQUIRE(controller.resolveDirty(PendingChangesAction::cancel).kind == OperationTransitionKind::cancelled);
}
