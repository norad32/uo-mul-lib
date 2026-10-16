#include "map_common.h"
#include "uomul/map.h"
#include <algorithm>
#include <bit>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <fcntl.h>
#include <filesystem>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace uomul
{

using detail::MapCommon;

namespace
{

constexpr double medianDivisor = 2.0;
constexpr std::size_t landIdCount = static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) + 1U;
constexpr std::size_t elevationCount = static_cast<std::size_t>(std::numeric_limits<std::uint8_t>::max()) + 1U;

std::expected<void, Error>
validateMapFileSize(const std::filesystem::path & path, std::uint64_t fileSize, std::uint64_t expectedSize, OpenOptions options)
{
    if (fileSize % MapCommon::landBlockByteSize != 0 && (fileSize > expectedSize || !options.recoverTruncatedMap))
    {
        return std::unexpected(
            MapCommon::makeError(ErrorCode::truncatedInput, path, fileSize, "map has trailing bytes beyond complete blocks"));
    }

    if (!options.recoverTruncatedMap && fileSize != expectedSize)
    {
        return std::unexpected(MapCommon::makeError(
            ErrorCode::truncatedInput, path, fileSize, "map size does not match dimensions and complete blocks"));
    }

    if (fileSize > expectedSize)
    {
        return std::unexpected(MapCommon::makeError(
            ErrorCode::truncatedInput, path, expectedSize, "map contains more blocks than specified dimensions"));
    }
    return {};
}

std::expected<void, Error> readRawLandBlock(int descriptor,
                                            std::uint64_t completeBlocks,
                                            BlockCoord block,
                                            Dimensions dimensions,
                                            const std::filesystem::path & path,
                                            LandBlockBytes & raw)
{
    const auto offset = MapCommon::blockOffset(block, dimensions);
    if (MapCommon::blockId(block, dimensions) >= completeBlocks ||
        !MapCommon::readExact(descriptor, raw.data(), raw.size(), offset))
    {
        return std::unexpected(
            MapCommon::makeError(ErrorCode::truncatedInput, path, offset, "land block is missing or truncated"));
    }
    return {};
}

template <typename Region, typename Callback>
std::expected<void, Error>
validateReadRequest(Region region, const Callback & callback, Dimensions dimensions, const std::filesystem::path & path)
{
    if (auto valid = MapCommon::validateRegion(region, dimensions, path); !valid)
    {
        return valid;
    }
    if (!callback)
    {
        return std::unexpected(MapCommon::makeError(ErrorCode::io, path, 0, "read callback is empty"));
    }
    return {};
}

class StatsAccumulator
{
public:
    void add(LandTile tile)
    {
        ++idCounts_[tile.id];
        const auto elevationIndex =
            static_cast<std::size_t>(static_cast<int>(tile.z) - std::numeric_limits<std::int8_t>::min());
        ++elevationCounts_[elevationIndex];
        elevationTotal_ += tile.z;
        ++tileCount_;
    }

    [[nodiscard]] MapStats finish() const
    {
        MapStats result{};
        result.tileCount = tileCount_;
        if (tileCount_ == 0)
        {
            return result;
        }

        const double sampleCount = static_cast<double>(tileCount_);
        appendIdStats(result, sampleCount);
        appendElevationStats(result, sampleCount);
        result.averageZ = static_cast<double>(elevationTotal_ / static_cast<long double>(tileCount_));
        result.medianZ = medianElevation();
        return result;
    }

private:
    void appendIdStats(MapStats & result, double sampleCount) const
    {
        for (std::size_t id = 0; id < idCounts_.size(); ++id)
        {
            const auto count = idCounts_[id];
            if (count != 0)
            {
                result.tileIds.push_back(ValueStats{.value = static_cast<std::uint16_t>(id),
                                                    .count = count,
                                                    .percentage = (static_cast<double>(count) * 100.0) / sampleCount});
            }
        }
    }

    void appendElevationStats(MapStats & result, double sampleCount) const
    {
        for (std::size_t index = 0; index < elevationCounts_.size(); ++index)
        {
            const auto count = elevationCounts_[index];
            if (count != 0)
            {
                const auto elevation =
                    static_cast<std::int8_t>(static_cast<int>(index) + std::numeric_limits<std::int8_t>::min());
                result.zs.push_back(ValueStats{
                    .value = elevation, .count = count, .percentage = (static_cast<double>(count) * 100.0) / sampleCount});
            }
        }
    }

    [[nodiscard]] std::optional<double> medianElevation() const
    {
        const std::uint64_t lowerMiddleIndex = (tileCount_ - 1) / 2;
        const std::uint64_t upperMiddleIndex = tileCount_ / 2;
        std::uint64_t currentIndex = 0;
        std::optional<std::int8_t> lowerMiddle;
        std::optional<std::int8_t> upperMiddle;
        for (std::size_t index = 0; index < elevationCounts_.size(); ++index)
        {
            const auto nextIndex = currentIndex + elevationCounts_[index];
            const auto elevation = static_cast<std::int8_t>(static_cast<int>(index) + std::numeric_limits<std::int8_t>::min());
            if (!lowerMiddle && lowerMiddleIndex < nextIndex)
            {
                lowerMiddle = elevation;
            }
            if (upperMiddleIndex < nextIndex)
            {
                upperMiddle = elevation;
                break;
            }
            currentIndex = nextIndex;
        }
        if (!lowerMiddle || !upperMiddle)
        {
            return std::nullopt;
        }
        return (static_cast<double>(*lowerMiddle) + static_cast<double>(*upperMiddle)) / medianDivisor;
    }

    std::vector<std::uint64_t> idCounts_ = std::vector<std::uint64_t>(landIdCount);
    std::array<std::uint64_t, elevationCount> elevationCounts_{};
    long double elevationTotal_ = 0.0L;
    std::uint64_t tileCount_ = 0;
};

std::expected<MapStats, Error> collectStats(const MapReader & reader, TileRegion region)
{
    if (region.min.x == region.maxExclusive.x || region.min.y == region.maxExclusive.y)
    {
        return MapStats{};
    }

    StatsAccumulator stats;
    const auto blocks = detail::blockRegionForTiles(region);
    auto read = reader.readRawLandBlocks(
        blocks,
        [&stats, region](BlockCoord coordinate, const LandBlockBytes & raw) -> std::expected<void, Error>
        {
            const auto tiles = detail::clipTilesToBlock(region, coordinate);
            for (std::uint32_t x = tiles.min.x; x < tiles.maxExclusive.x; ++x)
            {
                for (std::uint32_t y = tiles.min.y; y < tiles.maxExclusive.y; ++y)
                {
                    const auto tileIndex = detail::tileIndexInBlock({.x = x, .y = y});
                    const auto tileOffset = static_cast<std::size_t>(MapCommon::landHeaderByteSize) +
                                            (tileIndex * static_cast<std::size_t>(MapCommon::landTileByteSize));
                    stats.add(detail::decodeLandTile(raw.data() + tileOffset));
                }
            }
            return {};
        });
    if (!read)
    {
        return std::unexpected(read.error());
    }
    return stats.finish();
}

} // namespace

