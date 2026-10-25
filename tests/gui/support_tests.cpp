#include "gui/presentation/bounded_buffer.h"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <string_view>

TEST_CASE("copyToBuffer_Should_NullTerminateAndTruncate")
{
    std::array<char, 5> buffer{}; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    uo_mule::gui::copyToBuffer(buffer, std::string_view("abcdef"));

    REQUIRE(std::string_view(buffer.data()) == "abcd");
}

TEST_CASE("copyToBuffer_Should_ClearPreviousContentsForEmptyInput")
{
    constexpr std::size_t bufferSize = 5;
    std::array<char, bufferSize> buffer{'x', 'x', 'x', 'x', '\0'};

    uo_mule::gui::copyToBuffer(buffer, std::string_view{});

    REQUIRE(buffer.front() == '\0');
}
