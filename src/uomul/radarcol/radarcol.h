#pragma once

#include "uomul/mul_common/mul_common.h"
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>

namespace uomul
{

inline constexpr std::uint32_t radarMapColorCount = 0x4000;
inline constexpr std::uint32_t radarStaticColorOffset = 0x4000;
inline constexpr std::uint32_t radarColorCount = 0x8000;

/** @brief One raw 16-bit radar color value, stored in the client format. */
using RadarColor = std::uint16_t;

class RadarColorConverter
{
public:
    static constexpr std::uint32_t channelShift = 3;
    static constexpr std::uint32_t greenShift = 5;
    static constexpr std::uint32_t redShift = 10;
    static constexpr std::uint32_t byteShift = 8;
    static constexpr std::uint32_t channelMask = 0x1F;
    static constexpr std::uint32_t rgbMask = 0x00FFFFFF;
    static constexpr std::uint32_t colorMask = 0x7FFF;
    /** @brief Converts 8-bit RGB channels to the radar RGB555 layout.
     *  The low three bits of each input channel are discarded.
     *  @param red Red channel in the range [0, 255].
     *  @param green Green channel in the range [0, 255].
     *  @param blue Blue channel in the range [0, 255].
     *  @retval value Packed 15-bit RGB555 color with the high bit clear.
     */
    [[nodiscard]] static constexpr RadarColor fromRgb888(std::uint8_t red, std::uint8_t green, std::uint8_t blue) noexcept
    {
        const auto red5 = static_cast<std::uint32_t>(red) >> channelShift;
        const auto green5 = static_cast<std::uint32_t>(green) >> channelShift;
        const auto blue5 = static_cast<std::uint32_t>(blue) >> channelShift;
        return static_cast<RadarColor>((red5 << redShift) | (green5 << greenShift) | blue5);
    }

    /** @brief Converts packed 0xRRGGBB to the radar RGB555 layout.
     *  @param rgb Packed 24-bit RGB value.
     *  @retval value Packed 15-bit RGB555 color with the high bit clear.
     */
    [[nodiscard]] static constexpr RadarColor fromRgb24(std::uint32_t rgb) noexcept
    {
        return fromRgb888(static_cast<std::uint8_t>(rgb >> (2 * byteShift)),
                          static_cast<std::uint8_t>(rgb >> byteShift),
                          static_cast<std::uint8_t>(rgb));
    }

    /** @brief Converts packed 0xAARRGGBB to the radar RGB555 layout.
     *  The alpha channel is ignored.
     *  @param argb Packed 32-bit ARGB value.
     *  @retval value Packed 15-bit RGB555 color with the high bit clear.
     */
    [[nodiscard]] static constexpr RadarColor fromArgb8888(std::uint32_t argb) noexcept { return fromRgb24(argb & rgbMask); }

    /** @brief Converts packed 0xRRGGBBAA to the radar RGB555 layout.
     *  The alpha channel is ignored.
     *  @param rgba Packed 32-bit RGBA value.
     *  @retval value Packed 15-bit RGB555 color with the high bit clear.
     */
    [[nodiscard]] static constexpr RadarColor fromRgba8888(std::uint32_t rgba) noexcept { return fromRgb24(rgba >> byteShift); }

    /** @brief Converts RGB565 to RGB555 by discarding the least significant green bit.
     *  @param rgb565 Packed 16-bit RGB565 value.
     *  @retval value Packed RGB555 color with the high bit clear.
     */
    [[nodiscard]] static constexpr RadarColor fromRgb565(std::uint16_t rgb565) noexcept
    {
        const auto packed = static_cast<std::uint32_t>(rgb565);
        const std::uint32_t red = (packed >> 11U) & channelMask;
        const std::uint32_t green = (packed >> 6U) & channelMask;
        const std::uint32_t blue = packed & channelMask;
        return static_cast<RadarColor>((red << redShift) | (green << greenShift) | blue);
    }

    /** @brief Converts ARGB1555 to radar RGB555 by discarding the alpha bit.
     *  @param argb1555 Packed 16-bit ARGB1555 value.
     *  @retval value Lower 15 bits in radar RGB555 layout.
     */
    [[nodiscard]] static constexpr RadarColor fromArgb1555(std::uint16_t argb1555) noexcept
    {
        return static_cast<RadarColor>(static_cast<std::uint32_t>(argb1555) & colorMask);
    }

    /** @brief Normalizes an RGB555 word to the radar color representation.
     *  @param rgb555 Packed 16-bit RGB555 value, bit 15 is discarded.
     *  @retval value Lower 15 bits in radar RGB555 layout.
     */
    [[nodiscard]] static constexpr RadarColor fromRgb555(std::uint16_t rgb555) noexcept
    {
        return static_cast<RadarColor>(static_cast<std::uint32_t>(rgb555) & colorMask);
    }
};

struct RadarColEditOptions
{
    bool overwriteOutput = false;
};

class RadarColReader
{
public:
    RadarColReader(RadarColReader && other) noexcept;
    RadarColReader & operator=(RadarColReader && other) noexcept;
    RadarColReader(const RadarColReader &) = delete;
    RadarColReader & operator=(const RadarColReader &) = delete;
    ~RadarColReader();

