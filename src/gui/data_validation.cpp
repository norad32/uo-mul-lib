#include "gui/data_validation.h"
#include "gui/data_profile.h"
#include "uomul/map.h"
#include "uomul/radarcol/radarcol.h"
#include "uomul/texmaps/texmaps.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace uo_mule::gui
{

namespace
{

constexpr std::array<bool, 12> supportedRoles{
    true,
    true,
    true,
    true,
    true,
    true,
    true,
    true,
    true,
    true,
    true,
    false,
};

void addDiagnostic(DataProfileValidation & result,
                   Diagnostic::Severity severity,
                   std::optional<DataFileRole> role,
                   const std::filesystem::path & path,
                   std::string message,
                   std::vector<std::string> suggestions = {})
{
    result.diagnostics.push_back(Diagnostic{
        .severity = severity,
        .role = role,
        .path = path,
        .message = std::move(message),
        .fixSuggestions = std::move(suggestions),
    });
}

[[nodiscard]] bool dimensionsAreValid(const uomul::Dimensions & dimensions) noexcept
{
    return dimensions.widthTiles > 0 && dimensions.heightTiles > 0 &&
           dimensions.widthTiles % uomul::mapBlockDimension == 0 &&
           dimensions.heightTiles % uomul::mapBlockDimension == 0;
}

[[nodiscard]] DataFileValidation makeValidation(DataFileRole role, const std::filesystem::path & path)
{
    return DataFileValidation{
        .role = role,
        .status = DataFileStatus::missing,
        .path = path,
        .fileSize = std::nullopt,
    };
}

void appendValidation(DataProfileValidation & result, DataFileValidation validation)
{
    result.files.push_back(std::move(validation));
}

void validateFileRole(const DataProfile & profile, DataProfileValidation & result, DataFileRole role)
{
    const auto path = resolvedDataFilePath(profile, role);
    auto validation = makeValidation(role, path);
    const auto roleIndex = static_cast<std::size_t>(role);

    if (path.empty())
    {
        validation.status = supportedRoles.at(roleIndex) ? DataFileStatus::missing : DataFileStatus::unsupported;
        appendValidation(result, std::move(validation));
        return;
    }

    std::error_code statusError;
    const bool exists = std::filesystem::exists(path, statusError);
    const bool regularFile = !statusError && exists && std::filesystem::is_regular_file(path, statusError);
    if (statusError || !regularFile)
    {
        validation.status = DataFileStatus::invalid;
        appendValidation(result, std::move(validation));
        addDiagnostic(result,
                      Diagnostic::Severity::error,
                      role,
                      path,
                      statusError ? statusError.message() : "data path is not a regular file",
                      {"Choose an existing file or clear the override."});
        return;
    }

    validation.fileSize = std::filesystem::file_size(path, statusError);
    if (statusError)
    {
        validation.status = DataFileStatus::invalid;
        appendValidation(result, std::move(validation));
        addDiagnostic(result, Diagnostic::Severity::error, role, path, statusError.message());
        return;
    }

    if (!supportedRoles.at(roleIndex))
    {
        validation.status = DataFileStatus::unsupported;
        appendValidation(result, std::move(validation));
        addDiagnostic(result, Diagnostic::Severity::info, role, path, "recognized file is not consumed by the current GUI");
        return;
    }

    validation.status = profile.files.at(role).userOverride ? DataFileStatus::overridden : DataFileStatus::found;
    appendValidation(result, std::move(validation));
}

void validateKnownFiles(const DataProfile & profile, DataProfileValidation & result)
{
    result.files.reserve(knownDataFileRoles().size());
    for (const auto role : knownDataFileRoles())
    {
        validateFileRole(profile, result, role);
    }
}

void validateRadarcol(const DataProfile & profile, DataProfileValidation & result)
{
    auto * file = result.find(DataFileRole::radarcol);
    if (file == nullptr || file->status == DataFileStatus::missing || file->status == DataFileStatus::unsupported ||
        file->status == DataFileStatus::invalid)
    {
        return;
    }

    auto reader = uomul::RadarColReader::open(file->path);
    if (!reader)
    {
        file->status = DataFileStatus::invalid;
        addDiagnostic(result, Diagnostic::Severity::error, DataFileRole::radarcol, file->path, reader.error().message);
        return;
    }

    if (!profile.files.at(DataFileRole::radarcol).userOverride)
    {
        file->status = DataFileStatus::foundAndValidated;
    }
}

[[nodiscard]] bool isUnavailable(const DataFileValidation * file) noexcept
{
    return file == nullptr || file->status == DataFileStatus::missing || file->status == DataFileStatus::invalid;
}

void validateTexmaps(const DataProfile & profile, DataProfileValidation & result)
{
    auto * textureIndex = result.find(DataFileRole::texidx);
    auto * data = result.find(DataFileRole::texmaps);
    const bool hasIndex = !isUnavailable(textureIndex);
    const bool hasData = !isUnavailable(data);
    if (hasIndex == hasData)
    {
        if (!hasIndex)
        {
            return;
        }

        auto reader = uomul::TexMapsReader::open(textureIndex->path, data->path);
        if (!reader)
        {
            textureIndex->status = DataFileStatus::invalid;
            data->status = DataFileStatus::invalid;
            addDiagnostic(result, Diagnostic::Severity::error, std::nullopt, {}, reader.error().message);
            return;
        }

        if (!profile.files.at(DataFileRole::texidx).userOverride)
        {
            textureIndex->status = DataFileStatus::foundAndValidated;
        }
        if (!profile.files.at(DataFileRole::texmaps).userOverride)
        {
            data->status = DataFileStatus::foundAndValidated;
        }
        return;
    }

    const auto missingRole = hasIndex ? DataFileRole::texmaps : DataFileRole::texidx;
    addDiagnostic(result,
                  Diagnostic::Severity::error,
                  missingRole,
                  {},
                  "texture index and texture data must be configured as a pair",
                  {"Configure both texidx.mul and texmaps.mul."});
}

void validateMapRole(const DataProfile & profile, DataProfileValidation & result, std::size_t facetIndex)
{
    const auto role = static_cast<DataFileRole>(facetIndex);
    auto * file = result.find(role);
    if (isUnavailable(file))
    {
        return;
    }

    const auto & facet = profile.facets.at(facetIndex);
    if (!facet.dimensions)
    {
        addDiagnostic(result,
                      Diagnostic::Severity::info,
                      role,
                      file->path,
                      "map dimensions are not configured; structural validation is incomplete");
        return;
    }

    if (!dimensionsAreValid(*facet.dimensions))
    {
        addDiagnostic(result, Diagnostic::Severity::error, role, file->path, "map dimensions must be positive multiples of 8");
        file->status = DataFileStatus::invalid;
        return;
    }

    const uomul::OpenOptions options{.recoverTruncatedMap = facet.recoverTruncated};
    auto reader = uomul::MapReader::open(file->path, *facet.dimensions, options);
    if (!reader)
    {
        addDiagnostic(result, Diagnostic::Severity::error, role, file->path, reader.error().message);
        file->status = DataFileStatus::invalid;
        return;
    }

    if (!profile.files.at(role).userOverride)
    {
        file->status = DataFileStatus::foundAndValidated;
    }
}

void validateMaps(const DataProfile & profile, DataProfileValidation & result)
{
    for (std::size_t facetIndex = 0; facetIndex < mapFacetCount; facetIndex++)
    {
        validateMapRole(profile, result, facetIndex);
    }
}

void validateOutputFolder(const DataProfile & profile, DataProfileValidation & result)
{
    if (profile.outputRoot.empty() && !profile.useSourceFolderForOutput)
    {
        addDiagnostic(result,
                      Diagnostic::Severity::warning,
                      std::nullopt,
                      {},
                      "no output folder is configured",
                      {"Choose an output folder before starting an operation."});
    }

    if (profile.useSourceFolderForOutput && !profile.allowSourceOutputSame)
    {
        addDiagnostic(result,
                      Diagnostic::Severity::error,
                      std::nullopt,
                      profile.sourceRoot,
                      "using the source folder as output requires explicit confirmation");
    }

    if (profile.sourceRoot.empty() || (!profile.useSourceFolderForOutput && profile.outputRoot.empty()) ||
        profile.allowSourceOutputSame)
    {
        return;
    }

    std::error_code pathError;
    const auto source = std::filesystem::weakly_canonical(profile.sourceRoot, pathError);
    const auto output = profile.useSourceFolderForOutput
                            ? source
                            : std::filesystem::weakly_canonical(profile.outputRoot, pathError);
    if (!pathError && source == output)
    {
        addDiagnostic(result,
                      Diagnostic::Severity::error,
                      std::nullopt,
                      profile.outputRoot,
                      "source and output folders must be separate unless explicitly confirmed");
    }
}

} // namespace

DataFileValidation * DataProfileValidation::find(DataFileRole role) noexcept
{
    for (auto & file : files)
    {
        if (file.role == role)
        {
            return &file;
        }
    }
    return nullptr;
}

const DataFileValidation * DataProfileValidation::find(DataFileRole role) const noexcept
{
    for (const auto & file : files)
    {
        if (file.role == role)
        {
            return &file;
        }
    }
    return nullptr;
}

bool DataProfileValidation::hasErrors() const noexcept
{
    return std::ranges::any_of(diagnostics,
                               [](const Diagnostic & diagnostic)
                               { return diagnostic.severity == Diagnostic::Severity::error; });
}

DataProfileValidation validateDataProfile(const DataProfile & profile)
{
    DataProfileValidation result;
    validateKnownFiles(profile, result);
    validateRadarcol(profile, result);
    validateTexmaps(profile, result);
    validateMaps(profile, result);
    validateOutputFolder(profile, result);
    return result;
}

} // namespace uo_mule::gui