struct MapReader::Impl
{
    std::filesystem::path path;
    Dimensions dimensions{};
    int descriptor = -1;
    std::uint64_t blocks = 0;
    std::uint64_t ignoredBytes = 0;
    std::uint64_t missingBlocks = 0;
};

MapReader::MapReader(std::unique_ptr<Impl> impl) : impl_(std::move(impl))
{
}

MapReader::MapReader(MapReader &&) noexcept = default;
MapReader & MapReader::operator=(MapReader &&) noexcept = default;
MapReader::~MapReader()
{
    if (impl_ && impl_->descriptor >= 0)
    {
        static_cast<void>(::close(impl_->descriptor));
    }
}

std::expected<MapReader, Error> MapReader::open(const std::filesystem::path & path, Dimensions dimensions, OpenOptions options)
{
    auto count = MapCommon::blockCount(dimensions, path);
    if (!count)
    {
        return std::unexpected(count.error());
    }

    detail::ScopedDescriptor descriptor(::open(path.c_str(), O_RDONLY | O_CLOEXEC));
    if (descriptor.get() < 0)
    {
        return std::unexpected(MapCommon::makeError(ErrorCode::io, path, 0, std::strerror(errno)));
    }

    struct stat fileStatus{};
    if (::fstat(descriptor.get(), &fileStatus) != 0)
    {
        return std::unexpected(MapCommon::makeError(ErrorCode::io, path, 0, std::strerror(errno)));
    }
    if (fileStatus.st_size < 0)
    {
        return std::unexpected(MapCommon::makeError(ErrorCode::io, path, 0, "could not determine map size"));
    }

    const auto fileSize = static_cast<std::uint64_t>(fileStatus.st_size);
    const auto expectedSize = *count * MapCommon::landBlockByteSize;
    if (auto validSize = validateMapFileSize(path, fileSize, expectedSize, options); !validSize)
    {
        return std::unexpected(validSize.error());
    }
    const auto completeBlockCount = fileSize / MapCommon::landBlockByteSize;

    if (!S_ISREG(fileStatus.st_mode))
    {
        return std::unexpected(MapCommon::makeError(ErrorCode::io, path, 0, "map input must be a regular file"));
    }

    auto impl = std::make_unique<Impl>();
    impl->path = path;
    impl->dimensions = dimensions;
    impl->descriptor = descriptor.release();
    impl->blocks = std::min(*count, completeBlockCount);
    impl->missingBlocks = *count - impl->blocks;
    impl->ignoredBytes = fileSize % MapCommon::landBlockByteSize;
    return MapReader(std::move(impl));
}

