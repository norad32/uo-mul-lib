#pragma once

#include "gui_model/profile/data_profile.h"
#include "uomul/tiledata/tiledata.h"
#include <filesystem>
#include <optional>

namespace uo_mule::gui
{

[[nodiscard]] std::optional<uomul::TiledataLayout> resolveTiledataLayout(const std::filesystem::path & path,
                                                                         TiledataLayoutSelection selection);

} // namespace uo_mule::gui
