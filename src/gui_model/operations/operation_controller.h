#pragma once

#include "gui_model/documents/document_identity.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/documents/map_document_state.h"
#include <cstdint>
#include <filesystem>
#include <string>

namespace uo_mule::gui
{

enum class OperationKind : std::uint8_t
{
    none,
    openMap,
    newMap,
    save,
    saveAs,
    close,
    quit,
};

struct OperationRequest
{
    OperationKind kind = OperationKind::none;
    std::optional<DocumentId> documentId;
    std::filesystem::path path;
    uomul::Dimensions dimensions{};
    uomul::MapTile initialTile{};
};

enum class OperationPhase : std::uint8_t
{
    idle,
    awaitingDirtyDecision,
    awaitingSave,
    awaitingOverwrite,
};

enum class OperationTransitionKind : std::uint8_t
{
    execute,
    showDirtyDecision,
    showSaveDialog,
    saveFailed,
    showOverwriteConfirmation,
    rejected,
    cancelled,
};

struct OperationTransition
{
    OperationTransitionKind kind = OperationTransitionKind::cancelled;
    std::string error;
};

class OperationController
{
public:
    [[nodiscard]] OperationTransition
    begin(OperationRequest request, bool dirty, bool outputExists, OutputCollisionPolicy collisionPolicy);
    [[nodiscard]] OperationTransition resolveDirty(PendingChangesAction action);
    [[nodiscard]] OperationTransition resolveOverwrite(bool overwrite);
    [[nodiscard]] OperationTransition completeSave(bool success);
    [[nodiscard]] OperationTransition failDirtyResolution();
    [[nodiscard]] OperationTransition cancel();
    [[nodiscard]] OperationPhase phase() const noexcept;
    [[nodiscard]] const OperationRequest & request() const noexcept;
    void reset() noexcept;

private:
    [[nodiscard]] OperationTransition continueAfterChecks();
    [[nodiscard]] OperationTransition resolveDirtyDecision(PendingChangesAction action);
    [[nodiscard]] bool producesOutput() const noexcept;

    OperationRequest request_{};
    OperationPhase phase_ = OperationPhase::idle;
    bool dirty_ = false;
    bool outputExists_ = false;
    OutputCollisionPolicy collisionPolicy_ = OutputCollisionPolicy::confirm;
};

} // namespace uo_mule::gui
