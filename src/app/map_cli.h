#pragma once

#include <CLI/CLI.hpp>
#include <cstdint>
#include <iosfwd>
#include <string>

namespace im_cpp::app
{

struct MapCommandOptions
{
    std::string operation;
    std::string path;
    std::string outputPath;
    std::string otherPath;
    std::string reportPath;
    std::string diffDirectory;
    std::string facet;
    std::uint32_t mapIndex = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t regionWidth = 0;
    std::uint32_t regionHeight = 0;
    std::uint32_t blockX = 0;
    std::uint32_t blockY = 0;
    std::uint32_t tileX = 0;
    std::uint32_t tileY = 0;
    std::uint32_t landId = 0;
    std::int32_t z = 0;
    std::int32_t fromZ = 0;
    std::int32_t toZ = 0;
    std::uint32_t fromId = 0;
    std::uint32_t toId = 0;
    bool overwrite = false;
    bool recover = false;
    bool fillRegion = false;
    bool useBlock = false;
    bool useTile = false;
    bool replaceIdsSelected = false;
    bool replaceZSelected = false;
};

[[nodiscard]] CLI::App * configureMapCommand(CLI::App & application, MapCommandOptions & options);
[[nodiscard]] int runMapCommand(const MapCommandOptions & options, std::ostream & output, std::ostream & error);

} // namespace im_cpp::app
