#pragma once

#include "uomul/map/map.h"
#include <filesystem>
#include <optional>

namespace uo_mule::gui
{

// Document-owned map data. The reader represents the current effective map.
// workingPath is present while unsaved changes are staged in a temporary map.
struct MapChanges
{
    std::optional<uomul::MapReader> reader;
    uomul::Dimensions dimensions{};
    std::filesystem::path mapPath;
    std::filesystem::path originalPath;
    std::optional<std::filesystem::path> workingPath;
    std::optional<uomul::Error> error;

    [[nodiscard]] bool dirty() const noexcept { return workingPath.has_value(); }
};

} // namespace uo_mule::gui
