#pragma once

#include "gui/data_discovery.h"
#include "gui/data_profile.h"
#include "gui/data_validation.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace uo_mule::gui
{

inline constexpr std::size_t dataPathsDialogBufferSize = 1024;

enum class DataPathPickerMode : std::uint8_t
{
    none,
    sourceFolder,
    outputFolder,
    fileOverride,
};

struct DataPathPickerState
{
    DataPathPickerMode mode = DataPathPickerMode::none;
    bool openRequested = false;
    DataFileRole role = DataFileRole::radarcol;
    std::filesystem::path directory;
    std::filesystem::path selectedPath;
    std::array<char, dataPathsDialogBufferSize> location{};
    std::string error;
};

struct DataPathsDialogState
{
    bool visible = false;
    bool initialized = false;
    std::array<char, dataPathsDialogBufferSize> profileName{};
    std::array<char, dataPathsDialogBufferSize> sourceRoot{};
    std::array<char, dataPathsDialogBufferSize> outputRoot{};
    std::map<DataFileRole, std::array<char, dataPathsDialogBufferSize>> overrides;
    std::vector<std::filesystem::path> profiles;
    DataProfile workingProfile;
    DataDiscoveryResult discovery;
    DataProfileValidation validation;
    DataPathPickerState picker;
    DiscoveryMode discoveryMode = DiscoveryMode::folder;
    std::string message;
};

void requestDataPathsDialog(DataPathsDialogState & state, const DataProfile & profile);
[[nodiscard]] bool drawDataPathsDialog(DataPathsDialogState & state, DataProfile & activeProfile);

} // namespace uo_mule::gui
