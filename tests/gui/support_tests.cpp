#include "gui/support/bounded_buffer.h"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <string_view>

TEST_CASE("copyToBuffer_Should_NullTerminateAndTruncate")
{
    std::array<char, 5> buffer{};

    uo_mule::gui::copyToBuffer(buffer, std::string_view("abcdef"));

    REQUIRE(std::string_view(buffer.data()) == "abcd");
}

TEST_CASE("copyToBuffer_Should_ClearPreviousContentsForEmptyInput")
{
    std::array<char, 5> buffer{'x', 'x', 'x', 'x', '\0'};

    uo_mule::gui::copyToBuffer(buffer, std::string_view{});

    REQUIRE(buffer.front() == '\0');
}
