#include "gui_model/documents/map_document_state.h"
#include "gui_model/tools/tool_contracts.h"
#include "gui_model/world/map_color.h"
#include "gui_model/world/map_selection.h"
#include "uomul/map/map.h"
#include "uomul/mul_common/mul_common.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <unordered_set>
#include <utility>
#include <vector>

namespace uo_mule::gui
{

namespace
{

constexpr std::uint32_t colorChannelMaximum = std::numeric_limits<std::uint8_t>::max();
constexpr std::uint32_t idColorRedMultiplier = 73;
constexpr std::uint32_t idColorGreenMultiplier = 151;
constexpr std::uint32_t idColorBlueMultiplier = 199;
constexpr std::uint32_t idColorOffset = 47;
constexpr std::uint32_t idColorFirstShift = 7;
constexpr std::uint32_t idColorSecondShift = 9;
constexpr std::uint32_t idColorThirdShift = 8;
constexpr std::uint32_t grayscaleRange = colorChannelMaximum;
constexpr std::uint32_t signedElevationRange = std::numeric_limits<std::uint8_t>::max();
constexpr std::uint32_t maximumTemporaryPathAttempts = 128;
constexpr unsigned coordinateKeyShift = 32U;
std::atomic<std::uint64_t> temporaryPathCounter = 0;

[[nodiscard]] uomul::Error makeMapDocumentError(const std::filesystem::path & path, std::string message)
{
    return {.code = uomul::ErrorCode::io, .path = path, .message = std::move(message)};
}

[[nodiscard]] std::expected<std::filesystem::path, uomul::Error> createWorkingPath(const std::filesystem::path & source)
{
    std::error_code filesystemError;
    const auto directory = std::filesystem::temp_directory_path(filesystemError);

    if (filesystemError)
    {
        return std::unexpected(makeMapDocumentError(source, filesystemError.message()));
    }

    for (std::uint32_t attempt = 0; attempt < maximumTemporaryPathAttempts; attempt++)
    {
        const auto sequence = temporaryPathCounter.fetch_add(1, std::memory_order_relaxed);
        const auto filename =
            "uomul-map-work-" + std::to_string(::getpid()) + "-" + std::to_string(sequence) + "-" + source.filename().string();
        auto candidate = directory / filename;
        const bool exists = std::filesystem::exists(candidate, filesystemError);

        if (filesystemError)
        {
            return std::unexpected(makeMapDocumentError(candidate, filesystemError.message()));
        }

        if (!exists)
        {
            return candidate;
        }
    }

    return std::unexpected(makeMapDocumentError(source, "unable to allocate a unique map working path"));
}

[[nodiscard]] std::uint32_t clampCoordinate(std::uint32_t coordinate, std::uint32_t limit) noexcept
{
    return std::min(coordinate, limit);
}

[[nodiscard]] std::uint64_t coordinateKey(uomul::BlockCoord coordinate) noexcept
{
    return (static_cast<std::uint64_t>(coordinate.x) << coordinateKeyShift) | coordinate.y;
}

[[nodiscard]] std::size_t tileIndex(uomul::TileCoord tile) noexcept
{
    const std::size_t row = static_cast<std::size_t>(tile.y % uomul::mapBlockDimension);
    const std::size_t column = static_cast<std::size_t>(tile.x % uomul::mapBlockDimension);
    return (row * static_cast<std::size_t>(uomul::mapBlockDimension)) + column;
}

[[nodiscard]] std::vector<uomul::BlockCoord> affectedBlocks(const MapSelectionMask & selection, uomul::Dimensions dimensions);

[[nodiscard]] bool selectedTile(const MapSelectionMask & selection, uomul::TileCoord tile, uomul::BlockCoord block) noexcept
{
    return selection.granularity() == ToolSelectionGranularity::tiles ? selection.contains(tile) : selection.contains(block);
}

[[nodiscard]] uomul::MapTile applyFill(uomul::MapTile current, uomul::MapTile value, FillMode mode) noexcept
{
    if (mode == FillMode::id || mode == FillMode::idAndZ)
    {
        current.id = value.id;
    }
    if (mode == FillMode::z || mode == FillMode::idAndZ)
    {
        current.z = value.z;
    }
    return current;
}

[[nodiscard]] bool replaceMatches(uomul::MapTile current, uomul::MapTile source, ReplaceMode mode) noexcept
{
    if (mode == ReplaceMode::id)
    {
        return current.id == source.id;
    }
    if (mode == ReplaceMode::z)
    {
        return current.z == source.z;
    }
    return current == source;
}

[[nodiscard]] uomul::MapTile applyReplace(uomul::MapTile current, uomul::MapTile target, ReplaceMode mode) noexcept
{
    if (mode == ReplaceMode::id || mode == ReplaceMode::idAndZ)
    {
        current.id = target.id;
    }
    if (mode == ReplaceMode::z || mode == ReplaceMode::idAndZ)
    {
        current.z = target.z;
    }
    return current;
}

template <typename Transform>
std::expected<std::uint64_t, uomul::Error>
editSelectedBlocks(const MapDocumentState & mapDocument, uomul::MapEditor & editor, Transform transform)
{
    std::uint64_t changedTiles = 0;
    for (const uomul::BlockCoord block :
         affectedBlocks(mapDocument.viewState().selectionMask_, mapDocument.changes().dimensions_))
    {
        const uomul::BlockRegion region{
            .min = block,
            .maxExclusive = {.x = block.x + 1U, .y = block.y + 1U},
        };
        auto edited = editor.editBlocks(
            region,
            [&mapDocument, block, &changedTiles, &transform](
                uomul::BlockCoord coordinate, const uomul::MapBlock & current) -> std::expected<uomul::MapBlock, uomul::Error>
            {
                uomul::MapBlock changed = current;
                const std::uint32_t minX = coordinate.x * uomul::mapBlockDimension;
                const std::uint32_t minY = coordinate.y * uomul::mapBlockDimension;
                const std::uint32_t maxX =
                    std::min(mapDocument.changes().dimensions_.widthTiles, minX + uomul::mapBlockDimension);
                const std::uint32_t maxY =
                    std::min(mapDocument.changes().dimensions_.heightTiles, minY + uomul::mapBlockDimension);
                for (std::uint32_t y = minY; y < maxY; y++)
                {
                    for (std::uint32_t x = minX; x < maxX; x++)
                    {
                        const uomul::TileCoord tile{.x = x, .y = y};
                        if (!selectedTile(mapDocument.viewState().selectionMask_, tile, block))
                        {
                            continue;
                        }
                        const std::size_t index = tileIndex(tile);
                        const uomul::MapTile next = transform(current.at(index));
                        if (next != current.at(index))
                        {
                            changedTiles++;
                            changed.at(index) = next;
                        }
                    }
                }
                return changed;
            });
        if (!edited)
        {
            return std::unexpected(edited.error());
        }
    }
    return changedTiles;
}

[[nodiscard]] std::vector<uomul::BlockCoord> affectedBlocks(const MapSelectionMask & selection, uomul::Dimensions dimensions)
{
    std::vector<uomul::BlockCoord> blocks;
    std::unordered_set<std::uint64_t> seen;
    const auto add = [&blocks, &seen](uomul::BlockCoord block)
    {
        if (seen.insert(coordinateKey(block)).second)
        {
            blocks.push_back(block);
        }
    };

    if (selection.granularity() == ToolSelectionGranularity::tiles)
    {
        selection.forEachSelectedTile(
            [&add](uomul::TileCoord tile)
            { add({.x = tile.x / uomul::mapBlockDimension, .y = tile.y / uomul::mapBlockDimension}); });
    }
    else
    {
        selection.forEachSelectedBlock(add);
    }

    const std::uint32_t blockWidth = (dimensions.widthTiles + uomul::mapBlockDimension - 1U) / uomul::mapBlockDimension;
    const std::uint32_t blockHeight = (dimensions.heightTiles + uomul::mapBlockDimension - 1U) / uomul::mapBlockDimension;
    std::erase_if(
        blocks, [blockWidth, blockHeight](uomul::BlockCoord block) { return block.x >= blockWidth || block.y >= blockHeight; });
    return blocks;
}

[[nodiscard]] std::string normalizeTiledataName(std::string_view value)
{
    std::string normalized;
    bool pendingSpace = false;
    for (const char character : value)
    {
        const auto unsignedCharacter = static_cast<unsigned char>(character);
        if (std::isspace(unsignedCharacter) != 0)
        {
            pendingSpace = !normalized.empty();
            continue;
        }
        if (pendingSpace)
        {
            normalized.push_back(' ');
            pendingSpace = false;
        }
        normalized.push_back(static_cast<char>(std::tolower(unsignedCharacter)));
    }
    return normalized;
}

[[nodiscard]] bool fuzzyTileMatches(const uomul::MapTile & tile,
                                    const FuzzySelectionOptions & options,
                                    const FuzzyTiledataNameLookup & tiledataNameLookup)
{
    if (!options.idEnabled && !options.zEnabled && !options.nameEnabled)
    {
        return false;
    }
    if (options.idEnabled && tile.id != options.id)
    {
        return false;
    }
    if (options.zEnabled && tile.z != options.z)
    {
        return false;
    }
    if (options.nameEnabled)
    {
        if (!tiledataNameLookup)
        {
            return false;
        }
        const auto name = tiledataNameLookup(tile.id);
        if (!name || normalizeTiledataName(*name) != normalizeTiledataName(options.name))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::vector<uomul::TileCoord> scanWholeMap(const uomul::MapReader & reader,
                                                         uomul::Dimensions dimensions,
                                                         const FuzzySelectionOptions & options,
                                                         const FuzzyTiledataNameLookup & lookup)
{
    std::vector<uomul::TileCoord> selected;
    for (std::uint32_t y = 0; y < dimensions.heightTiles; y++)
    {
        for (std::uint32_t x = 0; x < dimensions.widthTiles; x++)
        {
            const uomul::TileCoord tile{.x = x, .y = y};
            const auto value = reader.readTile(tile);
            if (value && fuzzyTileMatches(*value, options, lookup))
            {
                selected.push_back(tile);
            }
        }
    }
    return selected;
}

[[nodiscard]] std::vector<uomul::TileCoord> scanContiguous(const uomul::MapReader & reader,
                                                           uomul::Dimensions dimensions,
                                                           uomul::TileCoord origin,
                                                           const FuzzySelectionOptions & options,
                                                           const FuzzyTiledataNameLookup & lookup)
{
    std::vector<uomul::TileCoord> selected;
    std::queue<uomul::TileCoord> pending;
    std::unordered_set<std::uint64_t> visited;
    pending.push(origin);
    while (!pending.empty())
    {
        const uomul::TileCoord tile = pending.front();
        pending.pop();
        const std::uint64_t key = (static_cast<std::uint64_t>(tile.x) << coordinateKeyShift) | tile.y;
        if (!visited.insert(key).second)
        {
            continue;
        }
        const auto value = reader.readTile(tile);
        if (!value || !fuzzyTileMatches(*value, options, lookup))
        {
            continue;
        }
        selected.push_back(tile);
        if (tile.x > 0U)
            pending.push({.x = tile.x - 1U, .y = tile.y});
        if (tile.x + 1U < dimensions.widthTiles)
            pending.push({.x = tile.x + 1U, .y = tile.y});
        if (tile.y > 0U)
            pending.push({.x = tile.x, .y = tile.y - 1U});
        if (tile.y + 1U < dimensions.heightTiles)
            pending.push({.x = tile.x, .y = tile.y + 1U});
    }
    return selected;
}

struct SelectionSamples
{
    std::map<std::int32_t, std::uint64_t> ids;
    std::map<std::int32_t, std::uint64_t> zs;
    std::vector<std::int32_t> elevations;
    std::uint64_t tileCount = 0;
};

void addSample(SelectionSamples & samples, uomul::MapTile tile)
{
    samples.ids[static_cast<std::int32_t>(tile.id)]++;
    samples.zs[static_cast<std::int32_t>(tile.z)]++;
    samples.elevations.push_back(static_cast<std::int32_t>(tile.z));
    samples.tileCount++;
}

[[nodiscard]] uomul::MapStats makeStats(SelectionSamples samples)
{
    constexpr double medianDivisor = 2.0;
    uomul::MapStats stats{.tileCount = samples.tileCount, .tileIds = {}, .zs = {}, .averageZ = {}, .medianZ = {}};
    for (const auto [value, count] : samples.ids)
    {
        stats.tileIds.push_back({
            .value = value,
            .count = count,
            .percentage =
                samples.tileCount == 0U ? 0.0 : (100.0 * static_cast<double>(count) / static_cast<double>(samples.tileCount)),
        });
    }
    for (const auto [value, count] : samples.zs)
    {
        stats.zs.push_back({
            .value = value,
            .count = count,
            .percentage =
                samples.tileCount == 0U ? 0.0 : (100.0 * static_cast<double>(count) / static_cast<double>(samples.tileCount)),
        });
    }
    if (!samples.elevations.empty())
    {
        std::ranges::sort(samples.elevations);
        stats.averageZ = std::accumulate(samples.elevations.begin(), samples.elevations.end(), 0.0) /
                         static_cast<double>(samples.elevations.size());
        const std::size_t midpoint = samples.elevations.size() / 2U;
        stats.medianZ = samples.elevations.size() % 2U == 0U ? (static_cast<double>(samples.elevations.at(midpoint - 1U)) +
                                                                static_cast<double>(samples.elevations.at(midpoint))) /
                                                                   medianDivisor
                                                             : static_cast<double>(samples.elevations.at(midpoint));
    }
    return stats;
}

[[nodiscard]] SelectionSamples collectSelectionSamples(const MapDocumentState & mapDocument)
{
    SelectionSamples samples;
    const MapSelectionMask & selection = mapDocument.viewState().selectionMask_;
    const uomul::Dimensions dimensions = mapDocument.changes().dimensions_;
    const uomul::MapReader & reader = *mapDocument.changes().reader_;
    if (selection.granularity() == ToolSelectionGranularity::tiles)
    {
        selection.forEachSelectedTile(
            [&samples, &reader, dimensions](uomul::TileCoord tile)
            {
                if (tile.x < dimensions.widthTiles && tile.y < dimensions.heightTiles)
                {
                    const auto value = reader.readTile(tile);
                    if (value)
                        addSample(samples, *value);
                }
            });
        return samples;
    }

    selection.forEachSelectedBlock(
        [&samples, &reader, dimensions](uomul::BlockCoord block)
        {
            const auto value = reader.readBlock(block);
            if (!value)
                return;
            const std::uint32_t minX = block.x * uomul::mapBlockDimension;
            const std::uint32_t minY = block.y * uomul::mapBlockDimension;
            const std::uint32_t maxX = std::min(dimensions.widthTiles, minX + uomul::mapBlockDimension);
            const std::uint32_t maxY = std::min(dimensions.heightTiles, minY + uomul::mapBlockDimension);
            for (std::uint32_t y = minY; y < maxY; y++)
            {
                for (std::uint32_t x = minX; x < maxX; x++)
                {
                    addSample(samples, value->at(tileIndex({.x = x, .y = y})));
                }
            }
        });
    return samples;
}

} // namespace

uomul::BlockCoord tileToBlock(uomul::TileCoord tile) noexcept
{
    return {.x = tile.x / uomul::mapBlockDimension, .y = tile.y / uomul::mapBlockDimension};
}

uomul::TileCoord selectionInspectorTile(MapSelectionMode mode, uomul::TileCoord tile) noexcept
{
    if (mode == MapSelectionMode::blocks)
    {
        return {
            .x = (tile.x / uomul::mapBlockDimension) * uomul::mapBlockDimension,
            .y = (tile.y / uomul::mapBlockDimension) * uomul::mapBlockDimension,
        };
    }

    return tile;
}

std::optional<uomul::TileCoord> screenToTile(double screenX,
                                             double screenY,
                                             double canvasX,
                                             double canvasY,
                                             double panX,
                                             double panY,
                                             double tileSize,
                                             uomul::Dimensions dimensions) noexcept
{
    if (!std::isfinite(screenX) || !std::isfinite(screenY) || !std::isfinite(canvasX) || !std::isfinite(canvasY) ||
        !std::isfinite(panX) || !std::isfinite(panY) || !std::isfinite(tileSize) || tileSize <= 0.0)
    {
        return std::nullopt;
    }

    const double tileX = std::floor((screenX - canvasX - panX) / tileSize);
    const double tileY = std::floor((screenY - canvasY - panY) / tileSize);

    if (tileX < 0.0 || tileY < 0.0 || tileX >= dimensions.widthTiles || tileY >= dimensions.heightTiles)
    {
        return std::nullopt;
    }

    return uomul::TileCoord{.x = static_cast<std::uint32_t>(tileX), .y = static_cast<std::uint32_t>(tileY)};
}

uomul::TileRegion clampTileRegion(uomul::TileRegion region, uomul::Dimensions dimensions) noexcept
{
    region.min.x = clampCoordinate(region.min.x, dimensions.widthTiles);
    region.min.y = clampCoordinate(region.min.y, dimensions.heightTiles);
    region.maxExclusive.x = clampCoordinate(region.maxExclusive.x, dimensions.widthTiles);
    region.maxExclusive.y = clampCoordinate(region.maxExclusive.y, dimensions.heightTiles);
    region.maxExclusive.x = std::max(region.min.x, region.maxExclusive.x);
    region.maxExclusive.y = std::max(region.min.y, region.maxExclusive.y);
    return region;
}

uomul::BlockRegion clampBlockRegion(uomul::BlockRegion region, uomul::Dimensions dimensions) noexcept
{
    const std::uint32_t blockWidth = (dimensions.widthTiles / uomul::mapBlockDimension) +
                                     static_cast<std::uint32_t>(dimensions.widthTiles % uomul::mapBlockDimension != 0);
    const std::uint32_t blockHeight = (dimensions.heightTiles / uomul::mapBlockDimension) +
                                      static_cast<std::uint32_t>(dimensions.heightTiles % uomul::mapBlockDimension != 0);
    region.min.x = clampCoordinate(region.min.x, blockWidth);
    region.min.y = clampCoordinate(region.min.y, blockHeight);
    region.maxExclusive.x = clampCoordinate(region.maxExclusive.x, blockWidth);
    region.maxExclusive.y = clampCoordinate(region.maxExclusive.y, blockHeight);
    region.maxExclusive.x = std::max(region.min.x, region.maxExclusive.x);
    region.maxExclusive.y = std::max(region.min.y, region.maxExclusive.y);
    return region;
}

uomul::TileRegion tileSelectionFromDrag(uomul::TileCoord first, uomul::TileCoord last, uomul::Dimensions dimensions) noexcept
{
    const uomul::TileCoord minimum{.x = std::min(first.x, last.x), .y = std::min(first.y, last.y)};
    const uomul::TileCoord maximum{.x = std::max(first.x, last.x), .y = std::max(first.y, last.y)};
    const uomul::TileRegion region{
        .min = minimum,
        .maxExclusive =
            {
                .x = maximum.x == std::numeric_limits<std::uint32_t>::max() ? maximum.x : maximum.x + 1,
                .y = maximum.y == std::numeric_limits<std::uint32_t>::max() ? maximum.y : maximum.y + 1,
            },
    };

    return clampTileRegion(region, dimensions);
}

uomul::BlockRegion blockSelectionFromDrag(uomul::TileCoord first, uomul::TileCoord last, uomul::Dimensions dimensions) noexcept
{
    const uomul::BlockCoord firstBlock = tileToBlock(first);
    const uomul::BlockCoord lastBlock = tileToBlock(last);
    const uomul::BlockCoord minimum{.x = std::min(firstBlock.x, lastBlock.x), .y = std::min(firstBlock.y, lastBlock.y)};
    const uomul::BlockCoord maximum{.x = std::max(firstBlock.x, lastBlock.x), .y = std::max(firstBlock.y, lastBlock.y)};
    const uomul::BlockRegion region{
        .min = minimum,
        .maxExclusive =
            {
                .x = maximum.x == std::numeric_limits<std::uint32_t>::max() ? maximum.x : maximum.x + 1,
                .y = maximum.y == std::numeric_limits<std::uint32_t>::max() ? maximum.y : maximum.y + 1,
            },
    };

    return clampBlockRegion(region, dimensions);
}

MapColor tileIdColor(std::uint16_t id) noexcept
{
    std::uint32_t value = static_cast<std::uint32_t>(id) + idColorOffset;
    value ^= value >> idColorFirstShift;
    value *= idColorRedMultiplier;
    const auto red = static_cast<std::uint8_t>((value >> 8U) & colorChannelMaximum);
    value ^= value >> idColorSecondShift;
    value *= idColorGreenMultiplier;
    const auto green = static_cast<std::uint8_t>((value >> 8U) & colorChannelMaximum);
    value ^= value >> idColorThirdShift;
    value *= idColorBlueMultiplier;
    const auto blue = static_cast<std::uint8_t>((value >> 8U) & colorChannelMaximum);
    return {.red = red, .green = green, .blue = blue};
}

MapColor tileDisplayColor(MapDisplayMode mode, uomul::MapTile tile, std::optional<MapColor> radarColor) noexcept
{
    if (mode == MapDisplayMode::z)
    {
        const std::uint8_t gray = zGrayscale(tile.z);
        return {.red = gray, .green = gray, .blue = gray};
    }

    MapColor color = radarColor.value_or(tileIdColor(tile.id));
    if (mode == MapDisplayMode::combined)
    {
        constexpr double combinedBrightnessMinimum = 0.5;
        constexpr double combinedBrightnessRange = 1.0;
        const double brightness =
            combinedBrightnessMinimum +
            ((static_cast<double>(zGrayscale(tile.z)) / static_cast<double>(colorChannelMaximum)) * combinedBrightnessRange);
        color.red = static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(static_cast<double>(color.red) * brightness), 0, static_cast<int>(colorChannelMaximum)));
        color.green = static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(static_cast<double>(color.green) * brightness), 0, static_cast<int>(colorChannelMaximum)));
        color.blue = static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(static_cast<double>(color.blue) * brightness), 0, static_cast<int>(colorChannelMaximum)));
    }

    return color;
}

