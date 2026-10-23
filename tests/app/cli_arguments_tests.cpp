#include "app/cli_arguments.h"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <string_view>

TEST_CASE("makeCliArguments_Should_ExcludeExecutableAndPreserveOrder")
{
    constexpr std::size_t checkArgumentBufferSize = std::string_view{"check"}.size() + 1;
    constexpr std::size_t verboseArgumentBufferSize = std::string_view{"--verbose"}.size() + 1;
    std::array<char, 4> executable{"app"};
    std::array<char, checkArgumentBufferSize> first{"check"};
    std::array<char, verboseArgumentBufferSize> second{"--verbose"};
    std::array<char *, 3> arguments{executable.data(), first.data(), second.data()};

    const auto parsed = uo_mule::app::makeCliArguments(arguments);

    REQUIRE(parsed.size() == 2);
    REQUIRE(parsed.at(0) == std::string_view("check"));
    REQUIRE(parsed.at(1) == std::string_view("--verbose"));
}
