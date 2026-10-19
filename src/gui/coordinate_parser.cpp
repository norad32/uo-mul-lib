#include "gui/coordinate_parser.h"
#include <charconv>
#include <cctype>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <cstddef>
#include <system_error>

namespace uo_mule::gui
{
namespace
{

[[nodiscard]] std::string trim(std::string_view input)
{
    while (!input.empty() && std::isspace(static_cast<unsigned char>(input.front())) != 0)
    {
        input.remove_prefix(1);
    }
    while (!input.empty() && std::isspace(static_cast<unsigned char>(input.back())) != 0)
    {
        input.remove_suffix(1);
    }
    return std::string(input);
}

[[nodiscard]] std::expected<std::uint32_t, std::string> parseNumber(std::string_view input)
{
    const std::string value = trim(input);
    if (value.empty())
    {
        return std::unexpected("coordinate values cannot be empty");
    }
    std::uint32_t number = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), number);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
    {
        return std::unexpected("coordinate values must be unsigned integers");
    }
    return number;
}

} // namespace

std::expected<ParsedCoordinate, std::string> parseCoordinate(std::string_view input, CoordinateKind kind)
{
    const std::string trimmed = trim(input);
    if (trimmed.empty())
    {
        return std::unexpected("coordinates must contain two values separated by whitespace");
    }

    const std::size_t separator = trimmed.find_first_of(" \t\n\r\f\v");
    if (separator == std::string_view::npos)
    {
        return std::unexpected("coordinates must contain two values separated by whitespace");
    }

    const auto x = parseNumber(std::string_view(trimmed).substr(0, separator));
    std::string_view remaining = std::string_view(trimmed).substr(separator + 1);
    while (!remaining.empty() && std::isspace(static_cast<unsigned char>(remaining.front())) != 0)
    {
        remaining.remove_prefix(1);
    }
    const std::size_t trailingWhitespace = remaining.find_first_of(" \t\n\r\f\v");
    const auto y = parseNumber(trailingWhitespace == std::string_view::npos
                                   ? remaining
                                   : remaining.substr(0, trailingWhitespace));
    if (!x || !y)
    {
        return std::unexpected(x ? y.error() : x.error());
    }
    if (trailingWhitespace != std::string_view::npos &&
        !trim(remaining.substr(trailingWhitespace + 1)).empty())
    {
        return std::unexpected("coordinates must contain exactly two values");
    }
    return ParsedCoordinate{.kind = kind, .value = {.x = *x, .y = *y}};
}

} // namespace uo_mule::gui
