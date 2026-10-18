#include "gui/data_profile.h"
#include <array>

namespace uo_mule::gui
{

namespace
{

constexpr std::array<DataFileRole, 12> roles{
    DataFileRole::map0,
    DataFileRole::map1,
    DataFileRole::map2,
    DataFileRole::map3,
    DataFileRole::map4,
    DataFileRole::map5,
    DataFileRole::map6,
    DataFileRole::map7,
    DataFileRole::radarcol,
    DataFileRole::texidx,
    DataFileRole::texmaps,
    DataFileRole::tiledata,
};

} // namespace

const std::vector<DataFileRole> & knownDataFileRoles() noexcept
{
    static const std::vector<DataFileRole> result(roles.begin(), roles.end());
    return result;
}

std::string_view dataFileRoleName(DataFileRole role) noexcept
{
    switch (role)
    {
        case DataFileRole::map0 :
            return "map0";
        case DataFileRole::map1 :
            return "map1";
        case DataFileRole::map2 :
            return "map2";
        case DataFileRole::map3 :
            return "map3";
        case DataFileRole::map4 :
            return "map4";
        case DataFileRole::map5 :
            return "map5";
        case DataFileRole::map6 :
            return "map6";
        case DataFileRole::map7 :
            return "map7";
        case DataFileRole::radarcol :
            return "radarcol";
        case DataFileRole::texidx :
            return "texidx";
        case DataFileRole::texmaps :
            return "texmaps";
        case DataFileRole::tiledata :
            return "tiledata";
    }
    return "unknown";
}

std::optional<DataFileRole> dataFileRoleFromName(std::string_view name) noexcept
{
    for (const auto role : roles)
    {
        if (dataFileRoleName(role) == name)
        {
            return role;
        }
    }
    return std::nullopt;
}

std::string_view dataFileStatusName(DataFileStatus status) noexcept
{
    switch (status)
    {
        case DataFileStatus::found :
            return "found";
        case DataFileStatus::foundAndValidated :
            return "found_and_validated";
        case DataFileStatus::missing :
            return "missing";
        case DataFileStatus::ambiguous :
            return "ambiguous";
        case DataFileStatus::invalid :
            return "invalid";
        case DataFileStatus::overridden :
            return "overridden";
        case DataFileStatus::optional :
            return "optional";
        case DataFileStatus::unsupported :
            return "unsupported";
    }
    return "unknown";
}

std::string_view discoveryModeName(DiscoveryMode mode) noexcept
{
    switch (mode)
    {
        case DiscoveryMode::none :
            return "none";
        case DiscoveryMode::folder :
            return "folder";
        case DiscoveryMode::installation :
            return "installation";
        case DiscoveryMode::deep :
            return "deep";
    }
    return "none";
}

std::optional<DiscoveryMode> discoveryModeFromName(std::string_view name) noexcept
{
    for (const auto mode : {DiscoveryMode::none, DiscoveryMode::folder, DiscoveryMode::installation, DiscoveryMode::deep})
    {
        if (discoveryModeName(mode) == name)
        {
            return mode;
        }
    }
    return std::nullopt;
}

std::string_view outputCollisionPolicyName(OutputCollisionPolicy policy) noexcept
{
    switch (policy)
    {
        case OutputCollisionPolicy::confirm :
            return "confirm";
        case OutputCollisionPolicy::overwrite :
            return "overwrite";
        case OutputCollisionPolicy::reject :
            return "reject";
    }
    return "confirm";
}

std::optional<OutputCollisionPolicy> outputCollisionPolicyFromName(std::string_view name) noexcept
{
    for (const auto policy : {OutputCollisionPolicy::confirm, OutputCollisionPolicy::overwrite, OutputCollisionPolicy::reject})
    {
        if (outputCollisionPolicyName(policy) == name)
        {
            return policy;
        }
    }
    return std::nullopt;
}

bool isMapRole(DataFileRole role) noexcept
{
    switch (role)
    {
        case DataFileRole::map0 :
        case DataFileRole::map1 :
        case DataFileRole::map2 :
        case DataFileRole::map3 :
        case DataFileRole::map4 :
        case DataFileRole::map5 :
        case DataFileRole::map6 :
        case DataFileRole::map7 :
            return true;
        case DataFileRole::radarcol :
        case DataFileRole::texidx :
        case DataFileRole::texmaps :
        case DataFileRole::tiledata :
            return false;
    }
    return false;
}

std::size_t mapRoleIndex(DataFileRole role) noexcept
{
    return static_cast<std::size_t>(role);
}

std::filesystem::path resolvedDataFilePath(const DataProfile & profile, DataFileRole role)
{
    const auto entry = profile.files.find(role);
    if (entry == profile.files.end() || entry->second.path.empty())
    {
        return {};
    }

    if (entry->second.path.is_absolute() || profile.sourceRoot.empty())
    {
        return entry->second.path;
    }
    return profile.sourceRoot / entry->second.path;
}

} // namespace uo_mule::gui
