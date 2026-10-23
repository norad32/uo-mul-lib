#include "app/cli.h"
#include "logger/logger.h"
#include "uomul/map.h"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

constexpr std::size_t mapBlockCount = 4;

class CliMapFixture
{
public:
    CliMapFixture()
    {
        path = std::filesystem::temp_directory_path() / "im-cpp-cli-map-tests";
        std::filesystem::remove_all(path);
        std::filesystem::create_directories(path);
        map = path / "map.mul";
        std::ofstream output(map, std::ios::binary);
        for (std::size_t block = 0; block < mapBlockCount; block++)
        {
            output.write("HEAD", 4);
            for (std::size_t tile = 0; tile < uomul::mapBlockTileCount; tile++)
            {
                const std::array<unsigned char, 3> record{static_cast<unsigned char>(tile), 0, 0};
                for (const auto byte : record)
                {
                    output.put(static_cast<char>(byte));
                }
            }
        }
    }

    CliMapFixture(const CliMapFixture &) = delete;
    CliMapFixture & operator=(const CliMapFixture &) = delete;
    CliMapFixture(CliMapFixture &&) = delete;
    CliMapFixture & operator=(CliMapFixture &&) = delete;

    ~CliMapFixture() { std::filesystem::remove_all(path); }

    std::filesystem::path path;
    std::filesystem::path map;
};

int run(const std::vector<std::string_view> & arguments, std::ostringstream & output, std::ostringstream & error)
{
    uo_mule::logger::Logger logger("map-cli-test");
    return uo_mule::app::runCli(arguments, output, error, logger);
}

} // namespace

TEST_CASE("MapCli_Should_InspectTileUsingExplicitDimensions")
{
    const CliMapFixture fixture;
    const auto map_path = fixture.map.string();
    const std::vector<std::string_view> arguments{
        "map",
        "inspect",
        "--map",
        map_path,
        "--width",
        "16",
        "--height",
        "16",
        "--tile-x",
        "0",
        "--tile-y",
        "0",
    };
    std::ostringstream output;
    std::ostringstream error;
    const int result = run(arguments, output, error);
    // NOLINTNEXTLINE(misc-include-cleaner)
    CAPTURE(result, output.str(), error.str());
    REQUIRE(result == 0);
    REQUIRE(output.str().find("tile 0,0 id=0 z=0") != std::string::npos);
    REQUIRE(error.str().empty());
}

TEST_CASE("MapCli_Should_RejectEditWithoutOperation")
{
    const CliMapFixture fixture;
    const auto map_path = fixture.map.string();
    const auto output_path = (fixture.path / "out.mul").string();
    const std::vector<std::string_view> arguments{
        "map",
        "edit",
        "--map",
        map_path,
        "--output",
        output_path,
        "--width",
        "16",
        "--height",
        "16",
    };
    std::ostringstream output;
    std::ostringstream error;
    const int result = run(arguments, output, error);
    // NOLINTNEXTLINE(misc-include-cleaner)
    CAPTURE(result, output.str(), error.str());
    REQUIRE(result == 2);
    REQUIRE(error.str().find("Specify --tile-x") != std::string::npos);
}
