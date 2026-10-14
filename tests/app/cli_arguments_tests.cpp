#include "app/cli_arguments.h"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <string_view>

TEST_CASE("makeCliArguments_Should_ExcludeExecutableAndPreserveOrder")
{
    std::array<char, 4> executable{"app"};
    std::array<char, 6> first{"check"};
    std::array<char, 10> second{"--verbose"};
    std::array<char *, 3> arguments{executable.data(), first.data(), second.data()};

    const auto parsed = im_cpp::app::makeCliArguments(arguments);

    REQUIRE(parsed.size() == 2);
    REQUIRE(parsed[0] == std::string_view("check"));
    REQUIRE(parsed[1] == std::string_view("--verbose"));
}
