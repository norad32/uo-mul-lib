#pragma once

#include <span>
#include <string_view>
#include <vector>

namespace uo_mule::app
{

[[nodiscard]] std::vector<std::string_view> makeCliArguments(std::span<char *> arguments);

}
