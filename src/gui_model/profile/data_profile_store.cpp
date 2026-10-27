#include "gui_model/profile/data_profile_store.h"
#include "core/app_info.h"
#include "gui_model/persistence/atomic_file.h"
#include "gui_model/persistence/config_paths.h"
#include "gui_model/persistence/preferences_store.h"
#include "gui_model/persistence/schema_validation.h"
#include "gui_model/profile/data_profile.h"
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <istream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace uo_mule::gui
{

namespace
{

[[nodiscard]] std::string safeProfileFileStem(std::string_view name)
{
    std::string stem;
    stem.reserve(name.size());
    for (const char character : name)
    {
        if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == '-' || character == '_')
        {
            stem.push_back(character);
        }
        else
        {
            stem.push_back('_');
        }
    }

    if (stem.empty() || stem == "." || stem == "..")
    {
        stem = "profile";
    }
    return stem;
}

void addDiagnostic(DataProfileLoadResult & result,
                   Diagnostic::Severity severity,
                   std::string message,
                   std::filesystem::path path = {})
{
    result.diagnostics.push_back(Diagnostic{
        .severity = severity,
        .role = std::nullopt,
        .path = std::move(path),
        .message = std::move(message),
        .fixSuggestions = {},
    });
}

void addParseDiagnostic(DataProfileLoadResult & result, const std::filesystem::path & path, std::string_view key)
{
    addDiagnostic(
        result, Diagnostic::Severity::warning, "ignored invalid value for profile key '" + std::string(key) + "'", path);
}

[[nodiscard]] bool readQuoted(std::istringstream & line, std::string & value)
{
    line >> std::quoted(value);
    return !line.fail();
}

[[nodiscard]] std::string rolePathKey(DataFileRole role)
{
    return std::string(dataFileRoleName(role)) + ".path";
}

[[nodiscard]] bool parseGeneralField(DataProfileLoadResult & result,
                                     const std::filesystem::path & path,
                                     std::string_view key,
                                     std::istringstream & line,
                                     std::string & value)
{
    if (key == "name" && readQuoted(line, value))
    {
        result.profile.name = value;
        return true;
    }

    if (key == "source_root" && readQuoted(line, value))
    {
        result.profile.sourceRoot = value;
        return true;
    }

    if (key == "discovery_mode" && readQuoted(line, value))
    {
        if (const auto mode = discoveryModeFromName(value))
        {
            result.profile.discoveryMode = *mode;
        }
        else
        {
            addParseDiagnostic(result, path, key);
        }
        return true;
    }

    if (key == "output_collision_policy" && readQuoted(line, value))
    {
        if (const auto policy = outputCollisionPolicyFromName(value))
        {
            result.profile.outputCollisionPolicy = *policy;
        }
        else
        {
            addParseDiagnostic(result, path, key);
        }

        return true;
    }

    if (key == "tiledata_layout" && readQuoted(line, value))
    {
        if (const auto layout = tiledataLayoutSelectionFromName(value))
        {
            result.profile.tiledataLayout = *layout;
        }
        else
        {
            addParseDiagnostic(result, path, key);
        }
        return true;
    }

    return false;
}

[[nodiscard]] bool parseRolePath(DataProfileLoadResult & result,
                                 const std::filesystem::path & path,
                                 std::string_view key,
                                 std::istringstream & line,
                                 std::string & value)
{
    if (!key.ends_with(".path") || !readQuoted(line, value))
    {
        return false;
    }

    const auto roleName = key.substr(0, key.size() - std::string_view(".path").size());
    if (const auto role = dataFileRoleFromName(roleName))
    {
        result.profile.files[*role] = DataFileEntry{.path = value};
    }
    else
    {
        addDiagnostic(
            result, Diagnostic::Severity::warning, "ignored unknown data file role '" + std::string(roleName) + "'", path);
    }

    return true;
}

[[nodiscard]] bool parseRoleOverride(DataProfileLoadResult & result,
                                     const std::filesystem::path & path,
                                     std::string_view key,
                                     std::istringstream & line)
{
    if (!key.ends_with(".override"))
    {
        return false;
    }

    const auto role = dataFileRoleFromName(key.substr(0, key.size() - 9));
    bool override = false;
    if (!role || !(line >> override))
    {
        if (!role)
        {
            addDiagnostic(result,
                          Diagnostic::Severity::warning,
                          "ignored unknown data file role in profile key '" + std::string(key) + "'",
                          path);
        }
        else
        {
            addParseDiagnostic(result, path, key);
        }
    }
    else
    {
        result.profile.files[*role].userOverride = override;
    }

    return true;
}

void parseProfileLine(DataProfileLoadResult & result,
                      const std::filesystem::path & path,
                      std::string_view key,
                      std::string_view rawValue)
{
    std::istringstream line{std::string(rawValue)};
    std::string value;
    if (key.starts_with("preference."))
    {
        const auto preferenceKey = key.substr(std::string_view("preference.").size());
        if (preferenceKey == "tool_fuzzy_name")
        {
            line >> std::quoted(value);
        }
        else
        {
            line >> value;
        }
        if (!line.fail())
        {
            applySerializedPreference(result.profile.preferences, preferenceKey, value);
        }
        else
        {
            addParseDiagnostic(result, path, key);
        }
        return;
    }
    if (key == "layout.window_geometry")
    {
        ProfileWindowLayout window;
        if (line >> window.x >> window.y >> window.width >> window.height && window.width > 0 && window.height > 0)
        {
            window.geometryValid = true;
            result.profile.layout.window = window;
        }
        else
        {
            addParseDiagnostic(result, path, key);
        }
        return;
    }
    if (key == "layout.panel" || key == "layout.window")
    {
        std::string stableId;
        bool visible = false;
        if (line >> std::quoted(stableId) >> visible)
        {
            auto & surfaces = key == "layout.panel" ? result.profile.layout.panels : result.profile.layout.windows;
            surfaces[std::move(stableId)] = visible;
        }
        else
        {
            addParseDiagnostic(result, path, key);
        }
        return;
    }
    if (key != "schema_version" && !parseGeneralField(result, path, key, line, value) &&
        !parseRolePath(result, path, key, line, value) && !parseRoleOverride(result, path, key, line))
    {
        addDiagnostic(result, Diagnostic::Severity::info, "ignored unknown profile key '" + std::string(key) + "'", path);
    }
}

[[nodiscard]] std::string serializeProfile(const DataProfile & profile)
{
    std::ostringstream contents;
    contents << "schema_version " << DataProfile::currentSchemaVersion << '\n';
    contents << "name " << std::quoted(profile.name) << '\n';
    contents << "source_root " << std::quoted(profile.sourceRoot.string()) << '\n';
    contents << "discovery_mode " << std::quoted(std::string(discoveryModeName(profile.discoveryMode))) << '\n';
    contents << "output_collision_policy " << std::quoted(std::string(outputCollisionPolicyName(profile.outputCollisionPolicy)))
             << '\n';
    contents << "tiledata_layout " << std::quoted(std::string(tiledataLayoutSelectionName(profile.tiledataLayout))) << '\n';
    return contents.str();
}

void appendRoleFiles(std::ostringstream & contents, const DataProfile & profile)
{
    for (const auto role : knownDataFileRoles())
    {
        if (const auto entry = profile.files.find(role); entry != profile.files.end())
        {
            contents << rolePathKey(role) << ' ' << std::quoted(entry->second.path.string()) << '\n';
            contents << dataFileRoleName(role) << ".override " << entry->second.userOverride << '\n';
        }
    }
}

void appendProfilePreferences(std::ostringstream & contents, const DataProfile & profile)
{
    appendSerializedPreferences(contents, profile.preferences, "preference.");
}

void appendProfileLayout(std::ostringstream & contents, const DataProfile & profile)
{
    if (profile.layout.window.geometryValid)
    {
        contents << "layout.window_geometry " << profile.layout.window.x << ' ' << profile.layout.window.y << ' '
                 << profile.layout.window.width << ' ' << profile.layout.window.height << '\n';
    }
    for (const auto & [stableId, visible] : profile.layout.panels)
    {
        contents << "layout.panel " << std::quoted(stableId) << ' ' << visible << '\n';
    }
    for (const auto & [stableId, visible] : profile.layout.windows)
    {
        contents << "layout.window " << std::quoted(stableId) << ' ' << visible << '\n';
    }
    contents << "layout.docking_ini_size " << profile.layout.dockingIni.size() << '\n';
    contents.write(profile.layout.dockingIni.data(), static_cast<std::streamsize>(profile.layout.dockingIni.size()));
    contents << '\n';
}

[[nodiscard]] bool
validateProfileSchema(std::ifstream & file, DataProfileLoadResult & result, const std::filesystem::path & path)
{
    std::string schemaLine;
    if (!std::getline(file, schemaLine) || !hasSchemaVersion(schemaLine, DataProfile::currentSchemaVersion))
    {
        addDiagnostic(result, Diagnostic::Severity::error, "invalid or missing data profile schema version", path);
        result.profile = {};
        return false;
    }
    return true;
}

} // namespace

