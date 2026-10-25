#include "image_codec/image_codec.h"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace
{

constexpr std::uint8_t fullChannel = 0xFFU;
std::atomic<std::uint64_t> testDirectoryCounter = 0;

class TestDirectory
{
public:
    TestDirectory(const TestDirectory &) = delete;
    TestDirectory & operator=(const TestDirectory &) = delete;
    TestDirectory(TestDirectory &&) = delete;
    TestDirectory & operator=(TestDirectory &&) = delete;

    TestDirectory()
    {
        const auto sequence = testDirectoryCounter.fetch_add(1);
        path_ = std::filesystem::temp_directory_path() / ("image-codec-tests-" + std::to_string(sequence));
        std::filesystem::create_directories(path_);
    }

    ~TestDirectory()
    {
        std::error_code ignoredError;
        std::filesystem::remove_all(path_, ignoredError);
    }

    [[nodiscard]] const std::filesystem::path & path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

[[nodiscard]] std::vector<std::uint8_t> samplePixels()
{
    return {
        fullChannel,
        0x00U,
        0x00U,
        0x00U,
        fullChannel,
        0x00U,
        0x00U,
        0x00U,
        fullChannel,
        fullChannel,
        fullChannel,
        fullChannel,
    };
}

} // namespace

TEST_CASE("imageCodec_Should_RoundTripRgbPixelsThroughBmp")
{
    const TestDirectory directory;
    const auto path = directory.path() / "sample.bmp";
    const auto pixels = samplePixels();
    REQUIRE(uo_mule::image::writeBmp(path, 2U, 2U, pixels));

    const auto decoded = uo_mule::image::decode(path);
    REQUIRE(decoded);
    if (decoded)
    {
        REQUIRE(decoded->width == 2U);
        REQUIRE(decoded->height == 2U);
        REQUIRE(decoded->pixels == pixels);
    }
}

TEST_CASE("imageCodec_Should_RoundTripRgbPixelsThroughPng")
{
    const TestDirectory directory;
    const auto path = directory.path() / "sample.png";
    const auto pixels = samplePixels();
    REQUIRE(uo_mule::image::writePng(path, 2U, 2U, pixels));

    const auto decoded = uo_mule::image::decode(path);
    REQUIRE(decoded);
    if (decoded)
    {
        REQUIRE(decoded->width == 2U);
        REQUIRE(decoded->height == 2U);
        REQUIRE(decoded->pixels == pixels);
    }
}

TEST_CASE("imageCodec_Should_RejectMismatchedRgbPixelCount")
{
    const TestDirectory directory;
    const auto path = directory.path() / "invalid.png";
    REQUIRE_FALSE(uo_mule::image::writePng(path, 2U, 2U, std::vector<std::uint8_t>{0U, 1U, 2U}));
    REQUIRE_FALSE(std::filesystem::exists(path));
}
