#include "gui_model/profile/data_validation.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/source/tiledata_source.h"
#include "uomul/radarcol/radarcol.h"
#include "uomul/texmaps/texmaps.h"
#include <algorithm>
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
    if (path.empty())
    {
        validation.status = DataFileStatus::missing;
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

    validation.status = DataFileStatus::found;
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

void validateRadarcol(DataProfileValidation & result)
{
    auto * file = result.find(DataFileRole::radarcol);
    if (file == nullptr || file->status == DataFileStatus::missing || file->status == DataFileStatus::invalid)
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

    file->status = DataFileStatus::foundAndValidated;
}

[[nodiscard]] bool isUnavailable(const DataFileValidation * file) noexcept
{
    return file == nullptr || file->status == DataFileStatus::missing || file->status == DataFileStatus::invalid;
}

void validateTexmaps(DataProfileValidation & result)
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

        textureIndex->status = DataFileStatus::foundAndValidated;
        data->status = DataFileStatus::foundAndValidated;
        return;
    }

    const auto missingRole = hasIndex ? DataFileRole::texmaps : DataFileRole::texidx;
    addDiagnostic(result,
                  Diagnostic::Severity::error,
                  missingRole,
                  {},
                  "Texmaps file pset is not available",
                  {"Set both texidx.mul and texmaps.mul."});
}

void validateTiledata(const DataProfile & profile, DataProfileValidation & result)
{
    auto * file = result.find(DataFileRole::tiledata);
    if (file == nullptr || file->status == DataFileStatus::missing || file->status == DataFileStatus::invalid)
    {
        return;
    }

    if (!resolveTiledataLayout(file->path, profile.tiledataLayout))
    {
        file->status = DataFileStatus::invalid;
        addDiagnostic(result,
                      Diagnostic::Severity::error,
                      DataFileRole::tiledata,
                      file->path,
                      "Tiledata file does not match the selected layout",
                      {"Choose auto, legacy32, or extended64, or provide a complete Tiledata file."});
        return;
    }

    file->status = DataFileStatus::foundAndValidated;
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
    return std::ranges::any_of(
        diagnostics, [](const Diagnostic & diagnostic) { return diagnostic.severity == Diagnostic::Severity::error; });
}

DataProfileValidation validateDataProfile(const DataProfile & profile)
{
    DataProfileValidation result;
    validateKnownFiles(profile, result);
    validateRadarcol(result);
    validateTexmaps(result);
    validateTiledata(profile, result);
    if (profile.sourceRoot.empty() && profile.files.empty())
    {
        addDiagnostic(result,
                      Diagnostic::Severity::error,
                      std::nullopt,
                      {},
                      "profile does not define a metadata source",
                      {"Choose a metadata folder or set a file override."});
    }
    return result;
}

} // namespace uo_mule::gui
