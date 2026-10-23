#pragma once

#include <algorithm>
#include <array>
#include <filesystem>
#include <string_view>
#include <type_traits>

namespace uo_mule::gui
{

template <typename Character, std::size_t size>
void copyToBuffer(std::array<Character, size> & buffer, std::basic_string_view<Character> value)
{
    static_assert(std::is_same_v<Character, char>);
    static_assert(size > 0U);
    const std::size_t length = std::min(value.size(), buffer.size() - 1U);
    std::copy_n(value.begin(), length, buffer.begin());
    buffer.at(length) = Character{};
}

template <typename Character, std::size_t size>
void copyToBuffer(std::array<Character, size> & buffer, const std::filesystem::path & value)
{
    copyToBuffer(buffer, std::string_view(value.string()));
}

} // namespace uo_mule::gui
