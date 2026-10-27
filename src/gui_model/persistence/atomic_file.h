#pragma once

#include <filesystem>
#include <string_view>

namespace uo_mule::gui
{

void writeFileAtomically(const std::filesystem::path & path, std::string_view contents, std::string_view description);

} // namespace uo_mule::gui
