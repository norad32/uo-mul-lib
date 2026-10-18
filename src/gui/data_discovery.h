#pragma once

#include "gui/data_profile.h"
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace uo_mule::gui
{

struct DiscoveryCandidate
{
    std::filesystem::path path;
    int score = 0;
    std::size_t directoryDepth = 0;
    bool exactFilename = false;
    bool knownLayout = false;
    bool valid = true;
    std::string validationMessage;
};

struct DiscoveredDataFile
{
    DataFileRole role;
    DataFileStatus status = DataFileStatus::missing;
    std::vector<DiscoveryCandidate> candidates;
    std::optional<std::size_t> selectedCandidate;
};

struct DataDiscoveryResult
{
    std::filesystem::path sourceRoot;
    DiscoveryMode mode = DiscoveryMode::none;
    std::vector<DiscoveredDataFile> files;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] DiscoveredDataFile * find(DataFileRole role) noexcept;
    [[nodiscard]] const DiscoveredDataFile * find(DataFileRole role) const noexcept;
};

struct DataDiscoveryOptions
{
    bool validateCandidates = true;
};

[[nodiscard]] DataDiscoveryResult discoverDataFiles(const std::filesystem::path & root,
                                                    DiscoveryMode mode,
                                                    DataDiscoveryOptions options = {});
void applyDiscovery(DataProfile & profile, const DataDiscoveryResult & result);

} // namespace uo_mule::gui
