#pragma once

#include "uomul/mul_common/mul_common.h"
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <vector>

namespace uo_mule::image
{

/** @brief Top-down 8-bit-per-channel RGB image data. */
struct RgbImage
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> pixels;

    bool operator==(const RgbImage &) const = default;
};

/** @brief Decodes BMP or PNG input into top-down RGB pixels. */
[[nodiscard]] std::expected<RgbImage, uomul::Error> decode(const std::filesystem::path & inputPath);

/** @brief Writes top-down RGB pixels as a 24-bit BMP. */
[[nodiscard]] std::expected<void, uomul::Error> writeBmp(const std::filesystem::path & outputPath,
                                                         std::uint32_t width,
                                                         std::uint32_t height,
                                                         std::span<const std::uint8_t> pixels);

/** @brief Writes top-down RGB pixels as an RGB PNG. */
[[nodiscard]] std::expected<void, uomul::Error> writePng(const std::filesystem::path & outputPath,
                                                         std::uint32_t width,
                                                         std::uint32_t height,
                                                         std::span<const std::uint8_t> pixels);

} // namespace uo_mule::image
