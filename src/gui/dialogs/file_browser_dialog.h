#pragma once

#include "gui/profile/data_profile.h"
#include "uomul/map.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace uo_mule::gui
{

inline constexpr std::size_t fileBrowserBufferSize = 1024;

enum class FileBrowserMode : std::uint8_t
{
    openFile,
    saveFile,
    chooseDirectory,
};

enum class FileBrowserTarget : std::uint8_t
{
    genericOpen,
    mapOpen,
    newMapOutput,
    mapSaveAs,
    dataSourceRoot,
    dataSearchRoot,
    dataOutputRoot,
    dataOverride,
    radarcolSource,
    radarcolOutput,
    texmapsIndex,
    texmapsData,
    texmapsCompanion,
};

enum class MulAssetKind : std::uint8_t
{
    unknown,
    map,
    radarcol,
    texmapsPair,
    tiledata,
};

struct FileBrowserRequest
{
    FileBrowserMode mode = FileBrowserMode::openFile;
    FileBrowserTarget target = FileBrowserTarget::mapOpen;
    DataFileRole role = DataFileRole::radarcol;
    std::filesystem::path initialPath;
    std::string extension;
    std::string title;
};

struct FileBrowserMapCandidate
{
    std::string presetName;
    std::string facet;
    uomul::Dimensions dimensions{};
    std::uint64_t expectedBlocks = 0;
    std::uint64_t completeBlocks = 0;
    std::uint64_t missingBlocks = 0;
    std::uint64_t trailingBytes = 0;
    bool valid = false;
};

struct FileBrowserAssetCandidate
{
    // Candidate presentation follows one scheme: label, compact detail, warning, likelihood score.
    MulAssetKind kind = MulAssetKind::unknown;
    std::filesystem::path path;
    std::filesystem::path companionPath;
    std::string label;
    std::string facet;
    std::string reason;
    std::string warning;
    int score = 0;
    std::uint32_t itemCount = 0;
    std::optional<uomul::Dimensions> dimensions;
    bool validated = false;
    bool openable = false;
};

struct FileBrowserResult
{
    FileBrowserTarget target = FileBrowserTarget::mapOpen;
    DataFileRole role = DataFileRole::radarcol;
    std::filesystem::path path;
    std::vector<FileBrowserMapCandidate> mapCandidates;
    std::vector<FileBrowserAssetCandidate> assetCandidates;
    std::optional<FileBrowserAssetCandidate> assetCandidate;
};

struct FileBrowserSummary
{
    std::filesystem::path path;
    bool exists = false;
    bool regularFile = false;
    bool readable = false;
    std::optional<std::uintmax_t> size;
    std::string modified;
    std::string error;
    std::vector<FileBrowserMapCandidate> mapCandidates;
    std::vector<FileBrowserAssetCandidate> assetCandidates;
};

struct FileBrowserState
{
    bool visible = false;
    bool showHidden = false;
    FileBrowserRequest request{};
    std::filesystem::path directory;
    std::filesystem::path selectedPath;
    std::array<char, fileBrowserBufferSize> location{};
    std::array<char, fileBrowserBufferSize> filename{};
    std::vector<std::filesystem::path> history;
    std::size_t historyIndex = 0;
    FileBrowserSummary summary{};
    std::string error;
    std::optional<FileBrowserResult> result;
};

[[nodiscard]] bool fileBrowserMatchesFilter(const std::filesystem::path & path, std::string_view extension);
[[nodiscard]] FileBrowserSummary inspectFileBrowserSelection(const std::filesystem::path & path);
[[nodiscard]] FileBrowserAssetCandidate inspectTexmapsPair(const std::filesystem::path & indexPath,
                                                             const std::filesystem::path & dataPath);
void sortFileBrowserCandidates(std::vector<FileBrowserAssetCandidate> & candidates);

void requestFileBrowser(FileBrowserState & state, FileBrowserRequest request);
void cancelFileBrowser(FileBrowserState & state);
[[nodiscard]] std::optional<FileBrowserResult> consumeFileBrowserResult(FileBrowserState & state);
void drawFileBrowser(FileBrowserState & state);

} // namespace uo_mule::gui
