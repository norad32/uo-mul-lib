#pragma once

#include "gui_model/persistence/preferences.h"
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

enum class DataFileRole : std::uint8_t
{
    radarcol,
    texidx,
    texmaps,
    tiledata,
};

enum class TiledataLayoutSelection : std::uint8_t
{
    autoDetect,
    legacy32,
    extended64,
};

enum class DataFileStatus : std::uint8_t
{
    found,
    foundAndValidated,
    missing,
    ambiguous,
    invalid,
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

inline constexpr int defaultProfileWindowWidth = 1280;
inline constexpr int defaultProfileWindowHeight = 1000;

struct ProfileWindowLayout
{
    bool geometryValid = false;
    int x = 0;
    int y = 0;
    int width = defaultProfileWindowWidth;
    int height = defaultProfileWindowHeight;
};

struct ProfileLayout
{
    ProfileWindowLayout window;
    std::string dockingIni;
    std::map<std::string, bool> panels;
    std::map<std::string, bool> windows;
};

struct DataProfile
{
    static constexpr std::uint32_t currentSchemaVersion = 0;

    std::uint32_t schemaVersion = currentSchemaVersion;
    std::string name;
    std::filesystem::path sourceRoot;
    DiscoveryMode discoveryMode = DiscoveryMode::none;
    OutputCollisionPolicy outputCollisionPolicy = OutputCollisionPolicy::confirm;
    TiledataLayoutSelection tiledataLayout = TiledataLayoutSelection::autoDetect;
    std::map<DataFileRole, DataFileEntry> files;
    Preferences preferences;
    ProfileLayout layout;
};

[[nodiscard]] const std::vector<DataFileRole> & knownDataFileRoles() noexcept;
[[nodiscard]] std::string_view dataFileRoleName(DataFileRole role) noexcept;
[[nodiscard]] std::optional<DataFileRole> dataFileRoleFromName(std::string_view name) noexcept;
[[nodiscard]] std::string_view dataFileStatusName(DataFileStatus status) noexcept;
[[nodiscard]] std::string_view discoveryModeName(DiscoveryMode mode) noexcept;
[[nodiscard]] std::optional<DiscoveryMode> discoveryModeFromName(std::string_view name) noexcept;
[[nodiscard]] std::string_view outputCollisionPolicyName(OutputCollisionPolicy policy) noexcept;
[[nodiscard]] std::optional<OutputCollisionPolicy> outputCollisionPolicyFromName(std::string_view name) noexcept;
[[nodiscard]] std::string_view tiledataLayoutSelectionName(TiledataLayoutSelection layout) noexcept;
[[nodiscard]] std::optional<TiledataLayoutSelection> tiledataLayoutSelectionFromName(std::string_view name) noexcept;
[[nodiscard]] std::filesystem::path resolvedDataFilePath(const DataProfile & profile, DataFileRole role);

} // namespace uo_mule::gui
