#pragma once

#include "gui/data_profile.h"
#include <filesystem>
#include <vector>

namespace uo_mule::gui
{

struct DataProfileLoadResult
{
    DataProfile profile;
    std::vector<Diagnostic> diagnostics;
};

[[nodiscard]] std::filesystem::path dataProfilesDirectory();
[[nodiscard]] std::filesystem::path dataProfilePath(std::string_view profileName, const std::filesystem::path & directory = {});
[[nodiscard]] DataProfileLoadResult loadDataProfile(const std::filesystem::path & path);
void saveDataProfile(const DataProfile & profile, const std::filesystem::path & path);

} // namespace uo_mule::gui
