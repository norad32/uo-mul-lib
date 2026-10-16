#include "map_helper.h"
#include "uomul/map.h"
#include <algorithm>
#include <array>
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

using detail::MapHelper;

namespace
{

constexpr double medianDivisor = 2.0;

std::expected<void, Error> readRawLandBlock(int descriptor,
                                            std::uint64_t completeBlocks,
                                            BlockCoord block,
                                            Dimensions dimensions,
                                            const std::filesystem::path & path,
                                            LandBlockBytes & raw)
{
    const auto offset = MapHelper::blockOffset(block, dimensions);
    if (MapHelper::blockId(block, dimensions) >= completeBlocks ||
        !MapHelper::readExact(descriptor, raw.data(), raw.size(), offset))
    {
        return std::unexpected(
            MapHelper::makeError(ErrorCode::truncatedInput, path, offset, "land block is missing or truncated"));
    }
    return {};
}

template <typename Region, typename Callback>
std::expected<void, Error>
validateReadRequest(Region region, const Callback & callback, Dimensions dimensions, const std::filesystem::path & path)
{
    if (auto valid = MapHelper::validateRegion(region, dimensions, path); !valid)
    {
        return valid;
    }
    if (!callback)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, path, 0, "read callback is empty"));
    }
    return {};
}

std::expected<MapStats, Error> collectStats(const MapReader & reader, TileRegion region)
{
    if (region.min.x == region.maxExclusive.x || region.min.y == region.maxExclusive.y)
    {
        return MapStats{};
    }

    std::vector<std::uint64_t> idCounts(static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) + 1U);
    std::array<std::uint64_t, static_cast<std::size_t>(std::numeric_limits<std::uint8_t>::max()) + 1U> zCounts{};
    long double elevationTotal = 0.0L;
    MapStats result{};
    const auto blocks = detail::blockRegionForTiles(region);
    auto read = reader.readRawLandBlocks(
        blocks,
        [&idCounts, &zCounts, &elevationTotal, &result, region](BlockCoord coordinate,
                                                                const LandBlockBytes & raw) -> std::expected<void, Error>
        {
            const auto tiles = detail::clipTilesToBlock(region, coordinate);
            for (std::uint32_t x = tiles.min.x; x < tiles.maxExclusive.x; ++x)
            {
                for (std::uint32_t y = tiles.min.y; y < tiles.maxExclusive.y; ++y)
                {
                    const auto index = detail::tileIndexInBlock({.x = x, .y = y});
                    const auto offset = static_cast<std::size_t>(MapHelper::landHeaderByteSize) +
                                        (index * static_cast<std::size_t>(MapHelper::landTileByteSize));
                    const auto id = static_cast<std::uint16_t>(
                        static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(raw.at(offset))) |
                        (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(raw.at(offset + 1)))
                         << MapHelper::bitsPerByte));
                    const auto z = static_cast<std::int8_t>(std::to_integer<std::uint8_t>(raw.at(offset + 2)));
                    ++idCounts.at(id);
                    ++zCounts.at(static_cast<std::size_t>(static_cast<int>(z) - std::numeric_limits<std::int8_t>::min()));
                    elevationTotal += z;
                    ++result.tileCount;
                }
            }
            return {};
        });
    if (!read)
    {
        return std::unexpected(read.error());
    }
    if (result.tileCount == 0)
    {
        return result;
    }

    const double sampleCount = static_cast<double>(result.tileCount);
    for (std::size_t value = 0; value < idCounts.size(); ++value)
    {
        const auto count = idCounts.at(value);
        if (count != 0)
        {
            result.tileIds.push_back(ValueStats{.value = static_cast<std::uint16_t>(value),
                                                .count = count,
                                                .percentage = (static_cast<double>(count) * 100.0) / sampleCount});
        }
    }
    for (std::size_t index = 0; index < zCounts.size(); ++index)
    {
        const auto count = zCounts.at(index);
        if (count != 0)
        {
            const auto value = static_cast<std::int8_t>(static_cast<int>(index) + std::numeric_limits<std::int8_t>::min());
            result.zs.push_back(
                ValueStats{.value = value, .count = count, .percentage = (static_cast<double>(count) * 100.0) / sampleCount});
        }
    }
    result.averageZ = static_cast<double>(elevationTotal / static_cast<long double>(result.tileCount));
    const std::uint64_t lowerMiddleIndex = (result.tileCount - 1) / 2;
    const std::uint64_t upperMiddleIndex = result.tileCount / 2;
    std::uint64_t currentIndex = 0;
    std::optional<std::int8_t> lowerMiddle;
    std::optional<std::int8_t> upperMiddle;
    for (std::size_t index = 0; index < zCounts.size(); ++index)
    {
        const auto count = zCounts.at(index);
        const auto nextIndex = currentIndex + count;
        const auto value = static_cast<std::int8_t>(static_cast<int>(index) + std::numeric_limits<std::int8_t>::min());
        if (!lowerMiddle && lowerMiddleIndex < nextIndex)
        {
            lowerMiddle = value;
        }
        if (upperMiddleIndex < nextIndex)
        {
            upperMiddle = value;
            break;
        }
        currentIndex = nextIndex;
    }
    if (lowerMiddle && upperMiddle)
    {
        result.medianZ = (static_cast<double>(lowerMiddle.value()) + static_cast<double>(upperMiddle.value())) / medianDivisor;
    }
    return result;
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
    auto count = MapHelper::blockCount(dimensions, path);
    if (!count)
    {
        return std::unexpected(count.error());
    }

    detail::ScopedDescriptor descriptor(::open(path.c_str(), O_RDONLY | O_CLOEXEC));
    if (descriptor.get() < 0)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, path, 0, std::strerror(errno)));
    }

    struct stat fileStatus{};
    if (::fstat(descriptor.get(), &fileStatus) != 0)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, path, 0, std::strerror(errno)));
    }
    if (fileStatus.st_size < 0)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, path, 0, "could not determine map size"));
    }

    const auto fileSize = static_cast<std::uint64_t>(fileStatus.st_size);
    const auto expectedSize = *count * MapHelper::landBlockByteSize;
    const auto completeBlockCount = fileSize / MapHelper::landBlockByteSize;
    if (fileSize % MapHelper::landBlockByteSize != 0 && (fileSize > expectedSize || !options.recoverTruncatedMap))
    {
        return std::unexpected(
            MapHelper::makeError(ErrorCode::truncatedInput, path, fileSize, "map has trailing bytes beyond complete blocks"));
    }

    if (!options.recoverTruncatedMap && fileSize != expectedSize)
    {
        return std::unexpected(MapHelper::makeError(
            ErrorCode::truncatedInput, path, fileSize, "map size does not match dimensions and complete blocks"));
    }

    if (fileSize > expectedSize)
    {
        return std::unexpected(MapHelper::makeError(
            ErrorCode::truncatedInput, path, expectedSize, "map contains more blocks than specified dimensions"));
    }

    if (!S_ISREG(fileStatus.st_mode))
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, path, 0, "map input must be a regular file"));
    }

    auto impl = std::make_unique<Impl>();
    impl->path = path;
    impl->dimensions = dimensions;
    impl->descriptor = descriptor.release();
    impl->blocks = std::min(*count, completeBlockCount);
    impl->missingBlocks = *count - impl->blocks;
    impl->ignoredBytes = fileSize % MapHelper::landBlockByteSize;
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
    if (auto valid = MapHelper::validateBlock(block, impl_->dimensions, impl_->path); !valid)
    {
        return std::unexpected(valid.error());
    }

    LandBlockBytes raw{};
    if (auto loaded = readRawLandBlock(impl_->descriptor, impl_->blocks, block, impl_->dimensions, impl_->path, raw); !loaded)
    {
        return std::unexpected(loaded.error());
    }
    return MapHelper::decodeBlock(raw);
}

