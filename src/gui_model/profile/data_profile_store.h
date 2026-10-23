#pragma once

#include "gui_model/profile/data_profile.h"
#include <filesystem>
#include <string_view>
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

class DataProfileStore
{
public:
    explicit DataProfileStore(std::filesystem::path directory = {});
    [[nodiscard]] DataProfileLoadResult load(std::string_view profileName) const;
    void save(const DataProfile & profile) const;
    void remove(std::string_view profileName) const;
    [[nodiscard]] std::filesystem::path path(std::string_view profileName) const;

private:
    std::filesystem::path directory_;
};

[[nodiscard]] DataProfileLoadResult loadDataProfile(const std::filesystem::path & path);
void saveDataProfile(const DataProfile & profile, const std::filesystem::path & path);

} // namespace uo_mule::gui
