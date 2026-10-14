#pragma once

#include "gui/preferences.h"
#include <filesystem>

namespace im_cpp::gui
{

[[nodiscard]] Preferences loadPreferences(const std::filesystem::path & path);
void savePreferences(const Preferences & preferences, const std::filesystem::path & path);

} // namespace im_cpp::gui
