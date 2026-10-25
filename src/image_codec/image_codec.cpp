#include "image_codec/image_codec.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/radarcol/radarcol.h"
#include "uomul/texmaps/texmaps.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace uo_mule::image
{

namespace
{

[[nodiscard]] uomul::Error makeError(const std::filesystem::path & path, uomul::ErrorCode code, std::string message)
{
    return {.code = code, .path = path, .message = std::move(message)};
}

[[nodiscard]] std::expected<std::size_t, uomul::Error>
expectedPixelBytes(const std::filesystem::path & path, std::uint32_t width, std::uint32_t height)
{
    constexpr std::size_t channelCount = 3U;
    if (width == 0U || height == 0U)
    {
        return std::unexpected(makeError(path, uomul::ErrorCode::invalidDimensions, "image dimensions must be nonzero"));
    }

    const auto pixels = static_cast<std::uint64_t>(width) * height;
    if (pixels > std::numeric_limits<std::size_t>::max() / channelCount)
    {
        return std::unexpected(makeError(path, uomul::ErrorCode::invalidDimensions, "image dimensions are too large"));
    }
    return static_cast<std::size_t>(pixels) * channelCount;
}

[[nodiscard]] std::expected<void, uomul::Error> validateOutput(const std::filesystem::path & path,
                                                               std::uint32_t width,
                                                               std::uint32_t height,
                                                               std::span<const std::uint8_t> pixels)
{
    auto expected = expectedPixelBytes(path, width, height);
    if (!expected)
    {
        return std::unexpected(expected.error());
    }

    if (pixels.size() != *expected)
    {
        return std::unexpected(
            makeError(path, uomul::ErrorCode::invalidFormat, "RGB pixel count does not match image dimensions"));
    }

    if (width > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
        height > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
        width > static_cast<std::uint32_t>(std::numeric_limits<int>::max() / 3))
    {
        return std::unexpected(makeError(path, uomul::ErrorCode::invalidDimensions, "image dimensions exceed codec limits"));
    }

    return {};
}

[[nodiscard]] std::expected<std::vector<unsigned char>, uomul::Error> readFile(const std::filesystem::path & path)
{
    std::error_code fileError;
    const auto size = std::filesystem::file_size(path, fileError);
    if (fileError)
    {
        return std::unexpected(makeError(path, uomul::ErrorCode::io, fileError.message()));
    }

    if (size > static_cast<std::uintmax_t>(std::numeric_limits<int>::max()))
    {
        return std::unexpected(makeError(path, uomul::ErrorCode::invalidFormat, "image file exceeds codec limits"));
    }

    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return std::unexpected(makeError(path, uomul::ErrorCode::io, "could not open image file"));
    }

    std::vector<char> rawBytes(static_cast<std::size_t>(size));
    if (!rawBytes.empty())
    {
        input.read(rawBytes.data(), static_cast<std::streamsize>(rawBytes.size()));
        if (!input)
        {
            return std::unexpected(makeError(path, uomul::ErrorCode::io, "could not read image file"));
        }
    }

    std::vector<unsigned char> bytes;
    bytes.reserve(rawBytes.size());
    for (const char byte : rawBytes)
    {
        bytes.push_back(static_cast<unsigned char>(byte));
    }

    return bytes;
}

[[nodiscard]] std::expected<void, uomul::Error> writeImage(const std::filesystem::path & path,
                                                           std::uint32_t width,
                                                           std::uint32_t height,
                                                           std::span<const std::uint8_t> pixels,
                                                           bool png)
{
    if (auto valid = validateOutput(path, width, height, pixels); !valid)
    {
        return std::unexpected(valid.error());
    }

    const auto filePath = path.string();
    const auto widthValue = static_cast<int>(width);
    const auto heightValue = static_cast<int>(height);
    const auto stride = static_cast<int>(width * 3U);
    const int written = png ? stbi_write_png(filePath.c_str(), widthValue, heightValue, 3, pixels.data(), stride)
                            : stbi_write_bmp(filePath.c_str(), widthValue, heightValue, 3, pixels.data());
    if (written == 0)
    {
        return std::unexpected(makeError(path, uomul::ErrorCode::io, "image encoder could not write output"));
    }

    return {};
}

} // namespace

std::expected<RgbImage, uomul::Error> decode(const std::filesystem::path & inputPath)
{
    auto bytes = readFile(inputPath);
    if (!bytes)
    {
        return std::unexpected(bytes.error());
    }

    int width = 0;
    int height = 0;
    const auto byteCount = static_cast<int>(bytes->size());
    const std::unique_ptr<unsigned char, decltype(&stbi_image_free)> decoded(
        stbi_load_from_memory(bytes->data(), byteCount, &width, &height, nullptr, 3), stbi_image_free);
    if (!decoded)
    {
        const char * reason = stbi_failure_reason();
        return std::unexpected(
            makeError(inputPath, uomul::ErrorCode::invalidFormat, reason == nullptr ? "image decoder rejected input" : reason));
    }

    if (width <= 0 || height <= 0)
    {
        return std::unexpected(makeError(inputPath, uomul::ErrorCode::invalidDimensions, "image dimensions are invalid"));
    }

    const auto outputWidth = static_cast<std::uint32_t>(width);
    const auto outputHeight = static_cast<std::uint32_t>(height);

    auto pixelBytes = expectedPixelBytes(inputPath, outputWidth, outputHeight);
    if (!pixelBytes)
    {
        return std::unexpected(pixelBytes.error());
    }

    RgbImage image{.width = outputWidth, .height = outputHeight, .pixels = std::vector<std::uint8_t>(*pixelBytes)};
    std::copy_n(decoded.get(), *pixelBytes, image.pixels.begin());
    return image;
}

std::expected<uomul::TextureImage, uomul::Error> toTexture(const RgbImage & image, const std::filesystem::path & sourcePath)
{
    if (image.width != image.height ||
        (image.width != uomul::textureSmallDimension && image.width != uomul::textureLargeDimension))
    {
        return std::unexpected(
            makeError(sourcePath, uomul::ErrorCode::invalidDimensions, "texture image dimensions must be 64x64 or 128x128"));
    }

    const auto expectedPixels = static_cast<std::size_t>(image.width) * image.height * 3U;
    if (image.pixels.size() != expectedPixels)
    {
        return std::unexpected(
            makeError(sourcePath, uomul::ErrorCode::invalidFormat, "RGB pixel count does not match texture dimensions"));
    }

    uomul::TextureImage texture;
    texture.dimension = static_cast<uomul::TextureDimension>(image.width);
    texture.pixels.reserve(expectedPixels / 3U);
    for (std::size_t offset = 0; offset < image.pixels.size(); offset += 3U)
    {
        texture.pixels.push_back(uomul::RadarColorConverter::fromRgb888(
            image.pixels.at(offset), image.pixels.at(offset + 1U), image.pixels.at(offset + 2U)));
    }
    return texture;
}

std::expected<void, uomul::Error> writeBmp(const std::filesystem::path & outputPath,
                                           std::uint32_t width,
                                           std::uint32_t height,
                                           std::span<const std::uint8_t> pixels)
{
    return writeImage(outputPath, width, height, pixels, false);
}

std::expected<void, uomul::Error> writePng(const std::filesystem::path & outputPath,
                                           std::uint32_t width,
                                           std::uint32_t height,
                                           std::span<const std::uint8_t> pixels)
{
    return writeImage(outputPath, width, height, pixels, true);
}

} // namespace uo_mule::image
