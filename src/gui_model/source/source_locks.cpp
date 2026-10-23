#include "gui_model/source/source_locks.h"
#include "gui_model/documents/document_identity.h"
#include <cstddef>
#include <functional>
#include <optional>
#include <string>

namespace uo_mule::gui
{

namespace
{

inline constexpr std::size_t hashGoldenRatio = 0x9e3779b9U;
inline constexpr std::size_t hashSeedShift = 6U;
inline constexpr std::size_t hashValueShift = 2U;

[[nodiscard]] std::size_t hashCombine(std::size_t seed, std::size_t value) noexcept
{
    return seed ^ (value + hashGoldenRatio + (seed << hashSeedShift) + (seed >> hashValueShift));
}

} // namespace

std::optional<DocumentId> SourceLockRegistry::owner(const DocumentSourceIdentity & identity) const
{
    const auto iterator = locks_.find({.identity = identity});
    return iterator == locks_.end() ? std::nullopt : std::optional<DocumentId>(iterator->second);
}

bool SourceLockRegistry::acquire(DocumentId documentId, const DocumentSourceIdentity & identity)
{
    if (identity.roles.empty())
    {
        return true;
    }
    if (const auto existing = owner(identity); existing && *existing != documentId)
    {
        return false;
    }
    locks_[{.identity = identity}] = documentId;
    return true;
}

void SourceLockRegistry::release(DocumentId documentId) noexcept
{
    for (auto iterator = locks_.begin(); iterator != locks_.end();)
    {
        if (iterator->second == documentId)
        {
            iterator = locks_.erase(iterator);
        }
        else
        {
            iterator++;
        }
    }
}

void SourceLockRegistry::clear() noexcept
{
    locks_.clear();
}

std::size_t SourceLockRegistry::LockKeyHash::operator()(const LockKey & key) const noexcept
{
    std::size_t result = 0;
    for (const auto & [role, path] : key.identity.roles)
    {
        result = hashCombine(result, std::hash<unsigned int>{}(static_cast<unsigned int>(role)));
        result = hashCombine(result, std::hash<std::string>{}(path.string()));
    }
    return result;
}

} // namespace uo_mule::gui