std::uint8_t zGrayscale(std::int8_t z) noexcept
{
    const auto shifted = static_cast<std::int32_t>(z) - std::numeric_limits<std::int8_t>::min();
    const auto mapped = static_cast<std::uint32_t>(shifted) * grayscaleRange / signedElevationRange;
    return static_cast<std::uint8_t>(mapped);
}

MapDocumentState::~MapDocumentState()
{
    changes_.reader_.reset();
    removeWorkingFile();
}

std::expected<void, uomul::Error> MapDocumentState::openMap(const std::filesystem::path & path,
                                                            uomul::Dimensions dimensions,
                                                            PendingChangesAction action,
                                                            const std::optional<std::filesystem::path> & savePath,
                                                            bool overwriteSave)
{
    auto candidate = uomul::MapReader::open(path, dimensions);
    if (!candidate)
    {
        return fail(candidate.error());
    }

    if (auto resolved = resolvePendingChanges(action, savePath, overwriteSave); !resolved)
    {
        return resolved;
    }

    candidate = uomul::MapReader::open(path, dimensions);
    if (!candidate)
    {
        return fail(candidate.error());
    }

    replaceDocument(std::move(*candidate), path, path);
    return {};
}

std::expected<void, uomul::Error> MapDocumentState::createEmptyMap(const std::filesystem::path & path,
                                                                   uomul::Dimensions dimensions,
                                                                   PendingChangesAction action,
                                                                   const std::optional<std::filesystem::path> & savePath,
                                                                   bool overwriteOutput,
                                                                   uomul::MapTile initialTile)
{
    if (changes_.reader_)
    {
        std::error_code currentPathError;
        std::error_code originalPathError;
        std::error_code outputPathError;
        const auto currentPath = std::filesystem::weakly_canonical(changes_.mapPath_, currentPathError);
        const auto sourcePath = std::filesystem::weakly_canonical(changes_.originalPath_, originalPathError);
        const auto canonicalOutput = std::filesystem::weakly_canonical(path, outputPathError);

        if (!outputPathError &&
            ((!currentPathError && currentPath == canonicalOutput) || (!originalPathError && sourcePath == canonicalOutput)))
        {
            return fail(makeMapDocumentError(path, "new map output must differ from the open source and working maps"));
        }
    }

    if (changes_.reader_ && changes_.dirty() && action == PendingChangesAction::save && (!savePath || *savePath == path))
    {
        return fail(makeMapDocumentError(path, "cannot replace the requested new-map path while saving staged changes"));
    }

    if (auto resolved = resolvePendingChanges(action, savePath, overwriteOutput); !resolved)
    {
        return resolved;
    }

    std::error_code filesystemError;
    const bool outputAlreadyExists = std::filesystem::exists(path, filesystemError);

    if (filesystemError)
    {
        return fail(makeMapDocumentError(path, filesystemError.message()));
    }

    if (auto created = uomul::MapEditor::createEmpty(path, dimensions, initialTile, {.overwriteOutput = overwriteOutput});
        !created)
    {
        return fail(created.error());
    }

    auto opened = uomul::MapReader::open(path, dimensions);

    if (!opened)
    {
        if (!outputAlreadyExists)
        {
            std::error_code ignoredError;
            std::filesystem::remove(path, ignoredError);
        }
        return fail(opened.error());
    }

    replaceDocument(std::move(*opened), path, path);
    return {};
}

