#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <string>

namespace uomul
{

inline constexpr std::size_t landBlockTileCount = 64;
inline constexpr std::size_t landBlockByteCount = 196;
inline constexpr std::uint32_t landBlockDimension = 8;

enum class ErrorCode : std::uint8_t
{
    io,
    invalidDimensions,
    invalidCoordinate,
    truncatedInput,
    invalidBlockCount,
    commitFailed,
};

struct Error
{
    ErrorCode code = ErrorCode::io;
    std::filesystem::path path;
    std::uint64_t offset = 0;
    std::string message;
};

struct Dimensions
{
    std::uint32_t widthTiles = 0;
    std::uint32_t heightTiles = 0;

    bool operator==(const Dimensions & other) const = default;
};

struct TileCoord
{
    std::uint32_t x = 0;
    std::uint32_t y = 0;
};

struct BlockCoord
{
    std::uint32_t x = 0;
    std::uint32_t y = 0;
};

struct TileRegion
{
    TileCoord min{};
    TileCoord maxExclusive{};
};

struct BlockRegion
{
    BlockCoord min{};
    BlockCoord maxExclusive{};
};

struct LandTile
{
    std::uint16_t id = 0;
    std::int8_t z = 0;
    bool operator==(const LandTile &) const = default;
};

using LandBlock = std::array<LandTile, landBlockTileCount>;
using LandBlockBytes = std::array<std::byte, landBlockByteCount>;
using LandBlockRead = std::function<std::expected<void, Error>(BlockCoord, const LandBlock &)>;
using LandTileRead = std::function<std::expected<void, Error>(TileCoord, const LandTile &)>;
using LandBlockEdit = std::function<std::expected<LandBlock, Error>(BlockCoord, const LandBlock &)>;
using LandBlockBytesRead = std::function<std::expected<void, Error>(BlockCoord, const LandBlockBytes &)>;
using LandTileEdit = std::function<std::expected<LandTile, Error>(TileCoord, const LandTile &)>;

struct OpenOptions
{
    bool recoverTruncatedMap = false;
};

struct EditOptions
{
    bool overwriteOutput = false;
};

struct DiffReport
{
    std::uint64_t landBlocksChanged = 0;
    std::uint64_t landTilesChanged = 0;
};

class MapReader
{
public:
    MapReader(MapReader && other) noexcept;
    MapReader & operator=(MapReader && other) noexcept;
    ~MapReader();

    MapReader(const MapReader &) = delete;
    MapReader & operator=(const MapReader &) = delete;

    /** @brief Opens a map using the supplied tile dimensions.
     *  @param path Map file to open.
     *  @param dimensions Map width and height in tiles; each must be a positive multiple of 8.
     *  @param options Controls recovery of truncated maps.
     *  @retval value Open reader.
     *  @retval error Invalid dimensions, inaccessible file, or malformed map details.
     */
    [[nodiscard]] static std::expected<MapReader, Error>
    open(const std::filesystem::path & path, Dimensions dimensions, OpenOptions options = {});

    /** @brief Returns the dimensions used to open this map.
     *  @retval value Map dimensions in tiles.
     */
    [[nodiscard]] Dimensions dimensions() const noexcept;

    /** @brief Returns the number of complete blocks available in the file.
     *  @retval value Number of complete land blocks.
     */
    [[nodiscard]] std::uint64_t completeBlocks() const noexcept;

    /** @brief Returns the number of trailing bytes not forming a complete block.
     *  @retval value Number of ignored trailing bytes.
     */
    [[nodiscard]] std::uint64_t ignoredTrailingBytes() const noexcept;

    /** @brief Returns how many dimension-implied blocks are missing from the file.
     *  @retval value Number of missing land blocks.
     */
    [[nodiscard]] std::uint64_t missingBlocks() const noexcept;

    /** @brief Reads and decodes one land block.
     *  @param block Block coordinates.
     *  @retval value Decoded land block.
     *  @retval error Invalid coordinate or unavailable/truncated block details.
     */
    [[nodiscard]] std::expected<LandBlock, Error> readLandBlock(BlockCoord block) const;

    /** @brief Reads one land tile.
     *  @param tile Global tile coordinates.
     *  @retval value Tile at the requested coordinate.
     *  @retval error Invalid coordinate or unavailable/truncated block details.
     */
    [[nodiscard]] std::expected<LandTile, Error> readLandTile(TileCoord tile) const;

