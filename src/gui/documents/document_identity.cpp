#include "gui/documents/document_identity.h"
#include <algorithm>
#include <filesystem>
#include <initializer_list>
#include <span>
#include <system_error>

namespace uo_mule::gui
{

std::filesystem::path canonicalDocumentPath(const std::filesystem::path & path)
{
    if (path.empty())
    {
        return {};
    }

    std::error_code error;
    auto canonical = std::filesystem::weakly_canonical(path, error);
    if (!error)
    {
        return canonical;
    }

    const auto absolute = std::filesystem::absolute(path, error);
    if (!error)
    {
        return absolute.lexically_normal();
    }

    return path.lexically_normal();
}

DocumentSourceIdentity makeDocumentSourceIdentity(std::span<const std::filesystem::path> paths)
{
    DocumentSourceIdentity identity;
    identity.canonicalPaths.reserve(paths.size());
    for (const auto & path : paths)
    {
        identity.canonicalPaths.push_back(canonicalDocumentPath(path));
    }
    std::ranges::sort(identity.canonicalPaths);
    return identity;
}

DocumentSourceIdentity makeDocumentSourceIdentity(std::initializer_list<std::filesystem::path> paths)
{
    return makeDocumentSourceIdentity(std::span<const std::filesystem::path>(paths.begin(), paths.size()));
}

bool sameDocumentIdentity(const DocumentIdentity & first, const DocumentIdentity & second) noexcept
{
    return first.kind == second.kind && first.source == second.source;
}

} // namespace uo_mule::gui
