#include "gui_model/source/source_validation.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/profile/data_validation.h"
#include "gui_model/source/source_identity.h"
#include "uomul/radarcol/radarcol.h"
#include "uomul/texmaps/texmaps.h"
#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace uo_mule::gui
{
namespace
{

[[nodiscard]] SourceValidationStatus mapStatus(DataFileStatus status) noexcept
{
    switch (status)
    {
        case DataFileStatus::missing :
            return SourceValidationStatus::missing;
        case DataFileStatus::invalid :
        case DataFileStatus::ambiguous :
            return SourceValidationStatus::invalid;
        case DataFileStatus::unsupported :
            return SourceValidationStatus::unsupported;
        case DataFileStatus::found :
        case DataFileStatus::foundAndValidated :
        case DataFileStatus::overridden :
        case DataFileStatus::optional :
            return SourceValidationStatus::valid;
    }
    return SourceValidationStatus::invalid;
}

[[nodiscard]] std::string_view diagnosticFor(const DataFileValidation & validation)
{
    switch (validation.status)
    {
        case DataFileStatus::missing :
            return "file is not available";
        case DataFileStatus::invalid :
            return "file is invalid";
        case DataFileStatus::unsupported :
            return "file is unsupported";
        default :
            return {};
    }
}

} // namespace

SourceValidationResult toSourceValidation(const DataFileValidation & validation, const std::string & diagnostic)
{
    return {
        .role = validation.role,
        .canonicalPath = canonicalSourcePath(validation.path),
        .status = mapStatus(validation.status),
        .byteCount = validation.fileSize,
        .recordCount = std::nullopt,
        .diagnostic = diagnostic.empty() ? std::string(diagnosticFor(validation)) : diagnostic,
    };
}

std::vector<SourceValidationResult> validateDataProfileSources(const DataProfile & profile)
{
    const DataProfileValidation validation = validateDataProfile(profile);
    std::vector<SourceValidationResult> result;
    result.reserve(validation.files.size());
    for (const auto & file : validation.files)
    {
        result.push_back(toSourceValidation(file));
        if (file.role == DataFileRole::radarcol && !file.path.empty() && result.back().status == SourceValidationStatus::valid)
        {
            const auto reader = uomul::RadarColReader::open(file.path);
            if (reader)
            {
                result.back().recordCount = reader->colorCount();
            }
        }
    }

    const auto index = std::ranges::find_if(result, [](const auto & item) { return item.role == DataFileRole::texidx; });
    const auto data = std::ranges::find_if(result, [](const auto & item) { return item.role == DataFileRole::texmaps; });
    if (index != result.end() && data != result.end())
    {
        const bool indexAvailable = index->status == SourceValidationStatus::valid;
        const bool dataAvailable = data->status == SourceValidationStatus::valid;
        if (indexAvailable != dataAvailable)
        {
            auto & incomplete = indexAvailable ? *data : *index;
            incomplete.status = SourceValidationStatus::incompletePair;
            incomplete.diagnostic = std::string(dataFileRoleName(incomplete.role)) + " role: Texmaps file set is not available";
        }
        else if (indexAvailable && dataAvailable)
        {
            const auto pair = uomul::TexMapsReader::open(index->canonicalPath, data->canonicalPath);
            if (!pair)
            {
                index->status = SourceValidationStatus::invalid;
                data->status = SourceValidationStatus::invalid;
                index->diagnostic = "texidx role: " + pair.error().message;
                data->diagnostic = "texmaps role: " + pair.error().message;
            }
            else
            {
                index->recordCount = pair->textureCount();
                data->recordCount = pair->textureCount();
            }
        }
    }
    return result;
}

} // namespace uo_mule::gui
