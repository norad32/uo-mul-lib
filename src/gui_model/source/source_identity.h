#pragma once

#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <map>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace uo_mule::gui
{

enum class SourceRole : std::uint8_t
{
    primary,
    companion,
    index,
    data,
};

struct SourcePath
{
    SourceRole role = SourceRole::primary;
    std::filesystem::path path;
};

struct SourceIdentity
{
    std::map<SourceRole, std::filesystem::path> roles;

    SourceIdentity();
    ~SourceIdentity();
    SourceIdentity(const SourceIdentity &) = default;
    SourceIdentity & operator=(const SourceIdentity &) = default;
    SourceIdentity(SourceIdentity &&) noexcept = default;
    SourceIdentity & operator=(SourceIdentity &&) noexcept = default;

    bool operator==(const SourceIdentity &) const = default;

    [[nodiscard]] bool contains(SourceRole role) const noexcept { return roles.contains(role); }
    [[nodiscard]] const std::filesystem::path * path(SourceRole role) const noexcept;
    [[nodiscard]] std::vector<std::filesystem::path> paths() const;
};

[[nodiscard]] std::filesystem::path canonicalSourcePath(const std::filesystem::path & path);
[[nodiscard]] SourceIdentity makeSourceIdentity(std::span<const std::filesystem::path> paths);
[[nodiscard]] SourceIdentity makeSourceIdentity(std::initializer_list<std::filesystem::path> paths);
[[nodiscard]] SourceIdentity makeRoleAwareSourceIdentity(std::span<const SourcePath> sources);
[[nodiscard]] SourceIdentity makeRoleAwareSourceIdentity(std::initializer_list<SourcePath> sources);

} // namespace uo_mule::gui
