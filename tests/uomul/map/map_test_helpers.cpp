#include "map_test_helpers.h"
#include "uomul/map/map.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>

namespace
{

constexpr std::size_t landHeaderByteCount = 4;
constexpr std::size_t tileRecordByteCount = 3;
constexpr std::uint32_t tilesPerBlock = uomul::mapBlockDimension * uomul::mapBlockDimension;
constexpr std::uint32_t fixtureBlockIdStride = 100;
constexpr std::uint32_t lowByteMask = 0xffU;

} // namespace

namespace uomul::test
{

void writeFixture(const std::filesystem::path & path, std::uint32_t blocks)
{
    std::ofstream output(path, std::ios::binary);
    for (std::uint32_t block = 0; block < blocks; block++)
    {
        const std::array<char, landHeaderByteCount> header{
            static_cast<char>(block + 1),
            static_cast<char>(block + 2),
            static_cast<char>(block + 3),
            static_cast<char>(block + 4),
        };
        output.write(header.data(), static_cast<std::streamsize>(header.size()));
        for (std::uint32_t tile = 0; tile < tilesPerBlock; tile++)
        {
            const auto id = static_cast<std::uint16_t>((block * fixtureBlockIdStride) + tile);
            const std::array<char, tileRecordByteCount> record{
                static_cast<char>(id & lowByteMask),
                static_cast<char>(static_cast<std::uint16_t>(id / 256)),
                static_cast<char>(tile == 0 ? lowByteMask : tile),
            };
            output.write(record.data(), static_cast<std::streamsize>(record.size()));
        }
    }
}

} // namespace uomul::test
