#include "gui/ui/formatting/file_size.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <ios>
#include <sstream>
#include <string>

namespace uo_mule::gui
{

std::string formatFileSize(std::uintmax_t bytes)
{
    constexpr std::uintmax_t unit = 1000;
    constexpr std::uintmax_t displayThreshold = 10000;
    constexpr double noDecimalThreshold = 100.0;
    constexpr double oneDecimalThreshold = 10.0;
    constexpr std::array<const char *, 5> units{"B", "kB", "MB", "GB", "TB"};

    if (bytes < displayThreshold)
    {
        return std::to_string(bytes) + " B";
    }

    double value = static_cast<double>(bytes);
    std::size_t unitIndex = 0;
    while (value >= static_cast<double>(unit) && unitIndex + 1U < units.size())
    {
        value /= static_cast<double>(unit);
        unitIndex++;
    }

    std::ostringstream formatted;
    int precision = 2;
    if (value >= oneDecimalThreshold)
    {
        precision = value >= noDecimalThreshold ? 0 : 1;
    }
    formatted << std::fixed << std::setprecision(precision) << value << ' ' << units.at(unitIndex);
    return formatted.str();
}

} // namespace uo_mule::gui