std::expected<LandTile, Error> MapReader::readLandTile(TileCoord tile) const
{
    if (auto valid = MapHelper::validateTile(tile, impl_->dimensions, impl_->path); !valid)
    {
        return std::unexpected(valid.error());
    }

    const BlockCoord block{.x = tile.x / landBlockDimension, .y = tile.y / landBlockDimension};
    const auto blockOffset = MapHelper::blockOffset(block, impl_->dimensions);
    if (MapHelper::blockId(block, impl_->dimensions) >= impl_->blocks)
    {
        return std::unexpected(
            MapHelper::makeError(ErrorCode::truncatedInput, impl_->path, blockOffset, "land block is missing or truncated"));
    }
    struct stat fileStatus{};
    if (::fstat(impl_->descriptor, &fileStatus) != 0)
    {
        return std::unexpected(MapHelper::makeError(ErrorCode::io, impl_->path, blockOffset, std::strerror(errno)));
    }
    if (fileStatus.st_size < 0 || static_cast<std::uint64_t>(fileStatus.st_size) < blockOffset + MapHelper::landBlockByteSize)
    {
        return std::unexpected(
            MapHelper::makeError(ErrorCode::truncatedInput, impl_->path, blockOffset, "land block is missing or truncated"));
    }
    constexpr std::size_t tileByteSize = static_cast<std::size_t>(MapHelper::landTileByteSize);
    std::array<std::byte, tileByteSize> raw{};
    const auto offset =
        blockOffset + MapHelper::landHeaderByteSize + (detail::tileIndexInBlock(tile) * MapHelper::landTileByteSize);
    if (!MapHelper::readExact(impl_->descriptor, raw.data(), raw.size(), offset))
    {
        return std::unexpected(
            MapHelper::makeError(ErrorCode::truncatedInput, impl_->path, offset, "land tile is missing or truncated"));
    }

    const auto low = std::to_integer<std::uint16_t>(raw.at(0));
    const auto high = std::to_integer<std::uint16_t>(raw.at(1));
    return LandTile{
        .id = static_cast<std::uint16_t>(low | static_cast<std::uint16_t>(high << MapHelper::bitsPerByte)),
        .z = std::bit_cast<std::int8_t>(std::to_integer<std::uint8_t>(raw.at(2))),
    };
}

