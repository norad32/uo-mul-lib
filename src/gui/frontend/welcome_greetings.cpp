#include "gui/frontend/welcome_greetings.h"
#include <cstddef>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace uo_mule::gui
{

const std::vector<std::string_view> & welcomeGreetings() noexcept
{
    static const std::vector<std::string_view> greetings{
        "I promise to take good care of you.",
        "I've been searching for a companion like you.",
        "Will you travel with me, noble creature?",
        "If you accompany me, we will protect one another.",
        "I've always wanted a pet like you.",
        "Don't be afraid.",
        "I won't hurt you.",

    };
    return greetings;
}

std::string chooseWelcomeGreeting(std::mt19937 & generator)
{
    const auto & greetings = welcomeGreetings();
    if (greetings.empty())
    {
        return "Welcome to UO Mule";
    }

    std::uniform_int_distribution<std::size_t> distribution(0, greetings.size() - 1U);
    return std::string(greetings.at(distribution(generator)));
}

} // namespace uo_mule::gui