    /** @brief Opens a radarcol.mul color table.
     *  @param path Radar color file to open.
     *  @retval value Reader for the complete color table.
     *  @retval error Inaccessible, truncated, or malformed file details.
     */
    [[nodiscard]] static std::expected<RadarColReader, Error> open(const std::filesystem::path & path);

    /** @brief Returns the number of 16-bit colors present in the file.
     *  @retval value Number of readable colors; a full table contains 0x8000 entries.
     */
    [[nodiscard]] std::uint32_t colorCount() const noexcept;

    /** @brief Reads a map radar color by map tile ID.
     *  @param id Map tile ID in the range [0, 0x4000).
     *  @retval value Raw RGB555 color word at the map ID index.
     *  @retval error ID is out of range or the color is unavailable.
     */
    [[nodiscard]] std::expected<RadarColor, Error> readMapColor(std::uint16_t id) const;

    /** @brief Reads a static radar color by static tile ID.
     *  @param id Static tile ID in the range [0, 0x4000).
     *  @retval value Raw RGB555 color word at staticId + 0x4000.
     *  @retval error ID is out of range or the color is unavailable.
     */
    [[nodiscard]] std::expected<RadarColor, Error> readStaticColor(std::uint16_t id) const;

    /** @brief Reads a radar color by its raw table index.
     *  @param index Color index in the on-disk table.
     *  @retval value Raw RGB555 color word.
     *  @retval error Index is unavailable or the file read fails.
     */
    [[nodiscard]] std::expected<RadarColor, Error> readRawColor(std::uint32_t index) const;

private:
    struct Impl;
    explicit RadarColReader(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

class RadarColEditor
{
public:
    RadarColEditor(RadarColEditor && other) noexcept;
    RadarColEditor & operator=(RadarColEditor && other) noexcept;
    RadarColEditor(const RadarColEditor &) = delete;
    RadarColEditor & operator=(const RadarColEditor &) = delete;
    ~RadarColEditor();

    /** @brief Creates an editor that stages a source color table to an output path.
     *  @param sourceFile Existing radarcol.mul file.
     *  @param outputFile Destination published by commit.
     *  @param options Controls replacement of an existing output.
     *  @retval value Editor for the staged color table.
     *  @retval error Invalid paths, malformed source, or staging I/O failure details.
     */
    [[nodiscard]] static std::expected<RadarColEditor, Error> create(const std::filesystem::path & sourceFile,
                                                                     const std::filesystem::path & outputFile,
                                                                     RadarColEditOptions options = {});

    /** @brief Creates a new empty radarcol.mul color table at an output path.
     *  All 0x8000 map and static color entries are initialized to zero.
     *  @param outputFile Destination published by commit.
     *  @param options Controls replacement of an existing output.
     *  @retval value Editor for the new color table.
     *  @retval error Invalid output path or staging I/O failure details.
     */
    [[nodiscard]] static std::expected<RadarColEditor, Error> createEmpty(const std::filesystem::path & outputFile,
                                                                          RadarColEditOptions options = {});

    /** @brief Edits the radar color associated with a map tile ID.
     *  @param id Map tile ID in the range [0, 0x4000).
     *  @param value Replacement RGB555 color word.
     *  @retval value Success when the change is staged.
     *  @retval error Inactive editor, invalid ID, or staging I/O failure details.
     */
    [[nodiscard]] std::expected<void, Error> editMapColor(std::uint16_t mapId, RadarColor value);

    /** @brief Edits the radar color associated with a static tile ID.
     *  @param id Static tile ID in the range [0, 0x4000).
     *  @param value Replacement RGB555 color word.
     *  @retval value Success when the change is staged.
     *  @retval error Inactive editor, invalid ID, or staging I/O failure details.
     */
    [[nodiscard]] std::expected<void, Error> editStaticColor(std::uint16_t id, RadarColor value);

    /** @brief Edits a radar color by its raw table index.
     *  @param index Raw color-table index, including the static color offset where applicable.
     *  @param value Replacement RGB555 color word.
     *  @retval value Success when the change is staged.
     *  @retval error Inactive editor, invalid index, or staging I/O failure details.
     */
    [[nodiscard]] std::expected<void, Error> editRawColor(std::uint32_t index, RadarColor value);

    /** @brief Publishes staged color changes to the output path.
     *  @retval value Output committed successfully.
     *  @retval error Inactive editor, sync failure, or publish failure details.
     */
    [[nodiscard]] std::expected<void, Error> commit();

    /** @brief Discards staged changes without publishing them.
     *  Safe to call more than once. The editor cannot be used after aborting.
     */
    void abort() noexcept;

private:
    struct Impl;
    explicit RadarColEditor(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace uomul
