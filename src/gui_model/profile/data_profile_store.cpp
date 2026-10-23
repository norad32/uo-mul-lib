#include "gui_model/profile/data_profile_store.h"
#include "core/app_info.h"
#include "gui_model/persistence/config_paths.h"
#include "gui_model/profile/data_profile.h"
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
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
#include <unistd.h>
#include <utility>

namespace uo_mule::gui
{

namespace
{

std::atomic<std::uint64_t> temporaryFileCounter = 0;

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

[[nodiscard]] std::filesystem::path temporaryProfilePath(const std::filesystem::path & path)
{
    return path.string() + ".tmp." + std::to_string(getpid()) + "." + std::to_string(temporaryFileCounter.fetch_add(1));
}

void writeProfileAtomically(const std::filesystem::path & path, std::string_view contents)
{
    const auto temporary = temporaryProfilePath(path);
    {
        std::ofstream file(temporary, std::ios::trunc);
        if (!file)
        {
            throw std::system_error(errno, std::generic_category(), "Cannot create temporary data profile");
        }

        file << contents;
        file.flush();
        if (!file)
        {
            throw std::system_error(errno, std::generic_category(), "Cannot write data profile");
        }
    }

    if (std::rename(temporary.c_str(), path.c_str()) != 0)
    {
        std::filesystem::remove(temporary);
        throw std::system_error(errno, std::generic_category(), "Cannot replace data profile");
    }
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

    std::string key;
    std::string rawValue;

    while (file >> key >> std::ws)
    {
        std::getline(file, rawValue);
        parseProfileLine(result, path, key, rawValue);
    }

    result.profile.schemaVersion = DataProfile::currentSchemaVersion;
    return result;
}

void saveDataProfile(const DataProfile & profile, const std::filesystem::path & path)
{
    std::filesystem::create_directories(path.parent_path());
    std::ostringstream contents;
    contents << serializeProfile(profile);
    appendRoleFiles(contents, profile);
    writeProfileAtomically(path, contents.str());
}

} // namespace uo_mule::gui
