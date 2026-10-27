#pragma once

#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>

namespace uo_mule::gui
{

[[nodiscard]] inline bool hasSchemaVersion(std::string_view line, std::uint32_t expectedVersion)
{
    std::istringstream input{std::string(line)};
    std::string key;
    std::uint32_t version = 0;
    if (!(input >> key >> version))
    {
        return false;
    }
    input >> std::ws;
    return key == "schema_version" && version == expectedVersion && input.eof();
}

} // namespace uo_mule::gui