std::expected<void, uomul::Error> MapDocumentState::save(const std::filesystem::path & outputPath, bool overwriteOutput)
{
    if (!changes_.reader_)
    {
        return fail(makeMapDocumentError(outputPath, "no map is open"));
    }

    if (outputPath.empty())
    {
        return fail(makeMapDocumentError(outputPath, "save output path cannot be empty"));
    }

    if (!changes_.dirty())
    {
        return {};
    }

    auto inPlace = uomul::detail::MulCommon::pathsExactlyMatch(changes_.mapPath_, outputPath);
    if (!inPlace)
    {
        return fail(inPlace.error());
    }

    auto replacesOriginal = uomul::detail::MulCommon::pathsExactlyMatch(changes_.originalPath_, outputPath);
    if (!replacesOriginal)
    {
        return fail(replacesOriginal.error());
    }

    auto editor = uomul::MapEditor::create(changes_.mapPath_,
                                           outputPath,
                                           changes_.dimensions_,
                                           {.overwriteOutput = overwriteOutput || *inPlace || *replacesOriginal});

    if (!editor)
    {
        return fail(editor.error());
    }

    if (*inPlace)
    {
        changes_.reader_.reset();
    }

    if (auto committed = editor->commit(); !committed)
    {
        if (*inPlace)
        {
            auto restored = uomul::MapReader::open(changes_.mapPath_, changes_.dimensions_);
            if (restored)
            {
                changes_.reader_ = std::move(*restored);
            }
        }
        return fail(committed.error());
    }

    auto opened = uomul::MapReader::open(outputPath, changes_.dimensions_);
    if (!opened)
    {
        return fail(opened.error());
    }

    const MapSelectionMode selectionMode = viewState_.selectionMode_;
    const auto tileSelection = viewState_.tileSelection_;
    const auto blockSelection = viewState_.blockSelection_;
    replaceDocument(std::move(*opened), outputPath, outputPath);
    viewState_.selectionMode_ = selectionMode;
    viewState_.tileSelection_ = tileSelection;
    viewState_.blockSelection_ = blockSelection;

    return {};
}