Dimensions MapReader::dimensions() const noexcept
{
    return impl_->dimensions;
}

std::uint64_t MapReader::completeBlocks() const noexcept
{
    return impl_->blocks;
}

std::uint64_t MapReader::ignoredTrailingBytes() const noexcept
{
    return impl_->ignoredBytes;
}

std::uint64_t MapReader::missingBlocks() const noexcept
{
    return impl_->missingBlocks;
}

std::expected<LandBlock, Error> MapReader::readLandBlock(BlockCoord block) const
{
    if (auto valid = MapCommon::validateBlock(block, impl_->dimensions, impl_->path); !valid)
    {
        return std::unexpected(valid.error());
    }

    LandBlockBytes raw{};
    if (auto loaded = readRawLandBlock(impl_->descriptor, impl_->blocks, block, impl_->dimensions, impl_->path, raw); !loaded)
    {
        return std::unexpected(loaded.error());
    }
    return MapCommon::decodeBlock(raw);
}

std::expected<LandTile, Error> MapReader::readLandTile(TileCoord tile) const
{
    if (auto valid = MapCommon::validateTile(tile, impl_->dimensions, impl_->path); !valid)
    {
        return std::unexpected(valid.error());
    }

    const BlockCoord block{.x = tile.x / landBlockDimension, .y = tile.y / landBlockDimension};
    const auto blockOffset = MapCommon::blockOffset(block, impl_->dimensions);
    if (MapCommon::blockId(block, impl_->dimensions) >= impl_->blocks)
    {
        return std::unexpected(
            MapCommon::makeError(ErrorCode::truncatedInput, impl_->path, blockOffset, "land block is missing or truncated"));
    }
    struct stat fileStatus{};
    if (::fstat(impl_->descriptor, &fileStatus) != 0)
    {
        return std::unexpected(MapCommon::makeError(ErrorCode::io, impl_->path, blockOffset, std::strerror(errno)));
    }
    if (fileStatus.st_size < 0 || static_cast<std::uint64_t>(fileStatus.st_size) < blockOffset + MapCommon::landBlockByteSize)
    {
        return std::unexpected(
            MapCommon::makeError(ErrorCode::truncatedInput, impl_->path, blockOffset, "land block is missing or truncated"));
    }
    constexpr std::size_t tileByteSize = static_cast<std::size_t>(MapCommon::landTileByteSize);
    std::array<std::byte, tileByteSize> raw{};
    const auto offset =
        blockOffset + MapCommon::landHeaderByteSize + (detail::tileIndexInBlock(tile) * MapCommon::landTileByteSize);
    if (!MapCommon::readExact(impl_->descriptor, raw.data(), raw.size(), offset))
    {
        return std::unexpected(
            MapCommon::makeError(ErrorCode::truncatedInput, impl_->path, offset, "land tile is missing or truncated"));
    }

    return detail::decodeLandTile(raw.data());
}

std::expected<void, Error> MapReader::readLandBlocks(BlockRegion region, const LandBlockRead & callback) const
{
    if (auto valid = validateReadRequest(region, callback, impl_->dimensions, impl_->path); !valid)
    {
        return valid;
    }

    return readRawLandBlocks(region,
                             [&callback](BlockCoord coordinate, const LandBlockBytes & raw) -> std::expected<void, Error>
                             { return callback(coordinate, MapCommon::decodeBlock(raw)); });
}

