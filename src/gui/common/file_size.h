#pragma once

#include <cstdint>
#include <string>

namespace uo_mule::gui
{

[[nodiscard]] std::string formatFileSize(std::uintmax_t bytes);

} // namespace uo_mule::gui
