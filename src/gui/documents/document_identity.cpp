#include "gui/documents/document_identity.h"
#include "gui/documents/document_contracts.h"
#include "gui/profile/data_profile.h"
#include <algorithm>
#include <filesystem>
#include <initializer_list>
#include <optional>
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

std::optional<DocumentSourceIdentity> configuredDocumentSourceIdentity(const DataProfile & profile, DocumentKind kind)
{
    switch (kind)
    {
        case DocumentKind::radarcol :
        {
            const auto path = resolvedDataFilePath(profile, DataFileRole::radarcol);
            if (path.empty())
            {
                return std::nullopt;
            }
            return makeDocumentSourceIdentity({path});
        }
        case DocumentKind::texmapPair :
        {
            const auto index = resolvedDataFilePath(profile, DataFileRole::texidx);
            const auto data = resolvedDataFilePath(profile, DataFileRole::texmaps);
            if (index.empty() || data.empty())
            {
                return std::nullopt;
            }
            return makeDocumentSourceIdentity({index, data});
        }
        case DocumentKind::map :
        case DocumentKind::unavailable :
            return std::nullopt;
    }
    return std::nullopt;
}

bool sameDocumentIdentity(const DocumentIdentity & first, const DocumentIdentity & second) noexcept
{
    return first.kind == second.kind && first.source == second.source;
}

} // namespace uo_mule::gui
