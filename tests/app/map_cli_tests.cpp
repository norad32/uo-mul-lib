#include "app/cli.h"
#include "core/logger.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

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
        for (int block = 0; block < 4; ++block)
        {
            output.write("HEAD", 4);
            for (int tile = 0; tile < 64; ++tile)
            {
                const std::array<unsigned char, 3> record{static_cast<unsigned char>(tile), 0, 0};
                output.write(reinterpret_cast<const char *>(record.data()), static_cast<std::streamsize>(record.size()));
            }
        }
    }

    ~CliMapFixture() { std::filesystem::remove_all(path); }

    std::filesystem::path path;
    std::filesystem::path map;
};

int run(const std::vector<std::string_view> & arguments, std::ostringstream & output, std::ostringstream & error)
{
    im_cpp::core::Logger logger("map-cli-test");
    return im_cpp::app::runCli(arguments, output, error, logger);
}

} // namespace

TEST_CASE("MapCli_Should_InspectTileUsingExplicitDimensions")
{
    CliMapFixture fixture;
    const auto map_path = fixture.map.string();
    const std::vector<std::string_view> arguments{
        "map", "inspect", "--map", map_path, "--width", "16", "--height", "16", "--tile-x", "0", "--tile-y", "0"};
    std::ostringstream output;
    std::ostringstream error;
    const int result = run(arguments, output, error);
    CAPTURE(result, output.str(), error.str());
    REQUIRE(result == 0);
    REQUIRE(output.str().find("tile 0,0 id=0 z=0") != std::string::npos);
    REQUIRE(error.str().empty());
}

TEST_CASE("MapCli_Should_RejectEditWithoutOperation")
{
    CliMapFixture fixture;
    const auto map_path = fixture.map.string();
    const auto output_path = (fixture.path / "out.mul").string();
    const std::vector<std::string_view> arguments{
        "map", "edit", "--map", map_path, "--output", output_path, "--width", "16", "--height", "16"};
    std::ostringstream output;
    std::ostringstream error;
    const int result = run(arguments, output, error);
    CAPTURE(result, output.str(), error.str());
    REQUIRE(result == 2);
    REQUIRE(error.str().find("Specify --tile-x") != std::string::npos);
}
