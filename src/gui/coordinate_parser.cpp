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

constexpr std::size_t blockPrefixLength = 6;

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

std::expected<ParsedCoordinate, std::string> parseCoordinate(std::string_view input)
{
    CoordinateKind kind = CoordinateKind::tile;
    const std::string trimmed = trim(input);
    std::string_view coordinates = trimmed;
    if (coordinates.starts_with("block:"))
    {
        kind = CoordinateKind::block;
        coordinates.remove_prefix(blockPrefixLength);
    }

    const std::size_t separator = coordinates.find(',');
    if (separator == std::string_view::npos)
    {
        return std::unexpected("coordinates must use x,y or block:x,y");
    }

    const auto x = parseNumber(coordinates.substr(0, separator));
    const auto y = parseNumber(coordinates.substr(separator + 1));
    if (!x || !y)
    {
        return std::unexpected(x ? y.error() : x.error());
    }
    return ParsedCoordinate{.kind = kind, .value = {.x = *x, .y = *y}};
}

} // namespace uo_mule::gui