std::expected<void, uomul::Error> MapDocumentState::discard()
{
    if (!changes_.dirty())
    {
        return {};
    }

    auto opened = uomul::MapReader::open(changes_.originalPath_, changes_.dimensions_);
    if (!opened)
    {
        return fail(opened.error());
    }

    replaceDocument(std::move(*opened), changes_.originalPath_, changes_.originalPath_);
    return {};
}

std::expected<void, uomul::Error> MapDocumentState::closeMap()
{
    if (changes_.dirty())
    {
        return fail(makeMapDocumentError(changes_.mapPath_, "pending map changes require save or discard before closing"));
    }

    changes_.reader_.reset();
    removeWorkingFile();
    changes_.dimensions_ = {};
    changes_.mapPath_.clear();
    changes_.originalPath_.clear();
    viewState_.tileSelection_.reset();
    viewState_.blockSelection_.reset();
    viewState_.selectionMask_.clear();
    viewState_.clearHover();
    viewState_.clearSelectedTile();
    changes_.error_.reset();

    return {};
}

std::expected<void, uomul::Error> MapDocumentState::fill(uomul::MapTile value, MapEditScope scope)
{
    return applyEdit(
        [this, value, scope](uomul::MapEditor & editor, const MapDocumentState &) -> std::expected<void, uomul::Error>
        {
            if (scope == MapEditScope::wholeMap)
            {
                return editor.fill(value);
            }

            if (viewState_.selectionMode_ == MapSelectionMode::tiles && viewState_.tileSelection_)
            {
                return editor.fillTiles(*viewState_.tileSelection_, value);
            }

            if (viewState_.selectionMode_ == MapSelectionMode::blocks && viewState_.blockSelection_)
            {
                return editor.fillBlocks(*viewState_.blockSelection_, value);
            }

            return std::unexpected(makeMapDocumentError(changes_.mapPath_, "selection scope requires a selection"));
        });
}

