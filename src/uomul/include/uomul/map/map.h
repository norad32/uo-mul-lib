#pragma once

#include "uomul/mul_common/mul_common.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace uomul
{

inline constexpr std::size_t mapBlockTileCount = 64;
inline constexpr std::size_t mapBlockByteCount = 196;
inline constexpr std::uint32_t mapBlockDimension = 8;

struct Dimensions
{
    std::uint32_t widthTiles = 0;
    std::uint32_t heightTiles = 0;

    /** @brief Compares tile dimensions for equality.
     *  @param other Dimensions to compare.
     *  @retval true Both width and height are equal.
     *  @retval false At least one dimension differs.
     */
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

struct MapTile
{
    std::uint16_t id = 0;
    std::int8_t z = 0;

    /** @brief Compares a land tile with another tile.
     *  @param other MapTile to compare.
     *  @retval true Both land ID and elevation are equal.
     *  @retval false The ID or elevation differs.
     */
    bool operator==(const MapTile & other) const = default;
};

using MapBlock = std::array<MapTile, mapBlockTileCount>;
using MapBlockBytes = std::array<std::byte, mapBlockByteCount>;
using MapBlockRead = std::function<std::expected<void, Error>(BlockCoord, const MapBlock &)>;
using MapTileRead = std::function<std::expected<void, Error>(TileCoord, const MapTile &)>;
using MapBlockEdit = std::function<std::expected<MapBlock, Error>(BlockCoord, const MapBlock &)>;
using MapBlockBytesRead = std::function<std::expected<void, Error>(BlockCoord, const MapBlockBytes &)>;
using MapTileEdit = std::function<std::expected<MapTile, Error>(TileCoord, const MapTile &)>;

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
    std::uint64_t blocksChanged = 0;
    std::uint64_t tilesChanged = 0;
};

struct ValueStats
{
    std::int32_t value = 0;
    std::uint64_t count = 0;
    double percentage = 0.0;
};

struct MapStats
{
    std::uint64_t tileCount = 0;
    std::vector<ValueStats> tileIds;
    std::vector<ValueStats> zs;
    std::optional<double> averageZ;
    std::optional<double> medianZ;
};

class MapReader
{
public:
    MapReader(MapReader && other) noexcept;
    MapReader & operator=(MapReader && other) noexcept;
    MapReader(const MapReader &) = delete;
    MapReader & operator=(const MapReader &) = delete;

    ~MapReader();

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
    [[nodiscard]] std::expected<MapBlock, Error> readBlock(BlockCoord block) const;

    /** @brief Reads one land tile.
     *  @param tile Global tile coordinates.
     *  @retval value MapTile at the requested coordinate.
     *  @retval error Invalid coordinate or unavailable/truncated block details.
     */
    [[nodiscard]] std::expected<MapTile, Error> readTile(TileCoord tile) const;

    /** @brief Reads decoded blocks in a half-open block region.
     *  @param region Block coordinates from min inclusive to maxExclusive exclusive.
     *  @param callback Called once per block; returning an error stops the read.
     *  @retval value Success when the operation completes.
     *  @retval error Validation, read, or callback failure details.
     */
    [[nodiscard]] std::expected<void, Error> readBlocks(BlockRegion region, const MapBlockRead & callback) const;

    /** @brief Reads raw blocks, including their four-byte headers, in a half-open region.
     *  @param region Block coordinates from min inclusive to maxExclusive exclusive.
     *  @param callback Called once per raw block; returning an error stops the read.
     *  @retval value Success when the operation completes.
     *  @retval error Validation, read, or callback failure details.
     */
    [[nodiscard]] std::expected<void, Error> readRawBlocks(BlockRegion region, const MapBlockBytesRead & callback) const;

    /** @brief Reads tiles in a half-open tile region.
     *  @param region Global tile coordinates from min inclusive to maxExclusive exclusive.
     *  @param callback Called once per tile; returning an error stops the read.
     *  @retval value Success when the operation completes.
     *  @retval error Validation, read, or callback failure details.
     */
    [[nodiscard]] std::expected<void, Error> readTiles(TileRegion region, const MapTileRead & callback) const;

    /** @brief Calculates whole-map ID and elevation statistics.
     *  ID and elevation frequencies are sparse and sorted by value. Percentages use all sampled tiles as the denominator.
     *  The average and median are unset when the map has no sampled tiles.
     *  @retval value Whole-map counts, distributions, average elevation, and median elevation.
     *  @retval error A required map block is unavailable or a read fails.
     */
    [[nodiscard]] std::expected<MapStats, Error> stats() const;

    /** @brief Calculates ID and elevation statistics for an exact half-open tile region.
     *  @param region Global tile coordinates from min inclusive to maxExclusive exclusive.
     *  @retval value Statistics for sampled tiles; empty regions have empty distributions and unset averages.
     *  @retval error Invalid region or a required map block is unavailable.
     */
    [[nodiscard]] std::expected<MapStats, Error> statsTiles(TileRegion region) const;

    /** @brief Calculates ID and elevation statistics for all tiles in a half-open block region.
     *  @param region Block coordinates from min inclusive to maxExclusive exclusive.
     *  @retval value Statistics for all tiles in selected blocks; empty regions have no samples.
     *  @retval error Invalid region or a required map block is unavailable.
     */
    [[nodiscard]] std::expected<MapStats, Error> statsBlocks(BlockRegion region) const;

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
    MapEditor(const MapEditor &) = delete;
    MapEditor & operator=(const MapEditor &) = delete;
    ~MapEditor();

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

    /** @brief Creates and publishes a complete map initialized with one tile value.
     *  @param outputMap Destination map path.
     *  @param dimensions New map width and height in tiles; each must be a positive multiple of 8.
     *  @param initialTile Tile value used throughout the new map; defaults to ID 0, Z 0.
     *  @param options Controls replacement of an existing output.
     *  @retval value Map created successfully.
     *  @retval error Invalid dimensions, output path, or staging I/O failure details.
     */
    [[nodiscard]] static std::expected<void, Error> createEmpty(const std::filesystem::path & outputMap,
                                                                Dimensions dimensions,
                                                                MapTile initialTile = MapTile{.id = 0, .z = 0},
                                                                EditOptions options = {});

    /** @brief Replaces one block in the staged map.
     *  @param block Block coordinates.
     *  @param data Replacement tiles.
     *  @retval value Success when the block is staged.
     *  @retval error Inactive editor, invalid coordinate, or I/O failure details.
     */
    [[nodiscard]] std::expected<void, Error> editBlock(BlockCoord block, const MapBlock & data);

    /** @brief Replaces one tile in the staged map.
     *  @param tile Global tile coordinates.
     *  @param data Replacement tile.
     *  @retval value Success when the tile is staged.
     *  @retval error Inactive editor, invalid coordinate, or I/O failure details.
     */
    [[nodiscard]] std::expected<void, Error> editTile(TileCoord tile, MapTile data);

    /** @brief Edits every block in a half-open block region transactionally.
     *  @param region Block coordinates from min inclusive to maxExclusive exclusive.
     *  @param callback Produces each replacement block; an error cancels the edit.
     *  @retval value Success when the edit is staged.
     *  @retval error Validation, callback, journal, or I/O failure details.
     */
    [[nodiscard]] std::expected<void, Error> editBlocks(BlockRegion region, const MapBlockEdit & callback);

    /** @brief Edits tiles in a half-open tile region transactionally.
     *  @param region Global tile coordinates from min inclusive to maxExclusive exclusive.
     *  @param callback Produces each replacement tile; an error cancels the edit.
     *  @retval value Success when the edit is staged.
     *  @retval error Validation, callback, journal, or I/O failure details.
     */
    [[nodiscard]] std::expected<void, Error> editTiles(TileRegion region, const MapTileEdit & callback);

    /** @brief Fills the entire staged map with one tile value.
     *  @param value MapTile value written to every map tile.
     *  @retval value Success when the fill is staged.
     *  @retval error Inactive editor or staging I/O failure details.
     */
    [[nodiscard]] std::expected<void, Error> fill(MapTile value);

    /** @brief Fills a half-open tile region with one value.
     *  @param region Global tile coordinates from min inclusive to maxExclusive exclusive.
     *  @param value MapTile value to write.
     *  @retval value Success when the fill is staged.
     *  @retval error Validation, callback, journal, or I/O failure details.
     */
    [[nodiscard]] std::expected<void, Error> fillTiles(TileRegion region, MapTile value);

    /** @brief Fills a half-open block region across all tiles in each selected block.
     *  @param region Block coordinates from min inclusive to maxExclusive exclusive.
     *  @param value MapTile value written to all tiles in each selected block.
     *  @retval value Success when the fill is staged.
     *  @retval error Validation, callback, journal, or I/O failure details.
     */
    [[nodiscard]] std::expected<void, Error> fillBlocks(BlockRegion region, MapTile value);

    /** @brief Replaces matching land IDs across the entire staged map.
     *  @param from Land ID to find.
     *  @param to Replacement land ID.
     *  @retval value Success when replacements are staged.
     *  @retval error Inactive editor or staging I/O failure details.
     */
    [[nodiscard]] std::expected<void, Error> replaceIds(std::uint16_t from, std::uint16_t to);

    /** @brief Replaces matching land IDs in a half-open tile region.
     *  @param region Global tile coordinates from min inclusive to maxExclusive exclusive.
     *  @param from ID to find.
     *  @param to Replacement ID.
     *  @retval value Success when replacements are staged.
     *  @retval error Validation, callback, journal, or I/O failure details.
     */
    [[nodiscard]] std::expected<void, Error> replaceIdsTiles(TileRegion region, std::uint16_t from, std::uint16_t to);

    /** @brief Replaces matching land IDs across all tiles in a half-open block region.
     *  @param region Block coordinates from min inclusive to maxExclusive exclusive.
     *  @param from Land ID to find.
     *  @param to Replacement land ID.
     *  @retval value Success when replacements are staged.
     *  @retval error Validation, callback, journal, or I/O failure details.
     */
    [[nodiscard]] std::expected<void, Error> replaceIdsBlocks(BlockRegion region, std::uint16_t from, std::uint16_t to);

    /** @brief Replaces matching elevations across the entire staged map.
     *  @param from Elevation to find.
     *  @param to Replacement elevation.
     *  @retval value Success when replacements are staged.
     *  @retval error Inactive editor or staging I/O failure details.
     */
    [[nodiscard]] std::expected<void, Error> replaceZ(std::int8_t from, std::int8_t to);

    /** @brief Replaces elevations in a half-open tile region.
     *  @param region Global tile coordinates from min inclusive to maxExclusive exclusive.
     *  @param from Elevation to find.
     *  @param to Replacement elevation.
     *  @retval value Success when replacements are staged.
     *  @retval error Validation, callback, journal, or I/O failure details.
     */
    [[nodiscard]] std::expected<void, Error> replaceZTiles(TileRegion region, std::int8_t from, std::int8_t to);

    /** @brief Replaces matching elevations across all tiles in a half-open block region.
     *  @param region Block coordinates from min inclusive to maxExclusive exclusive.
     *  @param from Elevation to find.
     *  @param to Replacement elevation.
     *  @retval value Success when replacements are staged.
     *  @retval error Validation, callback, journal, or I/O failure details.
     */
    [[nodiscard]] std::expected<void, Error> replaceZBlocks(BlockRegion region, std::int8_t from, std::int8_t to);

    /** @brief Publishes staged changes to the output path.
     *  @retval value Output committed successfully.
     *  @retval error Inactive editor, sync failure, or publish failure details.
     */
    [[nodiscard]] std::expected<void, Error> commit();

    /** @brief Discards staged changes without publishing them.
     *  Safe to call more than once; the editor cannot be used after aborting.
     */
    void abort() noexcept;

private:
    struct Impl;
    /** @brief Constructs an editor from its owned implementation state.
     *  @param impl Active editor state.
     */
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

    /** @brief Generates a diff containing only changes inside a tile region.
     *  Boundary blocks preserve base tiles outside the region; apply this diff to the same base map.
     *  @param baseMap Original map.
     *  @param targetMap Map whose in-region tiles are included.
     *  @param diffDirectory Destination directory.
     *  @param mapIndex Facet index used in output filenames.
     *  @param dimensions Dimensions shared by both maps.
     *  @param region Half-open tile region to compare.
     *  @param overwrite Whether existing output files may be replaced.
     *  @retval value Counts of changed blocks and in-region tiles.
     *  @retval error Invalid maps, region, diff output, or I/O failure details.
     */
    [[nodiscard]] static std::expected<DiffReport, Error> generateTiles(const std::filesystem::path & baseMap,
                                                                        const std::filesystem::path & targetMap,
                                                                        const std::filesystem::path & diffDirectory,
                                                                        std::uint32_t mapIndex,
                                                                        Dimensions dimensions,
                                                                        TileRegion region,
                                                                        bool overwrite = false);

    /** @brief Generates a diff containing only changes inside a block region.
     *  @param baseMap Original map.
     *  @param targetMap Map whose selected blocks are included.
     *  @param diffDirectory Destination directory.
     *  @param mapIndex Facet index used in output filenames.
     *  @param dimensions Dimensions shared by both maps.
     *  @param region Half-open block region to compare.
     *  @param overwrite Whether existing output files may be replaced.
     *  @retval value Counts of changed blocks and tiles.
     *  @retval error Invalid maps, region, diff output, or I/O failure details.
     */
    [[nodiscard]] static std::expected<DiffReport, Error> generateBlocks(const std::filesystem::path & baseMap,
                                                                         const std::filesystem::path & targetMap,
                                                                         const std::filesystem::path & diffDirectory,
                                                                         std::uint32_t mapIndex,
                                                                         Dimensions dimensions,
                                                                         BlockRegion region,
                                                                         bool overwrite = false);

    /** @brief Applies a legacy map diff to a base map and publishes the result.
     *  @param baseMap Map to patch.
     *  @param diffDirectory Directory containing the map-diff set.
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
