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
inline constexpr std::uint64_t landBlockDimension = 8;

enum class ErrorCode
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
    bool operator==(const Dimensions &) const = default;
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
    MapReader(MapReader &&) noexcept;
    MapReader & operator=(MapReader &&) noexcept;
    ~MapReader();
    MapReader(const MapReader &) = delete;
    MapReader & operator=(const MapReader &) = delete;

    [[nodiscard]] static std::expected<MapReader, Error>
    open(const std::filesystem::path & path, Dimensions dimensions, OpenOptions options = {});
    [[nodiscard]] Dimensions dimensions() const noexcept;
    [[nodiscard]] std::uint64_t completeBlocks() const noexcept;
    [[nodiscard]] std::uint64_t ignoredTrailingBytes() const noexcept;
    [[nodiscard]] std::uint64_t missingBlocks() const noexcept;
    [[nodiscard]] std::expected<LandBlock, Error> readLandBlock(BlockCoord block) const;
    [[nodiscard]] std::expected<LandTile, Error> readLandTile(TileCoord tile) const;
    [[nodiscard]] std::expected<void, Error> readLandBlocks(BlockRegion region, const LandBlockRead & callback) const;
    [[nodiscard]] std::expected<void, Error> readRawLandBlocks(BlockRegion region, const LandBlockBytesRead & callback) const;
    [[nodiscard]] std::expected<void, Error> readLandTiles(TileRegion region, const LandTileRead & callback) const;

private:
    struct Impl;
    explicit MapReader(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

class MapEditor
{
public:
    MapEditor(MapEditor &&) noexcept;
    MapEditor & operator=(MapEditor &&) noexcept;
    ~MapEditor();
    MapEditor(const MapEditor &) = delete;
    MapEditor & operator=(const MapEditor &) = delete;

    [[nodiscard]] static std::expected<MapEditor, Error> create(const std::filesystem::path & sourceMap,
                                                                const std::filesystem::path & outputMap,
                                                                Dimensions dimensions,
                                                                EditOptions options = {});
    [[nodiscard]] std::expected<void, Error> writeLandBlock(BlockCoord block, const LandBlock & data);
    [[nodiscard]] std::expected<void, Error> writeLandTile(TileCoord tile, LandTile data);
    [[nodiscard]] std::expected<void, Error> editLandBlocks(BlockRegion region, const LandBlockEdit & callback);
    [[nodiscard]] std::expected<void, Error> editLandTiles(TileRegion region, const LandTileEdit & callback);
    [[nodiscard]] std::expected<void, Error> fill(TileRegion region, LandTile value);
    [[nodiscard]] std::expected<void, Error> replaceIds(TileRegion region, std::uint16_t from, std::uint16_t to);
    [[nodiscard]] std::expected<void, Error> replaceZ(TileRegion region, std::int8_t from, std::int8_t to);
    [[nodiscard]] std::expected<void, Error> commit();
    void abort() noexcept;

private:
    struct Impl;
    explicit MapEditor(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

class MapDiff
{
public:
    [[nodiscard]] static std::expected<DiffReport, Error> generate(const std::filesystem::path & baseMap,
                                                                   const std::filesystem::path & targetMap,
                                                                   const std::filesystem::path & diffDirectory,
                                                                   std::uint32_t mapIndex,
                                                                   Dimensions dimensions,
                                                                   bool overwrite = false);
    [[nodiscard]] static std::expected<void, Error> apply(const std::filesystem::path & baseMap,
                                                          const std::filesystem::path & diffDirectory,
                                                          const std::filesystem::path & outputMap,
                                                          std::uint32_t mapIndex,
                                                          Dimensions dimensions,
                                                          bool overwrite = false);
    [[nodiscard]] static std::expected<DiffReport, Error> report(const std::filesystem::path & sourceMap,
                                                                 const std::filesystem::path & targetMap,
                                                                 const std::filesystem::path & reportFile,
                                                                 Dimensions dimensions,
                                                                 bool overwrite = false);
};

[[nodiscard]] std::expected<Dimensions, Error> facetDimensions(const std::string & name);
[[nodiscard]] std::expected<void, Error> writeCsvReport(const std::filesystem::path & sourceMap,
                                                        const std::filesystem::path & targetMap,
                                                        const std::filesystem::path & reportFile,
                                                        Dimensions dimensions,
                                                        bool overwrite = false);

} // namespace uomul
