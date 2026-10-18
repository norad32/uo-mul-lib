#pragma once

#include "uomul/map.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <vector>

namespace uomul
{

inline constexpr std::uint32_t textureCount = 0x4000;
inline constexpr std::uint32_t textureIndexEntryByteCount = 12;
inline constexpr std::uint32_t textureSmallDimension = 64;
inline constexpr std::uint32_t textureLargeDimension = 128;
inline constexpr std::size_t textureSmallPixelCount = static_cast<std::size_t>(textureSmallDimension) * textureSmallDimension;
inline constexpr std::size_t textureLargePixelCount = static_cast<std::size_t>(textureLargeDimension) * textureLargeDimension;
inline constexpr std::size_t textureSmallByteCount = textureSmallPixelCount * sizeof(std::uint16_t);
inline constexpr std::size_t textureLargeByteCount = textureLargePixelCount * sizeof(std::uint16_t);

/** @brief Width and height shared by a square land texture image. */
using TextureDimension = std::uint16_t;

struct TextureImage
{
    TextureDimension dimension = textureSmallDimension;
    std::vector<std::uint16_t> pixels;

    /** @brief Compares texture dimensions and raw pixel words for equality.
     *  @param other Texture image to compare.
     *  @retval true Dimensions and pixel values are equal.
     *  @retval false Dimensions or pixel values differ.
     */
    bool operator==(const TextureImage & other) const = default;
};

struct TexMapsEditOptions
{
    bool overwriteOutput = false;
};

/** @brief Summary of texture payload deduplication and compaction. */
struct TextureDeduplicationReport
{
    /** @brief Number of index slots inspected, including empty slots. */
    std::uint32_t entriesScanned = 0;
    /** @brief Number of unique populated texture images retained. */
    std::uint32_t uniquePayloads = 0;
    /** @brief Number of populated entries redirected to an earlier duplicate. */
    std::uint32_t duplicateEntries = 0;
    /** @brief Size of the staged texmaps.mul before deduplication. */
    std::uint64_t inputDataBytes = 0;
    /** @brief Size of the compacted texmaps.mul after deduplication. */
    std::uint64_t outputDataBytes = 0;
};

class TexMapsReader
{
public:
    TexMapsReader(TexMapsReader && other) noexcept;
    TexMapsReader & operator=(TexMapsReader && other) noexcept;
    TexMapsReader(const TexMapsReader &) = delete;
    TexMapsReader & operator=(const TexMapsReader &) = delete;
    ~TexMapsReader();

    /** @brief Opens a matching texidx.mul and texmaps.mul pair.
     *  @param indexFile Texture index file.
     *  @param dataFile Texture payload file referenced by the index.
     *  @retval value Reader for the validated texture pair.
     *  @retval error Invalid paths, malformed index entries, or data-range errors.
     */
    [[nodiscard]] static std::expected<TexMapsReader, Error>
    open(const std::filesystem::path & indexFile, const std::filesystem::path & dataFile);

    /** @brief Returns the number of complete 12-byte index slots.
     *  @retval value Number of slots present, at most 0x4000.
     */
    [[nodiscard]] std::uint32_t textureCount() const noexcept;

    /** @brief Tests whether a texture slot contains a payload.
     *  @param id Texture ID in the range [0, textureCount()).
     *  @retval true The index entry references a nonempty payload.
     *  @retval false The slot is empty.
     */
    [[nodiscard]] std::expected<bool, Error> hasTexture(std::uint32_t id) const;

