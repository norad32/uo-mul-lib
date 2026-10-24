#include "gui_model/profile/data_profile.h"
#include <array>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace uo_mule::gui
{

namespace
{

constexpr std::array<DataFileRole, 4> roles{
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

std::string_view tiledataLayoutSelectionName(TiledataLayoutSelection layout) noexcept
{
    switch (layout)
    {
        case TiledataLayoutSelection::autoDetect :
            return "auto";
        case TiledataLayoutSelection::legacy32 :
            return "legacy32";
        case TiledataLayoutSelection::extended64 :
            return "extended64";
    }
    return "auto";
}

std::optional<TiledataLayoutSelection> tiledataLayoutSelectionFromName(std::string_view name) noexcept
{
    for (const auto layout :
         {TiledataLayoutSelection::autoDetect, TiledataLayoutSelection::legacy32, TiledataLayoutSelection::extended64})
    {
        if (tiledataLayoutSelectionName(layout) == name)
        {
            return layout;
        }
    }
    return std::nullopt;
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
