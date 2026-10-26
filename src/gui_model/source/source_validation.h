#pragma once

#include "gui_model/profile/data_profile.h"
#include "gui_model/profile/data_validation.h"
#include "gui_model/source/source_identity.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace uo_mule::gui
{

enum class SourceValidationStatus : std::uint8_t
{
    missing,
    invalid,
    incompleteSet,
    valid,
    unsupported,
};

struct SourceValidationResult
{
    DataFileRole role = DataFileRole::radarcol;
    std::filesystem::path canonicalPath;
    SourceValidationStatus status = SourceValidationStatus::missing;
    std::optional<std::uintmax_t> byteCount;
    std::optional<std::uint64_t> recordCount;
    std::string diagnostic;
};

[[nodiscard]] SourceValidationResult toSourceValidation(const DataFileValidation & validation,
                                                        const std::string & diagnostic = {});
[[nodiscard]] std::vector<SourceValidationResult> validateDataProfileSources(const DataProfile & profile);

} // namespace uo_mule::gui
