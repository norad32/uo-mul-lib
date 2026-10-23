#pragma once

#include "gui_model/persistence/preferences.h"
#include <filesystem>

namespace uo_mule::gui
{

[[nodiscard]] Preferences loadPreferences(const std::filesystem::path & path);
void savePreferences(const Preferences & preferences, const std::filesystem::path & path);

} // namespace uo_mule::gui
