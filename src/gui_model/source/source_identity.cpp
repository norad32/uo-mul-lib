#include "gui_model/source/source_identity.h"
#include <filesystem>
#include <initializer_list>
#include <span>
#include <system_error>
#include <vector>

namespace uo_mule::gui
{

SourceIdentity::SourceIdentity() = default;

SourceIdentity::~SourceIdentity() = default;

std::filesystem::path canonicalSourcePath(const std::filesystem::path & path)
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

const std::filesystem::path * SourceIdentity::path(SourceRole role) const noexcept
{
    const auto iterator = roles.find(role);
    return iterator == roles.end() ? nullptr : &iterator->second;
}

std::vector<std::filesystem::path> SourceIdentity::paths() const
{
    std::vector<std::filesystem::path> result;
    result.reserve(roles.size());
    for (const auto & [role, pathValue] : roles)
    {
        static_cast<void>(role);
        result.push_back(pathValue);
    }
    return result;
}

SourceIdentity makeSourceIdentity(std::span<const std::filesystem::path> paths)
{
    std::vector<SourcePath> sources;
    sources.reserve(paths.size());
    bool first = true;
    for (const auto & path : paths)
    {
        sources.push_back({.role = first ? SourceRole::primary : SourceRole::companion, .path = path});
        first = false;
    }
    return makeRoleAwareSourceIdentity(sources);
}

SourceIdentity makeRoleAwareSourceIdentity(std::span<const SourcePath> sources)
{
    SourceIdentity identity;
    for (const auto & source : sources)
    {
        identity.roles[source.role] = canonicalSourcePath(source.path);
    }
    return identity;
}

SourceIdentity makeRoleAwareSourceIdentity(std::initializer_list<SourcePath> sources)
{
    return makeRoleAwareSourceIdentity(std::span<const SourcePath>(sources.begin(), sources.size()));
}

SourceIdentity makeSourceIdentity(std::initializer_list<std::filesystem::path> paths)
{
    return makeSourceIdentity(std::span<const std::filesystem::path>(paths.begin(), paths.size()));
}

} // namespace uo_mule::gui
