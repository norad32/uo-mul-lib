#include "gui/operations/operation_controller.h"
#include "gui/profile/data_profile.h"
#include "gui/workspaces/map_workspace.h"
#include <utility>

namespace uo_mule::gui
{

OperationTransition
OperationController::begin(OperationRequest request, bool dirty, bool outputExists, OutputCollisionPolicy collisionPolicy)
{
    request_ = std::move(request);
    dirty_ = dirty;
    outputExists_ = outputExists;
    collisionPolicy_ = collisionPolicy;
    phase_ = OperationPhase::idle;

    if (producesOutput() && outputExists_)
    {
        if (collisionPolicy_ == OutputCollisionPolicy::reject)
        {
            return {.kind = OperationTransitionKind::rejected, .error = "Output already exists"};
        }
        if (collisionPolicy_ == OutputCollisionPolicy::confirm)
        {
            phase_ = OperationPhase::awaitingOverwrite;
            return {.kind = OperationTransitionKind::showOverwriteConfirmation, .error = {}};
        }
    }
    return continueAfterChecks();
}

OperationTransition OperationController::resolveDirty(PendingChangesAction action)
{
    if (phase_ != OperationPhase::awaitingDirtyDecision)
    {
        return {.kind = OperationTransitionKind::rejected, .error = "No dirty operation is pending"};
    }
    return resolveDirtyDecision(action);
}

OperationTransition OperationController::resolveOverwrite(bool overwrite)
{
    if (phase_ != OperationPhase::awaitingOverwrite)
    {
        return {.kind = OperationTransitionKind::rejected, .error = "No output collision is pending"};
    }
    if (!overwrite)
    {
        reset();
        return {.kind = OperationTransitionKind::cancelled, .error = {}};
    }
    return continueAfterChecks();
}

OperationTransition OperationController::completeSave(bool success)
{
    if (phase_ != OperationPhase::awaitingSave)
    {
        return {.kind = OperationTransitionKind::rejected, .error = "No save is pending"};
    }
    if (!success)
    {
        reset();
        return {.kind = OperationTransitionKind::cancelled, .error = {}};
    }
    dirty_ = false;
    return continueAfterChecks();
}

OperationTransition OperationController::cancel()
{
    reset();
    return {.kind = OperationTransitionKind::cancelled, .error = {}};
}

OperationPhase OperationController::phase() const noexcept
{
    return phase_;
}

const OperationRequest & OperationController::request() const noexcept
{
    return request_;
}

void OperationController::reset() noexcept
{
    request_ = {};
    phase_ = OperationPhase::idle;
    dirty_ = false;
    outputExists_ = false;
}

OperationTransition OperationController::continueAfterChecks()
{
    if (dirty_)
    {
        phase_ = OperationPhase::awaitingDirtyDecision;
        return {.kind = OperationTransitionKind::showDirtyDecision, .error = {}};
    }
    phase_ = OperationPhase::idle;
    return {.kind = OperationTransitionKind::execute, .error = {}};
}

OperationTransition OperationController::resolveDirtyDecision(PendingChangesAction action)
{
    switch (action)
    {
        case PendingChangesAction::cancel :
            return cancel();
        case PendingChangesAction::discard :
            dirty_ = false;
            return continueAfterChecks();
        case PendingChangesAction::save :
            phase_ = OperationPhase::awaitingSave;
            return {.kind = OperationTransitionKind::showSaveDialog, .error = {}};
    }
    return {.kind = OperationTransitionKind::rejected, .error = "Unknown dirty decision"};
}

bool OperationController::producesOutput() const noexcept
{
    return request_.kind == OperationKind::newMap || request_.kind == OperationKind::save;
}

} // namespace uo_mule::gui
