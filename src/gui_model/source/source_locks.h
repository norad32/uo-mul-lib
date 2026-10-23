#pragma once

#include "gui_model/documents/document_identity.h"
#include <optional>
#include <unordered_map>

namespace uo_mule::gui
{

class SourceLockRegistry
{
public:
    [[nodiscard]] std::optional<DocumentId> owner(const DocumentSourceIdentity & identity) const;
    [[nodiscard]] bool acquire(DocumentId documentId, const DocumentSourceIdentity & identity);
    void release(DocumentId documentId) noexcept;
    void clear() noexcept;

private:
    struct LockKey
    {
        DocumentSourceIdentity identity;

        bool operator==(const LockKey & other) const noexcept { return identity == other.identity; }
    };

    struct LockKeyHash
    {
        [[nodiscard]] std::size_t operator()(const LockKey & key) const noexcept;
    };

    std::unordered_map<LockKey, DocumentId, LockKeyHash> locks_;
};

} // namespace uo_mule::gui