std::expected<void, uomul::Error> MapDocumentState::replaceTileId(std::uint16_t from, std::uint16_t to, MapEditScope scope)
{
    return applyEdit(
        [this, from, to, scope](uomul::MapEditor & editor, const MapDocumentState &) -> std::expected<void, uomul::Error>
        {
            if (scope == MapEditScope::wholeMap)
            {
                return editor.replaceIds(from, to);
            }

            if (viewState_.selectionMode_ == MapSelectionMode::tiles && viewState_.tileSelection_)
            {
                return editor.replaceIdsTiles(*viewState_.tileSelection_, from, to);
            }

            if (viewState_.selectionMode_ == MapSelectionMode::blocks && viewState_.blockSelection_)
            {
                return editor.replaceIdsBlocks(*viewState_.blockSelection_, from, to);
            }

            return std::unexpected(makeMapDocumentError(changes_.mapPath_, "selection scope requires a selection"));
        });
}

std::expected<void, uomul::Error> MapDocumentState::replaceZ(std::int8_t from, std::int8_t to, MapEditScope scope)
{
    return applyEdit(
        [this, from, to, scope](uomul::MapEditor & editor, const MapDocumentState &) -> std::expected<void, uomul::Error>
        {
            if (scope == MapEditScope::wholeMap)
            {
                return editor.replaceZ(from, to);
            }

            if (viewState_.selectionMode_ == MapSelectionMode::tiles && viewState_.tileSelection_)
            {
                return editor.replaceZTiles(*viewState_.tileSelection_, from, to);
            }

            if (viewState_.selectionMode_ == MapSelectionMode::blocks && viewState_.blockSelection_)
            {
                return editor.replaceZBlocks(*viewState_.blockSelection_, from, to);
            }

            return std::unexpected(makeMapDocumentError(changes_.mapPath_, "selection scope requires a selection"));
        });
}