std::expected<void, Error> MapReader::readRawLandBlocks(BlockRegion region, const LandBlockBytesRead & callback) const
{
    if (auto valid = validateReadRequest(region, callback, impl_->dimensions, impl_->path); !valid)
    {
        return valid;
    }

    const auto blocksPerRead =
        std::min(detail::maximumBlocksPerIoBatch, static_cast<std::size_t>(region.maxExclusive.y - region.min.y));
    std::vector<std::byte> buffer(blocksPerRead * static_cast<std::size_t>(MapCommon::landBlockByteSize));
    for (std::uint32_t x = region.min.x; x < region.maxExclusive.x; ++x)
    {
        std::uint32_t y = region.min.y;
        while (y < region.maxExclusive.y)
        {
            const auto blocksRemaining = static_cast<std::size_t>(region.maxExclusive.y - y);
            const auto blockCount = std::min(blocksPerRead, blocksRemaining);
            const BlockCoord first{.x = x, .y = y};
            const auto offset = MapCommon::blockOffset(first, impl_->dimensions);
            const auto byteCount = blockCount * static_cast<std::size_t>(MapCommon::landBlockByteSize);
            if (MapCommon::blockId(first, impl_->dimensions) + blockCount > impl_->blocks ||
                !MapCommon::readExact(impl_->descriptor, buffer.data(), byteCount, offset))
            {
                for (std::size_t index = 0; index < blockCount; ++index)
                {
                    const BlockCoord coordinate{.x = x, .y = y + static_cast<std::uint32_t>(index)};
                    LandBlockBytes raw{};
                    if (auto loaded =
                            readRawLandBlock(impl_->descriptor, impl_->blocks, coordinate, impl_->dimensions, impl_->path, raw);
                        !loaded)
                    {
                        return std::unexpected(loaded.error());
                    }
                    if (auto result = callback(coordinate, raw); !result)
                    {
                        return result;
                    }
                }
            }
            else
            {
                for (std::size_t index = 0; index < blockCount; ++index)
                {
                    LandBlockBytes raw{};
                    const auto blockOffset = index * static_cast<std::size_t>(MapCommon::landBlockByteSize);
                    std::ranges::copy_n(
                        std::next(buffer.begin(), static_cast<std::ptrdiff_t>(blockOffset)), raw.size(), raw.begin());
                    const BlockCoord coordinate{.x = x, .y = y + static_cast<std::uint32_t>(index)};
                    if (auto result = callback(coordinate, raw); !result)
                    {
                        return result;
                    }
                }
            }
            y += static_cast<std::uint32_t>(blockCount);
        }
    }
    return {};
}

std::expected<MapStats, Error> MapReader::stats() const
{
    return tileStats(
        {.min = {.x = 0, .y = 0}, .maxExclusive = {.x = impl_->dimensions.widthTiles, .y = impl_->dimensions.heightTiles}});
}

std::expected<MapStats, Error> MapReader::tileStats(TileRegion region) const
{
    if (auto valid = MapCommon::validateRegion(region, impl_->dimensions, impl_->path); !valid)
    {
        return std::unexpected(valid.error());
    }
    return collectStats(*this, region);
}

std::expected<MapStats, Error> MapReader::blockStats(BlockRegion region) const
{
    if (auto valid = MapCommon::validateRegion(region, impl_->dimensions, impl_->path); !valid)
    {
        return std::unexpected(valid.error());
    }
    const TileRegion tiles{
        .min = {.x = region.min.x * landBlockDimension, .y = region.min.y * landBlockDimension},
        .maxExclusive = {.x = region.maxExclusive.x * landBlockDimension, .y = region.maxExclusive.y * landBlockDimension},
    };
    return collectStats(*this, tiles);
}

std::expected<void, Error> MapReader::readLandTiles(TileRegion region, const LandTileRead & callback) const
{
    if (auto valid = validateReadRequest(region, callback, impl_->dimensions, impl_->path); !valid)
    {
        return valid;
    }

    const auto blocks = detail::blockRegionForTiles(region);
    return readRawLandBlocks(
        blocks,
        [&callback, region](BlockCoord coordinate, const LandBlockBytes & raw) -> std::expected<void, Error>
        {
            const auto block = MapCommon::decodeBlock(raw);
            return detail::visitTilesInBlock(region,
                                             coordinate,
                                             [&callback, &block](TileCoord tile, std::size_t index)
                                             { return callback(tile, block.at(index)); });
        });
}

} // namespace uomul
