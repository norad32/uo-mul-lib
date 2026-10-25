#include "gui_model/operations/operation_controller.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/documents/map_document_state.h"
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
        return {.kind = OperationTransitionKind::saveFailed, .error = "Save operation failed"};
    }
    dirty_ = false;
    if (request_.kind == OperationKind::saveAs)
    {
        phase_ = OperationPhase::idle;
        return {.kind = OperationTransitionKind::execute, .error = {}};
    }
    return continueAfterChecks();
}

OperationTransition OperationController::failDirtyResolution()
{
    if (phase_ != OperationPhase::idle || (request_.kind != OperationKind::close && request_.kind != OperationKind::quit))
    {
        return {.kind = OperationTransitionKind::rejected, .error = "No dirty close resolution is pending"};
    }
    dirty_ = true;
    phase_ = OperationPhase::awaitingDirtyDecision;
    return {.kind = OperationTransitionKind::saveFailed, .error = "Dirty close resolution failed"};
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
    if (request_.kind == OperationKind::saveAs)
    {
        phase_ = OperationPhase::awaitingSave;
        return {.kind = OperationTransitionKind::showSaveDialog, .error = {}};
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
    return request_.kind == OperationKind::newMap || request_.kind == OperationKind::save ||
           request_.kind == OperationKind::saveAs;
}

} // namespace uo_mule::gui