std::expected<MapEditResult, uomul::Error> MapDocumentState::fillMasked(uomul::MapTile value, FillMode mode)
{
    if (viewState_.selectionMask_.empty())
    {
        const auto failure = fail(makeMapDocumentError(changes_.mapPath_, "selection scope requires a selection"));
        return std::unexpected(failure.error());
    }

    return applyCountedEdit(
        [value, mode](uomul::MapEditor & editor, const MapDocumentState & mapDocument)
        {
            return editSelectedBlocks(
                mapDocument, editor, [value, mode](uomul::MapTile tile) { return applyFill(tile, value, mode); });
        });
}

std::expected<MapEditResult, uomul::Error>
MapDocumentState::replaceMasked(ReplaceMode mode, uomul::MapTile source, uomul::MapTile target)
{
    if (viewState_.selectionMask_.empty())
    {
        const auto failure = fail(makeMapDocumentError(changes_.mapPath_, "selection scope requires a selection"));
        return std::unexpected(failure.error());
    }

    return applyCountedEdit(
        [mode, source, target](uomul::MapEditor & editor, const MapDocumentState & mapDocument)
        {
            return editSelectedBlocks(mapDocument,
                                      editor,
                                      [mode, source, target](uomul::MapTile tile)
                                      { return replaceMatches(tile, source, mode) ? applyReplace(tile, target, mode) : tile; });
        });
}

