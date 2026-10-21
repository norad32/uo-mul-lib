#pragma once

#include "gui/profile/data_profile.h"
#include <cstdint>
#include <optional>
#include <vector>

namespace uo_mule::gui
{

struct DataFileValidation
{
    DataFileRole role;
    DataFileStatus status = DataFileStatus::missing;
    std::filesystem::path path;
    std::optional<std::uintmax_t> fileSize;
};

struct DataProfileValidation
{
    std::vector<DataFileValidation> files;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] DataFileValidation * find(DataFileRole role) noexcept;
    [[nodiscard]] const DataFileValidation * find(DataFileRole role) const noexcept;
    [[nodiscard]] bool hasErrors() const noexcept;
};

[[nodiscard]] DataProfileValidation validateDataProfile(const DataProfile & profile);

} // namespace uo_mule::gui
