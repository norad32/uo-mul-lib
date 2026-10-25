#pragma once

#include "gui_model/documents/document_contracts.h"
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace uo_mule::gui
{

/** @brief Shared state for a document with staged editable changes. */
class StagedDocumentState
{
public:
    [[nodiscard]] DocumentAccess access() const noexcept { return access_; }
    [[nodiscard]] bool readOnly() const noexcept { return access_ == DocumentAccess::readOnly; }
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
    [[nodiscard]] const std::string & error() const noexcept { return error_; }

    void setAccess(DocumentAccess value) noexcept { access_ = value; }
    void setDirty(bool value) noexcept { dirty_ = value; }
    void touch() noexcept { revision_++; }
    void resetChanges() noexcept
    {
        dirty_ = false;
        revision_++;
    }
    void setError(std::string value) { error_ = std::move(value); }
    void clearError() noexcept { error_.clear(); }

private:
    DocumentAccess access_ = DocumentAccess::editable;
    bool dirty_ = false;
    std::uint64_t revision_ = 0;
    std::string error_;
};

/** @brief Single pending intent slot shared by staged document types. */
template <typename Intent>
class PendingDocumentIntent
{
public:
    void request(Intent intent) { pending_ = std::move(intent); }

    [[nodiscard]] std::optional<Intent> take() noexcept
    {
        if (!pending_)
        {
            return std::nullopt;
        }
        std::optional<Intent> result = std::move(pending_);
        pending_.reset();
        return result;
    }

private:
    std::optional<Intent> pending_;
};

} // namespace uo_mule::gui