std::expected<std::size_t, uomul::Error> MapDocumentState::fuzzySelect(const FuzzySelectionOptions & options,
                                                                       uomul::TileCoord origin,
                                                                       const FuzzyTiledataNameLookup & tiledataNameLookup)
{
    if (!changes_.reader_)
    {
        return std::unexpected(fail(makeMapDocumentError(changes_.mapPath_, "no map is open")).error());
    }
    if (viewState_.selectionMask_.granularity() == ToolSelectionGranularity::blocks)
    {
        return std::unexpected(fail(makeMapDocumentError(changes_.mapPath_, "Fuzzy Select supports tiles only")).error());
    }
    if (!options.idEnabled && !options.zEnabled && !options.nameEnabled)
    {
        return std::unexpected(fail(makeMapDocumentError(changes_.mapPath_, "enable at least one fuzzy criterion")).error());
    }
    if (options.nameEnabled && !tiledataNameLookup)
    {
        return std::unexpected(fail(makeMapDocumentError(changes_.mapPath_, "Tiledata name matching is unavailable")).error());
    }
    if (origin.x >= changes_.dimensions_.widthTiles || origin.y >= changes_.dimensions_.heightTiles)
    {
        return std::unexpected(
            fail(makeMapDocumentError(changes_.mapPath_, "fuzzy selection origin is outside the map")).error());
    }

    if (options.scope == FuzzyScope::contiguous)
    {
        const auto originValue = changes_.reader_->readTile(origin);
        if (!originValue)
        {
            return std::unexpected(originValue.error());
        }
        if (!fuzzyTileMatches(*originValue, options, tiledataNameLookup))
        {
            viewState_.selectionMask_.clear();
            viewState_.tileSelection_.reset();
            viewState_.blockSelection_.reset();
            viewState_.selectionMode_ = MapSelectionMode::tiles;
            changes_.error_.reset();
            return std::size_t{0};
        }
    }

    const std::vector<uomul::TileCoord> selected =
        options.scope == FuzzyScope::wholeMap
            ? scanWholeMap(*changes_.reader_, changes_.dimensions_, options, tiledataNameLookup)
            : scanContiguous(*changes_.reader_, changes_.dimensions_, origin, options, tiledataNameLookup);

    viewState_.selectionMask_.clear();
    viewState_.selectionMask_.setGranularity(ToolSelectionGranularity::tiles);
    for (const uomul::TileCoord tile : selected)
    {
        viewState_.selectionMask_.selectTile(tile, changes_.dimensions_);
    }
    viewState_.tileSelection_.reset();
    viewState_.blockSelection_.reset();
    viewState_.selectionMode_ = MapSelectionMode::tiles;
    changes_.error_.reset();
    return selected.size();
}

std::expected<std::optional<uomul::MapStats>, uomul::Error> MapDocumentState::selectionStats()
{
    if (!changes_.reader_)
    {
        return std::optional<uomul::MapStats>{};
    }

    if (viewState_.selectionMask_.empty())
    {
        return std::optional<uomul::MapStats>{};
    }

    return std::optional<uomul::MapStats>{makeStats(collectSelectionSamples(*this))};
}

std::expected<void, uomul::Error> MapDocumentState::updateHover(uomul::TileCoord tile)
{
    if (!changes_.reader_)
    {
        viewState_.clearHover();
        return fail(makeMapDocumentError(changes_.mapPath_, "no map is open"));
    }

    auto value = changes_.reader_->readTile(tile);
    if (!value)
    {
        viewState_.clearHover();
        return fail(value.error());
    }

    viewState_.hoveredTile_ = tile;
    viewState_.hoveredValue_ = *value;
    changes_.error_.reset();

    return {};
}

std::expected<void, uomul::Error> MapDocumentState::updateSelectedTile(uomul::TileCoord tile)
{
    if (!changes_.reader_)
    {
        viewState_.clearSelectedTile();
        return fail(makeMapDocumentError(changes_.mapPath_, "no map is open"));
    }

    auto value = changes_.reader_->readTile(tile);
    if (!value)
    {
        viewState_.clearSelectedTile();
        return fail(value.error());
    }

    viewState_.selectedTile_ = tile;
    viewState_.selectedValue_ = *value;
    changes_.error_.reset();

    return {};
}

