#pragma once

#include "uomul/map.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace uo_mule::gui
{

inline constexpr std::size_t mapFacetCount = 8;

enum class DataFileRole : std::uint8_t
{
    map0,
    map1,
    map2,
    map3,
    map4,
    map5,
    map6,
    map7,
    radarcol,
    texidx,
    texmaps,
    tiledata,
};

enum class DataFileStatus : std::uint8_t
{
    found,
    foundAndValidated,
    missing,
    ambiguous,
    invalid,
    overridden,
    optional,
    unsupported,
};

enum class DiscoveryMode : std::uint8_t
{
    none,
    folder,
    installation,
    deep,
};

enum class OutputCollisionPolicy : std::uint8_t
{
    confirm,
    overwrite,
    reject,
};

struct Diagnostic
{
    enum class Severity : std::uint8_t
    {
        info,
        warning,
        error,
    };

    Severity severity = Severity::info;
    std::optional<DataFileRole> role;
    std::filesystem::path path;
    std::string message;
    std::vector<std::string> fixSuggestions;
};

struct DataFileEntry
{
    std::filesystem::path path;
    bool userOverride = false;
};

struct MapFacetConfiguration
{
    std::string facet;
    std::optional<uomul::Dimensions> dimensions;
    bool recoverTruncated = false;
};

struct DataProfile
{
    static constexpr std::uint32_t currentSchemaVersion = 1;

    std::uint32_t schemaVersion = currentSchemaVersion;
    std::string name;
    std::filesystem::path sourceRoot;
    std::filesystem::path outputRoot;
    DiscoveryMode discoveryMode = DiscoveryMode::none;
    bool createOutputIfMissing = true;
    bool useSourceFolderForOutput = false;
    OutputCollisionPolicy outputCollisionPolicy = OutputCollisionPolicy::confirm;
    bool allowSourceOutputSame = false;
    std::map<DataFileRole, DataFileEntry> files;
    std::array<MapFacetConfiguration, mapFacetCount> facets{};
};

[[nodiscard]] const std::vector<DataFileRole> & knownDataFileRoles() noexcept;
[[nodiscard]] std::string_view dataFileRoleName(DataFileRole role) noexcept;
[[nodiscard]] std::optional<DataFileRole> dataFileRoleFromName(std::string_view name) noexcept;
[[nodiscard]] std::string_view dataFileStatusName(DataFileStatus status) noexcept;
[[nodiscard]] std::string_view discoveryModeName(DiscoveryMode mode) noexcept;
[[nodiscard]] std::optional<DiscoveryMode> discoveryModeFromName(std::string_view name) noexcept;
[[nodiscard]] std::string_view outputCollisionPolicyName(OutputCollisionPolicy policy) noexcept;
[[nodiscard]] std::optional<OutputCollisionPolicy> outputCollisionPolicyFromName(std::string_view name) noexcept;

[[nodiscard]] bool isMapRole(DataFileRole role) noexcept;
[[nodiscard]] std::size_t mapRoleIndex(DataFileRole role) noexcept;
[[nodiscard]] std::filesystem::path resolvedDataFilePath(const DataProfile & profile, DataFileRole role);

} // namespace uo_mule::gui