    /** @brief Reads decoded blocks in a half-open block region.
     *  @param region Block coordinates from min inclusive to maxExclusive exclusive.
     *  @param callback Called once per block; returning an error stops the read.
     *  @retval value Success when the operation completes.
     *  @retval error Validation, read, or callback failure details.
     */
    [[nodiscard]] std::expected<void, Error> readLandBlocks(BlockRegion region, const LandBlockRead & callback) const;

    /** @brief Reads raw blocks, including their four-byte headers, in a half-open region.
     *  @param region Block coordinates from min inclusive to maxExclusive exclusive.
     *  @param callback Called once per raw block; returning an error stops the read.
     *  @retval value Success when the operation completes.
     *  @retval error Validation, read, or callback failure details.
     */
    [[nodiscard]] std::expected<void, Error> readRawLandBlocks(BlockRegion region, const LandBlockBytesRead & callback) const;

    /** @brief Reads tiles in a half-open tile region.
     *  @param region Global tile coordinates from min inclusive to maxExclusive exclusive.
     *  @param callback Called once per tile; returning an error stops the read.
     *  @retval value Success when the operation completes.
     *  @retval error Validation, read, or callback failure details.
     */
    [[nodiscard]] std::expected<void, Error> readLandTiles(TileRegion region, const LandTileRead & callback) const;

private:
    struct Impl;
    explicit MapReader(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

class MapEditor
{
public:
    MapEditor(MapEditor && other) noexcept;
    MapEditor & operator=(MapEditor && other) noexcept;
    ~MapEditor();

    MapEditor(const MapEditor &) = delete;
    MapEditor & operator=(const MapEditor &) = delete;

    /** @brief Creates an editor by staging a source map to a separate output path.
     *  @param sourceMap Map to copy and edit.
     *  @param outputMap Destination published by commit.
     *  @param dimensions Source map dimensions in tiles.
     *  @param options Controls replacement of an existing output.
     *  @retval value Editor.
     *  @retval error Invalid paths, dimensions, source, or output details.
     */
    [[nodiscard]] static std::expected<MapEditor, Error> create(const std::filesystem::path & sourceMap,
                                                                const std::filesystem::path & outputMap,
                                                                Dimensions dimensions,
                                                                EditOptions options = {});

    /** @brief Replaces one block in the staged map.
     *  @param block Block coordinates.
     *  @param data Replacement tiles.
     *  @retval value Success when the block is staged.
     *  @retval error Inactive editor, invalid coordinate, or I/O failure details.
     */
    [[nodiscard]] std::expected<void, Error> writeLandBlock(BlockCoord block, const LandBlock & data);

    /** @brief Replaces one tile in the staged map.
     *  @param tile Global tile coordinates.
     *  @param data Replacement tile.
     *  @retval value Success when the tile is staged.
     *  @retval error Inactive editor, invalid coordinate, or I/O failure details.
     */
    [[nodiscard]] std::expected<void, Error> writeLandTile(TileCoord tile, LandTile data);

    /** @brief Edits every block in a half-open block region transactionally.
     *  @param region Block coordinates from min inclusive to maxExclusive exclusive.
     *  @param callback Produces each replacement block; an error cancels the edit.
     *  @retval value Success when the edit is staged.
     *  @retval error Validation, callback, journal, or I/O failure details.
     */
    [[nodiscard]] std::expected<void, Error> editLandBlocks(BlockRegion region, const LandBlockEdit & callback);

    /** @brief Edits tiles in a half-open tile region transactionally.
     *  @param region Global tile coordinates from min inclusive to maxExclusive exclusive.
     *  @param callback Produces each replacement tile; an error cancels the edit.
     *  @retval value Success when the edit is staged.
     *  @retval error Validation, callback, journal, or I/O failure details.
     */
    [[nodiscard]] std::expected<void, Error> editLandTiles(TileRegion region, const LandTileEdit & callback);

    /** @brief Fills a half-open tile region with one value.
     *  @param region Global tile coordinates from min inclusive to maxExclusive exclusive.
     *  @param value Tile value to write.
     *  @retval value Success when the fill is staged.
     *  @retval error Validation, callback, journal, or I/O failure details.
     */
    [[nodiscard]] std::expected<void, Error> fill(TileRegion region, LandTile value);

    /** @brief Replaces land IDs in a half-open tile region.
     *  @param region Global tile coordinates from min inclusive to maxExclusive exclusive.
     *  @param from ID to find.
     *  @param to Replacement ID.
     *  @retval value Success when replacements are staged.
     *  @retval error Validation, callback, journal, or I/O failure details.
     */
    [[nodiscard]] std::expected<void, Error> replaceIds(TileRegion region, std::uint16_t from, std::uint16_t to);

    /** @brief Replaces elevations in a half-open tile region.
     *  @param region Global tile coordinates from min inclusive to maxExclusive exclusive.
     *  @param from Elevation to find.
     *  @param to Replacement elevation.
     *  @retval value Success when replacements are staged.
     *  @retval error Validation, callback, journal, or I/O failure details.
     */
    [[nodiscard]] std::expected<void, Error> replaceZ(TileRegion region, std::int8_t from, std::int8_t to);

    /** @brief Publishes staged changes to the output path.
     *  @retval value Output committed successfully.
     *  @retval error Inactive editor, sync failure, or publish failure details.
     */
    [[nodiscard]] std::expected<void, Error> commit();

    /** @brief Discards staged changes without publishing them. */
    void abort() noexcept;

private:
    struct Impl;
    explicit MapEditor(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

class MapDiff
{
public:
    /** @brief Generates legacy map-diff data and index files.
     *  @param baseMap Original map.
     *  @param targetMap Map whose changed blocks are stored in the diff.
     *  @param diffDirectory Destination directory.
     *  @param mapIndex Facet index used in output filenames.
     *  @param dimensions Dimensions shared by both maps.
     *  @param overwrite Whether existing output files may be replaced.
     *  @retval value Counts of changed blocks and tiles.
     *  @retval error Invalid maps, diff output, or I/O failure details.
     */
    [[nodiscard]] static std::expected<DiffReport, Error> generate(const std::filesystem::path & baseMap,
                                                                   const std::filesystem::path & targetMap,
                                                                   const std::filesystem::path & diffDirectory,
                                                                   std::uint32_t mapIndex,
                                                                   Dimensions dimensions,
                                                                   bool overwrite = false);

    /** @brief Applies a legacy map diff to a base map and publishes the result.
     *  @param baseMap Map to patch.
     *  @param diffDirectory Directory containing the map-diff pair.
     *  @param outputMap Destination map path.
     *  @param mapIndex Facet index used in diff filenames.
     *  @param dimensions Dimensions shared by the maps.
     *  @param overwrite Whether an existing output may be replaced.
     *  @retval value Patched map committed successfully.
     *  @retval error Invalid inputs, malformed diff, or I/O failure details.
     */
    [[nodiscard]] static std::expected<void, Error> apply(const std::filesystem::path & baseMap,
                                                          const std::filesystem::path & diffDirectory,
                                                          const std::filesystem::path & outputMap,
                                                          std::uint32_t mapIndex,
                                                          Dimensions dimensions,
                                                          bool overwrite = false);

    /** @brief Compares two maps and writes a CSV change report.
     *  @param sourceMap First map.
     *  @param targetMap Second map.
     *  @param reportFile CSV destination path.
     *  @param dimensions Dimensions shared by both maps.
     *  @param overwrite Whether an existing report may be replaced.
     *  @retval value Counts of changed blocks and tiles.
     *  @retval error Invalid maps, report path, or I/O failure details.
     */
    [[nodiscard]] static std::expected<DiffReport, Error> report(const std::filesystem::path & sourceMap,
                                                                 const std::filesystem::path & targetMap,
                                                                 const std::filesystem::path & reportFile,
                                                                 Dimensions dimensions,
                                                                 bool overwrite = false);
};

/** @brief Looks up dimensions for a named supported facet preset.
 *  @param name Facet name; matching is case-insensitive.
 *  @retval value Dimensions for the named facet.
 *  @retval error Unknown facet name.
 */
[[nodiscard]] std::expected<Dimensions, Error> facetDimensions(const std::string & name);

/** @brief Writes a CSV report of tile changes between two maps.
 *  @param sourceMap First map.
 *  @param targetMap Second map.
 *  @param reportFile CSV destination path.
 *  @param dimensions Dimensions shared by both maps.
 *  @param overwrite Whether an existing report may be replaced.
 *  @retval value CSV file written successfully.
 *  @retval error Invalid maps, report path, or I/O failure details.
 */
[[nodiscard]] std::expected<void, Error> writeCsvReport(const std::filesystem::path & sourceMap,
                                                        const std::filesystem::path & targetMap,
                                                        const std::filesystem::path & reportFile,
                                                        Dimensions dimensions,
                                                        bool overwrite = false);

} // namespace uomul
