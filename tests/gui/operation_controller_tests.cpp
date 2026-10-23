#include "gui/operations/operation_controller.h"
#include "gui/profile/data_profile.h"
#include "gui/workspaces/map_workspace.h"
#include <catch2/catch_test_macros.hpp>

using uo_mule::gui::OperationController;
using uo_mule::gui::OperationKind;
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
