#include "gui/data_profile_store.h"
#include "core/app_info.h"
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <system_error>
#include <unistd.h>

namespace uo_mule::gui
{

namespace
{

std::atomic<std::uint64_t> temporaryFileCounter = 0;

[[nodiscard]] std::filesystem::path defaultConfigDirectory()
{
    if (const char * config = std::getenv("XDG_CONFIG_HOME"); config != nullptr && std::filesystem::path(config).is_absolute())
    {
        return config;
    }

    if (const char * home = std::getenv("HOME"); home != nullptr && *home != '\0')
    {
        return std::filesystem::path(home) / ".config";
    }
    return {};
}

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

void parseSchemaVersion(DataProfileLoadResult & result,
                        const std::filesystem::path & path,
                        std::istringstream & line,
                        std::string_view key)
{
    std::uint32_t schema = 0;
    if (!(line >> schema))
    {
        addParseDiagnostic(result, path, key);
    }
    else if (schema == 0)
    {
        result.profile.schemaVersion = DataProfile::currentSchemaVersion;
        addDiagnostic(result, Diagnostic::Severity::info, "migrated data profile schema to version 1", path);
    }
    else if (schema <= DataProfile::currentSchemaVersion)
    {
        result.profile.schemaVersion = DataProfile::currentSchemaVersion;
    }
    else
    {
        addDiagnostic(result, Diagnostic::Severity::error, "data profile schema is newer than this application", path);
    }
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

    if (key == "output_root" && readQuoted(line, value))
    {
        result.profile.outputRoot = value;
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

    if (key == "create_output_if_missing" && (line >> result.profile.createOutputIfMissing))
    {
        return true;
    }

    if (key == "use_source_folder_for_output" && (line >> result.profile.useSourceFolderForOutput))
    {
        return true;
    }

    if (key == "allow_source_output_same" && (line >> result.profile.allowSourceOutputSame))
    {
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

[[nodiscard]] bool
parseFacetName(DataProfileLoadResult & result, std::string_view key, std::istringstream & line, std::string & value)
{
    if (!key.ends_with(".facet") || !readQuoted(line, value))
    {
        return false;
    }

    const auto roleName = key.substr(0, key.size() - std::string_view(".facet").size());
    if (const auto role = dataFileRoleFromName(roleName); role && isMapRole(*role))
    {
        result.profile.facets.at(mapRoleIndex(*role)).facet = value;
    }

    return true;
}

[[nodiscard]] bool parseFacetProperty(DataProfileLoadResult & result,
                                      const std::filesystem::path & path,
                                      std::string_view key,
                                      std::istringstream & line)
{
    const bool isDimension = key.ends_with(".width") || key.ends_with(".height");
    const bool isRecovery = key.ends_with(".recover_truncated");
    if (!isDimension && !isRecovery)
    {
        return false;
    }

    const auto separator = key.find('.');
    if (separator == std::string_view::npos)
    {
        return true;
    }

    const auto role = dataFileRoleFromName(key.substr(0, separator));
    if (!role || !isMapRole(*role))
    {
        return true;
    }

    bool boolean = false;
    if (isRecovery && (line >> boolean))
    {
        result.profile.facets.at(mapRoleIndex(*role)).recoverTruncated = boolean;
        return true;
    }

    auto & dimensions = result.profile.facets.at(mapRoleIndex(*role)).dimensions;
    if (!dimensions)
    {
        dimensions.emplace();
    }

    std::uint32_t number = 0;
    if (line >> number)
    {
        if (key.ends_with(".width"))
        {
            dimensions->widthTiles = number;
        }
        else
        {
            dimensions->heightTiles = number;
        }
    }
    else
    {
        addParseDiagnostic(result, path, key);
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
    if (key == "schema_version")
    {
        parseSchemaVersion(result, path, line, key);
    }
    else if (!parseGeneralField(result, path, key, line, value) && !parseRolePath(result, path, key, line, value) &&
             !parseRoleOverride(result, path, key, line) && !parseFacetName(result, key, line, value) &&
             !parseFacetProperty(result, path, key, line))
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
    contents << "output_root " << std::quoted(profile.outputRoot.string()) << '\n';
    contents << "discovery_mode " << std::quoted(std::string(discoveryModeName(profile.discoveryMode))) << '\n';
    contents << "create_output_if_missing " << profile.createOutputIfMissing << '\n';
    contents << "use_source_folder_for_output " << profile.useSourceFolderForOutput << '\n';
    contents << "allow_source_output_same " << profile.allowSourceOutputSame << '\n';
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

void appendFacetSettings(std::ostringstream & contents, const DataProfile & profile)
{
    for (std::size_t index = 0; index < profile.facets.size(); index++)
    {
        const auto role = static_cast<DataFileRole>(index);
        const auto & facet = profile.facets.at(index);
        if (!facet.facet.empty())
        {
            contents << dataFileRoleName(role) << ".facet " << std::quoted(facet.facet) << '\n';
        }

        if (facet.dimensions)
        {
            contents << dataFileRoleName(role) << ".width " << facet.dimensions->widthTiles << '\n';
            contents << dataFileRoleName(role) << ".height " << facet.dimensions->heightTiles << '\n';
        }

        if (facet.dimensions || facet.recoverTruncated)
        {
            contents << dataFileRoleName(role) << ".recover_truncated " << facet.recoverTruncated << '\n';
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
    const auto config = defaultConfigDirectory();
    return config.empty() ? std::filesystem::path{} : config / std::string(core::applicationName()) / "profiles";
}

std::filesystem::path dataProfilePath(std::string_view profileName, const std::filesystem::path & directory)
{
    const auto baseDirectory = directory.empty() ? dataProfilesDirectory() : directory;
    return baseDirectory / (safeProfileFileStem(profileName) + ".profile");
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
    appendFacetSettings(contents, profile);
    writeProfileAtomically(path, contents.str());
}

} // namespace uo_mule::gui
