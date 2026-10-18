#pragma once

#include "uomul/radarcol/radarcol.h"
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

namespace uo_mule::gui
{

inline constexpr std::size_t radarcolPathBufferSize = 1024;
inline constexpr std::size_t radarcolPageColorCount = 256;
inline constexpr std::uint32_t unloadedRadarcolPage = std::numeric_limits<std::uint32_t>::max();

struct RadarcolPanelState
{
    std::array<char, radarcolPathBufferSize> sourcePath{};
    std::array<char, radarcolPathBufferSize> outputPath{};
    std::array<uomul::RadarColor, radarcolPageColorCount> pageColors{};
    std::array<bool, radarcolPageColorCount> pageColorAvailable{};
    std::array<float, 3> pickerColor{};
    std::optional<uomul::RadarColReader> reader;
    std::optional<uomul::RadarColEditor> editor;
    std::string error;
    std::uint32_t page = 0;
    std::uint32_t pageCount = 0;
    std::uint32_t loadedPageIndex = unloadedRadarcolPage;
    std::uint32_t firstColor = 0;
    bool staticColors = false;
    bool overwriteOutput = false;
};

void drawRadarcolPanel(RadarcolPanelState & state, bool editMode, bool & visible);

} // namespace uo_mule::gui