    /** @brief Reads a texture by ID through its paired index entry.
     *  Pixel words are returned in file order without color-channel conversion.
     *  @param id Texture ID in the range [0, textureCount()).
     *  @retval value Square texture image with raw 16-bit pixel words.
     *  @retval error Invalid ID, empty slot, unsupported dimensions, or read failure details.
     */
    [[nodiscard]] std::expected<TextureImage, Error> readTexture(std::uint32_t id) const;

private:
    struct Impl;
    explicit TexMapsReader(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

class TexMapsEditor
{
public:
    TexMapsEditor(TexMapsEditor && other) noexcept;
    TexMapsEditor & operator=(TexMapsEditor && other) noexcept;
    TexMapsEditor(const TexMapsEditor &) = delete;
    TexMapsEditor & operator=(const TexMapsEditor &) = delete;
    ~TexMapsEditor();

    /** @brief Creates an editor that stages a matching source index and payload pair.
     *  @param sourceIndex Existing texidx.mul file.
     *  @param sourceData Existing texmaps.mul file referenced by sourceIndex.
     *  @param outputIndex Destination index published by commit.
     *  @param outputData Destination payload published by commit.
     *  @param options Controls replacement of both output files.
     *  @retval value Editor for the staged texture pair.
     *  @retval error Invalid paths, malformed input pair, or staging I/O failure details.
     */
    [[nodiscard]] static std::expected<TexMapsEditor, Error>
    create(const std::filesystem::path & sourceIndex,
           const std::filesystem::path & sourceData,
           const std::filesystem::path & outputIndex,
           const std::filesystem::path & outputData,
           TexMapsEditOptions options = {});

    /** @brief Creates an editor for a new empty texture pair.
     *  The index contains 0x4000 empty entries and the payload file is empty.
     *  @param outputIndex Destination texidx.mul path.
     *  @param outputData Destination texmaps.mul path.
     *  @param options Controls replacement of both output files.
     *  @retval value Editor for the new staged texture pair.
     *  @retval error Invalid paths or staging I/O failure details.
     */
    [[nodiscard]] static std::expected<TexMapsEditor, Error>
    createEmpty(const std::filesystem::path & outputIndex,
                const std::filesystem::path & outputData,
                TexMapsEditOptions options = {});

    /** @brief Replaces or fills one texture slot and updates its index entry.
     *  The new payload is appended to the staged texmaps.mul; the previous payload, if any,
     *  remains unreferenced so other index entries sharing it are not affected.
     *  @param id Texture ID in the range [0, 0x4000).
     *  @param texture Square image with raw 16-bit pixels in row-major order.
     *  @retval value Success when both payload and index changes are staged.
     *  @retval error Inactive editor, invalid ID/image, or staging I/O failure details.
     */
    [[nodiscard]] std::expected<void, Error> editTexture(std::uint32_t id, const TextureImage & texture);

    /** @brief Clears one texture slot by replacing its index entry with an empty entry.
     *  Existing payload bytes remain in the staged data file and may be shared by other slots.
     *  @param id Texture ID in the range [0, 0x4000).
     *  @retval value Success when the empty entry is staged.
     *  @retval error Inactive editor, invalid ID, or staging I/O failure details.
     */
    [[nodiscard]] std::expected<void, Error> clearTexture(std::uint32_t id);

    /** @brief Deduplicates identical texture payloads in the staged pair.
     *  Rewrites index lookups so identical raw pixel words with the same dimensions reference one
     *  payload, rebuilds the data file with unique referenced payloads, and preserves empty entries.
     *  Equality is determined by full pixel comparison, not hash equality alone.
     *  @retval value Counts of scanned entries, unique payloads, duplicates, and bytes before/after.
     *  @retval error Inactive editor, malformed staged pair, or staging I/O failure details; failure leaves the editor state unchanged.
     */
    [[nodiscard]] std::expected<TextureDeduplicationReport, Error> deduplicateTextures();

    /** @brief Publishes the staged index and payload pair together.
     *  @retval value Both outputs committed successfully.
     *  @retval error Inactive editor, sync failure, or pair-publish failure details.
     */
    [[nodiscard]] std::expected<void, Error> commit();

    /** @brief Discards staged data without publishing either output.
     *  Safe to call more than once, the editor cannot be used after aborting.
     */
    void abort() noexcept;

private:
    struct Impl;
    explicit TexMapsEditor(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace uomul
