#include "map_stats.h"
#include "map_common.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace uomul::detail
{

namespace
{

constexpr double medianDivisor = 2.0;
constexpr std::size_t landIdCount = static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) + 1U;
constexpr std::size_t elevationCount = static_cast<std::size_t>(std::numeric_limits<std::uint8_t>::max()) + 1U;

class MedianElevationFinder
{
public:
    explicit MedianElevationFinder(std::uint64_t tileCount) :
        lowerMiddleRank_((tileCount - 1) / 2), upperMiddleRank_(tileCount / 2)
    {
    }

    [[nodiscard]] bool addElevationBucket(std::size_t index, std::uint64_t count)
    {
        cumulativeTileCount_ += count;
        const auto elevation = static_cast<std::int8_t>(static_cast<int>(index) + std::numeric_limits<std::int8_t>::min());
        if (!lowerMiddleElevation_ && lowerMiddleRank_ < cumulativeTileCount_)
        {
            lowerMiddleElevation_ = elevation;
        }
        if (lowerMiddleElevation_ && upperMiddleRank_ < cumulativeTileCount_)
        {
            median_ = (static_cast<double>(*lowerMiddleElevation_) + static_cast<double>(elevation)) / medianDivisor;
        }
        return median_.has_value();
    }

    [[nodiscard]] std::optional<double> median() const { return median_; }

private:
    std::uint64_t lowerMiddleRank_;
    std::uint64_t upperMiddleRank_;
    std::uint64_t cumulativeTileCount_ = 0;
    std::optional<std::int8_t> lowerMiddleElevation_;
    std::optional<double> median_;
};

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
        MedianElevationFinder finder(tileCount_);
        for (std::size_t index = 0; index < elevationCounts_.size(); ++index)
        {
            if (finder.addElevationBucket(index, elevationCounts_[index]))
            {
                break;
            }
        }
        return finder.median();
    }

    std::vector<std::uint64_t> idCounts_ = std::vector<std::uint64_t>(landIdCount);
    std::array<std::uint64_t, elevationCount> elevationCounts_{};
    long double elevationTotal_ = 0.0L;
    std::uint64_t tileCount_ = 0;
};

void accumulateBlockStats(StatsAccumulator & stats, TileRegion region, BlockCoord block, const LandBlockBytes & raw)
{
    const auto tiles = clipTilesToBlock(region, block);
    for (std::uint32_t x = tiles.min.x; x < tiles.maxExclusive.x; ++x)
    {
        for (std::uint32_t y = tiles.min.y; y < tiles.maxExclusive.y; ++y)
        {
            const auto tileIndex = tileIndexInBlock({.x = x, .y = y});
            const auto tileOffset = static_cast<std::size_t>(MapCommon::landHeaderByteSize) +
                                    (tileIndex * static_cast<std::size_t>(MapCommon::landTileByteSize));
            stats.add(decodeLandTile(raw.data() + tileOffset));
        }
    }
}

} // namespace

std::expected<MapStats, Error> collectStats(const MapReader & reader, TileRegion region)
{
    if (region.min.x == region.maxExclusive.x || region.min.y == region.maxExclusive.y)
    {
        return MapStats{};
    }

    StatsAccumulator stats;
    const auto blocks = blockRegionForTiles(region);
    auto read = reader.readRawLandBlocks(
        blocks,
        [&stats, region](BlockCoord coordinate, const LandBlockBytes & raw) -> std::expected<void, Error>
        {
            accumulateBlockStats(stats, region, coordinate, raw);
            return {};
        });
    if (!read)
    {
        return std::unexpected(read.error());
    }
    return stats.finish();
}

} // namespace uomul::detail
