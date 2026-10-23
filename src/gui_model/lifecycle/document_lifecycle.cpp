#include "gui_model/lifecycle/document_lifecycle.h"
#include "gui_model/documents/document_identity.h"
#include <optional>

namespace uo_mule::gui
{

void DocumentLifecycleController::requestActivation(DocumentId id) noexcept
{
    if (!activation_)
    {
        activation_ = id;
    }
}

void DocumentLifecycleController::requestClose(DocumentCloseRequest request) noexcept
{
    if (!close_)
    {
        close_ = request;
    }
}

std::optional<DocumentId> DocumentLifecycleController::takeActivation() noexcept
{
    const auto result = activation_;
    activation_.reset();
    return result;
}

std::optional<DocumentCloseRequest> DocumentLifecycleController::takeClose() noexcept
{
    const auto result = close_;
    close_.reset();
    return result;
}

} // namespace uo_mule::gui
