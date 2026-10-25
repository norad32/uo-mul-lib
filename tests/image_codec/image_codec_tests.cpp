#include "image_codec/image_codec.h"
#include "uomul/radarcol/radarcol.h"
#include "uomul/texmaps/texmaps.h"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
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

TEST_CASE("imageCodec_Should_ConvertRgbPixelsToTexmapRgb555")
{
    uo_mule::image::RgbImage image{
        .width = uomul::textureSmallDimension,
        .height = uomul::textureSmallDimension,
        .pixels = std::vector<std::uint8_t>(uomul::textureSmallPixelCount * 3U),
    };
    image.pixels.at(0) = fullChannel;

    const auto texture = uo_mule::image::toTexture(image);
    REQUIRE(texture);
    if (texture)
    {
        REQUIRE(texture->dimension == uomul::textureSmallDimension);
        REQUIRE(texture->pixels.size() == uomul::textureSmallPixelCount);
        REQUIRE(texture->pixels.front() == uomul::RadarColorConverter::fromRgb888(fullChannel, 0U, 0U));
    }
}

TEST_CASE("imageCodec_Should_RejectUnsupportedTextureDimensions")
{
    const uo_mule::image::RgbImage image{
        .width = 32U,
        .height = 32U,
        .pixels = std::vector<std::uint8_t>(static_cast<std::size_t>(32U) * 32U * 3U),
    };

    REQUIRE_FALSE(uo_mule::image::toTexture(image));
}
