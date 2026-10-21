#pragma once

#include "gui/data_profile.h"
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
    mapOpen,
    newMapOutput,
    mapSaveAs,
    dataSourceRoot,
    dataOutputRoot,
    dataOverride,
    radarcolSource,
    radarcolOutput,
    texmapsIndex,
    texmapsData,
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

struct FileBrowserResult
{
    FileBrowserTarget target = FileBrowserTarget::mapOpen;
    DataFileRole role = DataFileRole::radarcol;
    std::filesystem::path path;
    std::vector<FileBrowserMapCandidate> mapCandidates;
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

void requestFileBrowser(FileBrowserState & state, FileBrowserRequest request);
void cancelFileBrowser(FileBrowserState & state);
[[nodiscard]] std::optional<FileBrowserResult> consumeFileBrowserResult(FileBrowserState & state);
void drawFileBrowser(FileBrowserState & state);

} // namespace uo_mule::gui
