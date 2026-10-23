#pragma once

#include "gui_model/documents/document_identity.h"
#include <cstdint>
#include <optional>

namespace uo_mule::gui
{

enum class DocumentCloseDirtyPolicy : std::uint8_t
{
    prompt,
};

struct DocumentCloseRequest
{
    DocumentId id = 0;
    DocumentCloseDirtyPolicy dirtyPolicy = DocumentCloseDirtyPolicy::prompt;
};

class DocumentLifecycleController
{
public:
    void requestActivation(DocumentId id) noexcept;
    void requestClose(DocumentCloseRequest request) noexcept;

    [[nodiscard]] std::optional<DocumentId> takeActivation() noexcept;
    [[nodiscard]] std::optional<DocumentCloseRequest> takeClose() noexcept;

    [[nodiscard]] bool hasPendingActivation() const noexcept { return activation_.has_value(); }
    [[nodiscard]] bool hasPendingClose() const noexcept { return close_.has_value(); }

private:
    std::optional<DocumentId> activation_;
    std::optional<DocumentCloseRequest> close_;
};

} // namespace uo_mule::gui