std::expected<void, Error> MapReader::readLandBlocks(BlockRegion region, const LandBlockRead & callback) const
{
    if (auto valid = validateReadRequest(region, callback, impl_->dimensions, impl_->path); !valid)
    {
        return valid;
    }

    return readRawLandBlocks(region,
                             [&callback](BlockCoord coordinate, const LandBlockBytes & raw) -> std::expected<void, Error>
                             { return callback(coordinate, MapHelper::decodeBlock(raw)); });
}

std::expected<void, Error> MapReader::readRawLandBlocks(BlockRegion region, const LandBlockBytesRead & callback) const
{
    if (auto valid = validateReadRequest(region, callback, impl_->dimensions, impl_->path); !valid)
    {
        return valid;
    }

    constexpr std::size_t maximumBlocksPerRead = 4096;
    const auto blocksPerRead = std::min(maximumBlocksPerRead, static_cast<std::size_t>(region.maxExclusive.y - region.min.y));
    std::vector<std::byte> buffer(blocksPerRead * static_cast<std::size_t>(MapHelper::landBlockByteSize));
    for (std::uint32_t x = region.min.x; x < region.maxExclusive.x; ++x)
    {
        std::uint32_t y = region.min.y;
        while (y < region.maxExclusive.y)
        {
            const auto blocksRemaining = static_cast<std::size_t>(region.maxExclusive.y - y);
            const auto blockCount = std::min(blocksPerRead, blocksRemaining);
            const BlockCoord first{.x = x, .y = y};
            const auto offset = MapHelper::blockOffset(first, impl_->dimensions);
            const auto byteCount = blockCount * static_cast<std::size_t>(MapHelper::landBlockByteSize);
            if (MapHelper::blockId(first, impl_->dimensions) + blockCount > impl_->blocks ||
                !MapHelper::readExact(impl_->descriptor, buffer.data(), byteCount, offset))
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
                    const auto blockOffset = index * static_cast<std::size_t>(MapHelper::landBlockByteSize);
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
    if (auto valid = MapHelper::validateRegion(region, impl_->dimensions, impl_->path); !valid)
    {
        return std::unexpected(valid.error());
    }
    return collectStats(*this, region);
}

std::expected<MapStats, Error> MapReader::blockStats(BlockRegion region) const
{
    if (auto valid = MapHelper::validateRegion(region, impl_->dimensions, impl_->path); !valid)
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
            const auto block = MapHelper::decodeBlock(raw);
            return detail::visitTilesInBlock(region,
                                             coordinate,
                                             [&callback, &block](TileCoord tile, std::size_t index)
                                             { return callback(tile, block.at(index)); });
        });
}

} // namespace uomul
