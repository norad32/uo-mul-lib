#pragma once

#include "gui_model/persistence/preferences.h"
#include <filesystem>
#include <ostream>
#include <string_view>

namespace uo_mule::gui
{

[[nodiscard]] Preferences loadPreferences(const std::filesystem::path & path);
void savePreferences(const Preferences & preferences, const std::filesystem::path & path);
void applySerializedPreference(Preferences & preferences, std::string_view key, std::string_view value);
void appendSerializedPreferences(std::ostream & output, const Preferences & preferences, std::string_view prefix = {});

} // namespace uo_mule::gui
