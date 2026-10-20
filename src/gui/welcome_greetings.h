#pragma once

#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace uo_mule::gui
{

[[nodiscard]] const std::vector<std::string_view> & welcomeGreetings() noexcept;
[[nodiscard]] std::string chooseWelcomeGreeting(std::mt19937 & generator);

} // namespace uo_mule::gui