std::filesystem::path dataProfilesDirectory()
{
    const auto config = configDirectory();
    return config.empty() ? std::filesystem::path{} : config / std::string(core::applicationName()) / "profiles";
}

std::filesystem::path dataProfilePath(std::string_view profileName, const std::filesystem::path & directory)
{
    const auto baseDirectory = directory.empty() ? dataProfilesDirectory() : directory;
    return baseDirectory / (safeProfileFileStem(profileName) + ".profile");
}

DataProfileStore::DataProfileStore(std::filesystem::path directory) :
    directory_(directory.empty() ? dataProfilesDirectory() : std::move(directory))
{
}

DataProfileLoadResult DataProfileStore::load(std::string_view profileName) const
{
    return loadDataProfile(path(profileName));
}

void DataProfileStore::save(const DataProfile & profile) const
{
    saveDataProfile(profile, path(profile.name));
}

void DataProfileStore::remove(std::string_view profileName) const
{
    std::filesystem::remove(path(profileName));
}

std::filesystem::path DataProfileStore::path(std::string_view profileName) const
{
    return dataProfilePath(profileName, directory_);
}

DataProfileLoadResult loadDataProfile(const std::filesystem::path & path)
{
    DataProfileLoadResult result;
    std::ifstream file(path);
    if (!file)
    {
        std::error_code error;
        if (std::filesystem::exists(path, error) && !error)
        {
            addDiagnostic(result, Diagnostic::Severity::error, "cannot open data profile", path);
        }

        return result;
    }

    if (!validateProfileSchema(file, result, path))
    {
        return result;
    }

    std::string key;
    std::string rawValue;

    while (file >> key)
    {
        if (key == "layout.docking_ini_size")
        {
            std::size_t size = 0;
            if (!(file >> size))
            {
                addParseDiagnostic(result, path, key);
                break;
            }
            file.get();
            std::string dockingIni(size, '\0');
            file.read(dockingIni.data(), static_cast<std::streamsize>(size));
            if (file.gcount() != static_cast<std::streamsize>(size))
            {
                addParseDiagnostic(result, path, key);
                break;
            }
            result.profile.layout.dockingIni = std::move(dockingIni);
            if (file.peek() != std::char_traits<char>::eof())
            {
                file.get();
            }
            continue;
        }
        file >> std::ws;
        std::getline(file, rawValue);
        parseProfileLine(result, path, key, rawValue);
    }

    result.profile.schemaVersion = DataProfile::currentSchemaVersion;
    return result;
}

void saveDataProfile(const DataProfile & profile, const std::filesystem::path & path)
{
    std::ostringstream contents;
    contents << serializeProfile(profile);
    appendRoleFiles(contents, profile);
    appendProfilePreferences(contents, profile);
    appendProfileLayout(contents, profile);
    writeFileAtomically(path, contents.str(), "data profile");
}

} // namespace uo_mule::gui