std::expected<void, uomul::Error> MapDocumentState::resolvePendingChanges(PendingChangesAction action,
                                                                          const std::optional<std::filesystem::path> & savePath,
                                                                          bool overwriteSave)
{
    if (!changes_.dirty())
    {
        return {};
    }

    if (action == PendingChangesAction::cancel)
    {
        return fail(makeMapDocumentError(changes_.mapPath_, "pending map changes require save or discard"));
    }

    if (action == PendingChangesAction::discard)
    {
        return discard();
    }

    if (!savePath)
    {
        return fail(makeMapDocumentError(changes_.mapPath_, "a save path is required to resolve pending map changes"));
    }

    return save(*savePath, overwriteSave);
}

std::expected<void, uomul::Error> MapDocumentState::applyEdit(const EditOperation & operation)
{
    if (!changes_.reader_)
    {
        return fail(makeMapDocumentError(changes_.mapPath_, "no map is open"));
    }

    const auto sourcePath = changes_.mapPath_;
    auto outputPath = createWorkingPath(sourcePath);

    if (!outputPath)
    {
        return fail(outputPath.error());
    }

    auto editor = uomul::MapEditor::create(sourcePath, *outputPath, changes_.dimensions_);
    if (!editor)
    {
        return fail(editor.error());
    }

    if (auto edited = operation(*editor, *this); !edited)
    {
        return fail(edited.error());
    }

    if (auto committed = editor->commit(); !committed)
    {
        return fail(committed.error());
    }

    auto opened = uomul::MapReader::open(*outputPath, changes_.dimensions_);
    if (!opened)
    {
        std::error_code ignoredError;
        std::filesystem::remove(*outputPath, ignoredError);
        return fail(opened.error());
    }

    const auto selectionMode = viewState_.selectionMode_;
    const auto tileSelection = viewState_.tileSelection_;
    const auto blockSelection = viewState_.blockSelection_;
    const auto selectionMask = viewState_.selectionMask_;
    replaceDocument(std::move(*opened), *outputPath, changes_.originalPath_, *outputPath);
    viewState_.selectionMode_ = selectionMode;
    viewState_.tileSelection_ = tileSelection;
    viewState_.blockSelection_ = blockSelection;
    viewState_.selectionMask_ = selectionMask;
    changes_.error_.reset();

    return {};
}

std::expected<MapEditResult, uomul::Error> MapDocumentState::applyCountedEdit(const CountedEditOperation & operation)
{
    if (!changes_.reader_)
    {
        const auto failure = fail(makeMapDocumentError(changes_.mapPath_, "no map is open"));
        return std::unexpected(failure.error());
    }

    const auto sourcePath = changes_.mapPath_;
    auto outputPath = createWorkingPath(sourcePath);
    if (!outputPath)
    {
        const auto failure = fail(outputPath.error());
        return std::unexpected(failure.error());
    }

    auto editor = uomul::MapEditor::create(sourcePath, *outputPath, changes_.dimensions_);
    if (!editor)
    {
        const auto failure = fail(editor.error());
        return std::unexpected(failure.error());
    }

    auto changed = operation(*editor, *this);
    if (!changed)
    {
        const auto failure = fail(changed.error());
        return std::unexpected(failure.error());
    }

    if (auto committed = editor->commit(); !committed)
    {
        const auto failure = fail(committed.error());
        return std::unexpected(failure.error());
    }

    auto opened = uomul::MapReader::open(*outputPath, changes_.dimensions_);
    if (!opened)
    {
        std::error_code ignoredError;
        std::filesystem::remove(*outputPath, ignoredError);
        const auto failure = fail(opened.error());
        return std::unexpected(failure.error());
    }

    const auto selectionMode = viewState_.selectionMode_;
    const auto tileSelection = viewState_.tileSelection_;
    const auto blockSelection = viewState_.blockSelection_;
    const auto selectionMask = viewState_.selectionMask_;
    replaceDocument(std::move(*opened), *outputPath, changes_.originalPath_, *outputPath);
    viewState_.selectionMode_ = selectionMode;
    viewState_.tileSelection_ = tileSelection;
    viewState_.blockSelection_ = blockSelection;
    viewState_.selectionMask_ = selectionMask;
    changes_.error_.reset();

    return MapEditResult{.changedTiles = *changed};
}

std::expected<void, uomul::Error> MapDocumentState::fail(uomul::Error error)
{
    changes_.error_ = error;
    return std::unexpected(std::move(error));
}

void MapDocumentState::replaceDocument(uomul::MapReader reader,
                                       std::filesystem::path path,
                                       std::filesystem::path originalPath,
                                       std::optional<std::filesystem::path> workingPath)
{
    const auto previousWorkingPath = changes_.workingPath_;
    changes_.reader_ = std::move(reader);
    changes_.dimensions_ = changes_.reader_->dimensions();
    changes_.mapPath_ = std::move(path);
    changes_.originalPath_ = std::move(originalPath);
    changes_.workingPath_ = std::move(workingPath);
    viewState_.tileSelection_.reset();
    viewState_.blockSelection_.reset();
    viewState_.clearHover();
    viewState_.clearSelectedTile();
    changes_.error_.reset();

    if (previousWorkingPath && (!changes_.workingPath_ || *previousWorkingPath != *changes_.workingPath_))
    {
        std::error_code ignoredError;
        std::filesystem::remove(*previousWorkingPath, ignoredError);
    }
}

void MapDocumentState::removeWorkingFile() noexcept
{
    if (changes_.workingPath_)
    {
        std::error_code ignoredError;
        std::filesystem::remove(*changes_.workingPath_, ignoredError);
        changes_.workingPath_.reset();
    }
}

} // namespace uo_mule::gui
