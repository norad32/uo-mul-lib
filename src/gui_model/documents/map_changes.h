#pragma once

#include "uomul/map/map.h"
#include <filesystem>
#include <optional>

namespace uo_mule::gui
{

// Document-owned map data. The reader represents the current effective map.
// workingPath_ is present while unsaved changes are staged in a temporary map.
struct MapChanges
{
    std::optional<uomul::MapReader> reader_;
    uomul::Dimensions dimensions_{};
    std::filesystem::path mapPath_;
    std::filesystem::path originalPath_;
    std::optional<std::filesystem::path> workingPath_;
    std::optional<uomul::Error> error_;

    [[nodiscard]] bool dirty() const noexcept { return workingPath_.has_value(); }
};

} // namespace uo_mule::gui
