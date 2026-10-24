#include "gui_model/workspaces/map_workspace.h"
#include "gui_model/world/map_color.h"
#include "uomul/map/map.h"
#include "uomul/mul_common/mul_common.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_set>
#include <unistd.h>
#include <utility>

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
std::atomic<std::uint64_t> temporaryPathCounter = 0;

[[nodiscard]] uomul::Error makeWorkspaceError(const std::filesystem::path & path, std::string message)
{
    return {.code = uomul::ErrorCode::io, .path = path, .message = std::move(message)};
}

[[nodiscard]] std::expected<std::filesystem::path, uomul::Error> createWorkingPath(const std::filesystem::path & source)
{
    std::error_code filesystemError;
    const auto directory = std::filesystem::temp_directory_path(filesystemError);

    if (filesystemError)
    {
        return std::unexpected(makeWorkspaceError(source, filesystemError.message()));
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
            return std::unexpected(makeWorkspaceError(candidate, filesystemError.message()));
        }

        if (!exists)
        {
            return candidate;
        }
    }

    return std::unexpected(makeWorkspaceError(source, "unable to allocate a unique map working path"));
}

[[nodiscard]] std::uint32_t clampCoordinate(std::uint32_t coordinate, std::uint32_t limit) noexcept
{
    return std::min(coordinate, limit);
}

[[nodiscard]] std::uint64_t coordinateKey(uomul::BlockCoord coordinate) noexcept
{
    return (static_cast<std::uint64_t>(coordinate.x) << 32U) | coordinate.y;
}

[[nodiscard]] std::vector<uomul::BlockCoord> affectedBlocks(const MapSelectionMask & selection,
                                                             uomul::Dimensions dimensions)
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
        selection.forEachSelectedTile([&add](uomul::TileCoord tile)
                                      { add({.x = tile.x / uomul::mapBlockDimension, .y = tile.y / uomul::mapBlockDimension}); });
    }
    else
    {
        selection.forEachSelectedBlock(add);
    }

    const std::uint32_t blockWidth = (dimensions.widthTiles + uomul::mapBlockDimension - 1U) / uomul::mapBlockDimension;
    const std::uint32_t blockHeight = (dimensions.heightTiles + uomul::mapBlockDimension - 1U) / uomul::mapBlockDimension;
    std::erase_if(blocks, [blockWidth, blockHeight](uomul::BlockCoord block)
                 { return block.x >= blockWidth || block.y >= blockHeight; });
    return blocks;
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

MapWorkspace::~MapWorkspace()
{
    reader_.reset();
    removeWorkingFile();
}

std::expected<void, uomul::Error> MapWorkspace::openMap(const std::filesystem::path & path,
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

std::expected<void, uomul::Error> MapWorkspace::createEmptyMap(const std::filesystem::path & path,
                                                               uomul::Dimensions dimensions,
                                                               PendingChangesAction action,
                                                               const std::optional<std::filesystem::path> & savePath,
                                                               bool overwriteOutput,
                                                               uomul::MapTile initialTile)
{
    if (reader_)
    {
        std::error_code currentPathError;
        std::error_code originalPathError;
        std::error_code outputPathError;
        const auto currentPath = std::filesystem::weakly_canonical(mapPath_, currentPathError);
        const auto sourcePath = std::filesystem::weakly_canonical(originalPath_, originalPathError);
        const auto canonicalOutput = std::filesystem::weakly_canonical(path, outputPathError);

        if (!outputPathError &&
            ((!currentPathError && currentPath == canonicalOutput) || (!originalPathError && sourcePath == canonicalOutput)))
        {
            return fail(makeWorkspaceError(path, "new map output must differ from the open source and working maps"));
        }
    }

    if (reader_ && dirty() && action == PendingChangesAction::save && (!savePath || *savePath == path))
    {
        return fail(makeWorkspaceError(path, "cannot replace the requested new-map path while saving staged changes"));
    }

    if (auto resolved = resolvePendingChanges(action, savePath, overwriteOutput); !resolved)
    {
        return resolved;
    }

    std::error_code filesystemError;
    const bool outputAlreadyExists = std::filesystem::exists(path, filesystemError);

    if (filesystemError)
    {
        return fail(makeWorkspaceError(path, filesystemError.message()));
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

std::expected<void, uomul::Error> MapWorkspace::save(const std::filesystem::path & outputPath, bool overwriteOutput)
{
    if (!reader_)
    {
        return fail(makeWorkspaceError(outputPath, "no map is open"));
    }

    if (outputPath.empty())
    {
        return fail(makeWorkspaceError(outputPath, "save output path cannot be empty"));
    }

    if (!dirty())
    {
        return {};
    }

    auto inPlace = uomul::detail::MulCommon::pathsExactlyMatch(mapPath_, outputPath);
    if (!inPlace)
    {
        return fail(inPlace.error());
    }

    auto replacesOriginal = uomul::detail::MulCommon::pathsExactlyMatch(originalPath_, outputPath);
    if (!replacesOriginal)
    {
        return fail(replacesOriginal.error());
    }

    auto editor = uomul::MapEditor::create(
        mapPath_, outputPath, dimensions_, {.overwriteOutput = overwriteOutput || *inPlace || *replacesOriginal});

    if (!editor)
    {
        return fail(editor.error());
    }

    if (*inPlace)
    {
        reader_.reset();
    }

    if (auto committed = editor->commit(); !committed)
    {
        if (*inPlace)
        {
            auto restored = uomul::MapReader::open(mapPath_, dimensions_);
            if (restored)
            {
                reader_ = std::move(*restored);
            }
        }
        return fail(committed.error());
    }

    auto opened = uomul::MapReader::open(outputPath, dimensions_);
    if (!opened)
    {
        return fail(opened.error());
    }

    const MapSelectionMode selectionMode = selectionMode_;
    const auto tileSelection = tileSelection_;
    const auto blockSelection = blockSelection_;
    replaceDocument(std::move(*opened), outputPath, outputPath);
    selectionMode_ = selectionMode;
    tileSelection_ = tileSelection;
    blockSelection_ = blockSelection;

    return {};
}

std::expected<void, uomul::Error> MapWorkspace::discard()
{
    if (!dirty())
    {
        return {};
    }

    auto opened = uomul::MapReader::open(originalPath_, dimensions_);
    if (!opened)
    {
        return fail(opened.error());
    }

    replaceDocument(std::move(*opened), originalPath_, originalPath_);
    return {};
}

std::expected<void, uomul::Error> MapWorkspace::closeMap()
{
    if (dirty())
    {
        return fail(makeWorkspaceError(mapPath_, "pending map changes require save or discard before closing"));
    }

    reader_.reset();
    removeWorkingFile();
    dimensions_ = {};
    mapPath_.clear();
    originalPath_.clear();
    tileSelection_.reset();
    blockSelection_.reset();
    selectionMask_.clear();
    clearHover();
    clearSelectedTile();
    error_.reset();

    return {};
}

std::expected<void, uomul::Error> MapWorkspace::fill(uomul::MapTile value, MapEditScope scope)
{
    return applyEdit(
        [this, value, scope](uomul::MapEditor & editor, const MapWorkspace &) -> std::expected<void, uomul::Error>
        {
            if (scope == MapEditScope::wholeMap)
            {
                return editor.fill(value);
            }

            if (selectionMode_ == MapSelectionMode::tiles && tileSelection_)
            {
                return editor.fillTiles(*tileSelection_, value);
            }

            if (selectionMode_ == MapSelectionMode::blocks && blockSelection_)
            {
                return editor.fillBlocks(*blockSelection_, value);
            }

            return std::unexpected(makeWorkspaceError(mapPath_, "selection scope requires a selection"));
        });
}

std::expected<void, uomul::Error> MapWorkspace::replaceTileId(std::uint16_t from, std::uint16_t to, MapEditScope scope)
{
    return applyEdit(
        [this, from, to, scope](uomul::MapEditor & editor, const MapWorkspace &) -> std::expected<void, uomul::Error>
        {
            if (scope == MapEditScope::wholeMap)
            {
                return editor.replaceIds(from, to);
            }

            if (selectionMode_ == MapSelectionMode::tiles && tileSelection_)
            {
                return editor.replaceIdsTiles(*tileSelection_, from, to);
            }

            if (selectionMode_ == MapSelectionMode::blocks && blockSelection_)
            {
                return editor.replaceIdsBlocks(*blockSelection_, from, to);
            }

            return std::unexpected(makeWorkspaceError(mapPath_, "selection scope requires a selection"));
        });
}

std::expected<void, uomul::Error> MapWorkspace::replaceZ(std::int8_t from, std::int8_t to, MapEditScope scope)
{
    return applyEdit(
        [this, from, to, scope](uomul::MapEditor & editor, const MapWorkspace &) -> std::expected<void, uomul::Error>
        {
            if (scope == MapEditScope::wholeMap)
            {
                return editor.replaceZ(from, to);
            }

            if (selectionMode_ == MapSelectionMode::tiles && tileSelection_)
            {
                return editor.replaceZTiles(*tileSelection_, from, to);
            }

            if (selectionMode_ == MapSelectionMode::blocks && blockSelection_)
            {
                return editor.replaceZBlocks(*blockSelection_, from, to);
            }

            return std::unexpected(makeWorkspaceError(mapPath_, "selection scope requires a selection"));
        });
}

std::expected<MapEditResult, uomul::Error> MapWorkspace::fillMasked(uomul::MapTile value, FillMode mode)
{
    if (selectionMask_.empty())
    {
        const auto failure = fail(makeWorkspaceError(mapPath_, "selection scope requires a selection"));
        return std::unexpected(failure.error());
    }

    return applyCountedEdit(
        [this, value, mode](uomul::MapEditor & editor, const MapWorkspace & workspace) -> std::expected<std::uint64_t, uomul::Error>
        {
            std::uint64_t changedTiles = 0;
            for (const uomul::BlockCoord block : affectedBlocks(workspace.selectionMask_, workspace.dimensions_))
            {
                const uomul::BlockRegion region{
                    .min = block,
                    .maxExclusive = {.x = block.x + 1U, .y = block.y + 1U},
                };
                auto edited = editor.editBlocks(
                    region,
                    [this, value, mode, &changedTiles](uomul::BlockCoord coordinate,
                                                        const uomul::MapBlock & current) -> std::expected<uomul::MapBlock, uomul::Error>
                    {
                        uomul::MapBlock changed = current;
                        const std::uint32_t minX = coordinate.x * uomul::mapBlockDimension;
                        const std::uint32_t minY = coordinate.y * uomul::mapBlockDimension;
                        const std::uint32_t maxX = std::min(dimensions_.widthTiles, minX + uomul::mapBlockDimension);
                        const std::uint32_t maxY = std::min(dimensions_.heightTiles, minY + uomul::mapBlockDimension);
                        for (std::uint32_t y = minY; y < maxY; y++)
                        {
                            for (std::uint32_t x = minX; x < maxX; x++)
                            {
                                const uomul::TileCoord tile{.x = x, .y = y};
                                if ((selectionMask_.granularity() == ToolSelectionGranularity::tiles &&
                                     !selectionMask_.contains(tile)) ||
                                    (selectionMask_.granularity() == ToolSelectionGranularity::blocks &&
                                     !selectionMask_.contains(coordinate)))
                                {
                                    continue;
                                }

                                const std::size_t index = static_cast<std::size_t>(
                                    ((y % uomul::mapBlockDimension) * uomul::mapBlockDimension) +
                                    (x % uomul::mapBlockDimension));
                                uomul::MapTile next = current.at(index);
                                if (mode == FillMode::id || mode == FillMode::idAndZ)
                                {
                                    next.id = value.id;
                                }
                                if (mode == FillMode::z || mode == FillMode::idAndZ)
                                {
                                    next.z = value.z;
                                }
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
        });
}

std::expected<MapEditResult, uomul::Error>
MapWorkspace::replaceMasked(ReplaceMode mode, uomul::MapTile source, uomul::MapTile target)
{
    if (selectionMask_.empty())
    {
        const auto failure = fail(makeWorkspaceError(mapPath_, "selection scope requires a selection"));
        return std::unexpected(failure.error());
    }

    return applyCountedEdit(
        [this, mode, source, target](uomul::MapEditor & editor,
                                     const MapWorkspace & workspace) -> std::expected<std::uint64_t, uomul::Error>
        {
            std::uint64_t changedTiles = 0;
            for (const uomul::BlockCoord block : affectedBlocks(workspace.selectionMask_, workspace.dimensions_))
            {
                const uomul::BlockRegion region{
                    .min = block,
                    .maxExclusive = {.x = block.x + 1U, .y = block.y + 1U},
                };
                auto edited = editor.editBlocks(
                    region,
                    [this, mode, source, target, &changedTiles](uomul::BlockCoord coordinate,
                                                                  const uomul::MapBlock & current)
                        -> std::expected<uomul::MapBlock, uomul::Error>
                    {
                        uomul::MapBlock changed = current;
                        const std::uint32_t minX = coordinate.x * uomul::mapBlockDimension;
                        const std::uint32_t minY = coordinate.y * uomul::mapBlockDimension;
                        const std::uint32_t maxX = std::min(dimensions_.widthTiles, minX + uomul::mapBlockDimension);
                        const std::uint32_t maxY = std::min(dimensions_.heightTiles, minY + uomul::mapBlockDimension);
                        for (std::uint32_t y = minY; y < maxY; y++)
                        {
                            for (std::uint32_t x = minX; x < maxX; x++)
                            {
                                const uomul::TileCoord tile{.x = x, .y = y};
                                if ((selectionMask_.granularity() == ToolSelectionGranularity::tiles &&
                                     !selectionMask_.contains(tile)) ||
                                    (selectionMask_.granularity() == ToolSelectionGranularity::blocks &&
                                     !selectionMask_.contains(coordinate)))
                                {
                                    continue;
                                }

                                const std::size_t index = static_cast<std::size_t>(
                                    ((y % uomul::mapBlockDimension) * uomul::mapBlockDimension) +
                                    (x % uomul::mapBlockDimension));
                                const uomul::MapTile currentTile = current.at(index);
                                const bool matches = mode == ReplaceMode::id
                                                         ? currentTile.id == source.id
                                                         : mode == ReplaceMode::z ? currentTile.z == source.z
                                                                                   : currentTile == source;
                                if (!matches)
                                {
                                    continue;
                                }

                                uomul::MapTile next = currentTile;
                                if (mode == ReplaceMode::id || mode == ReplaceMode::idAndZ)
                                {
                                    next.id = target.id;
                                }
                                if (mode == ReplaceMode::z || mode == ReplaceMode::idAndZ)
                                {
                                    next.z = target.z;
                                }
                                if (next != currentTile)
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
        });
}

std::expected<std::optional<uomul::MapStats>, uomul::Error> MapWorkspace::selectionStats()
{
    if (!reader_)
    {
        return std::optional<uomul::MapStats>{};
    }

    if (selectionMode_ == MapSelectionMode::tiles)
    {
        if (!tileSelection_)
        {
            return std::optional<uomul::MapStats>{};
        }

        auto stats = reader_->statsTiles(*tileSelection_);
        if (!stats)
        {
            error_ = stats.error();
            return std::unexpected(stats.error());
        }

        return std::optional<uomul::MapStats>{std::move(*stats)};
    }

    if (!blockSelection_)
    {
        return std::optional<uomul::MapStats>{};
    }

    auto stats = reader_->statsBlocks(*blockSelection_);
    if (!stats)
    {
        error_ = stats.error();
        return std::unexpected(stats.error());
    }

    return std::optional<uomul::MapStats>{std::move(*stats)};
}

std::expected<void, uomul::Error> MapWorkspace::updateHover(uomul::TileCoord tile)
{
    if (!reader_)
    {
        clearHover();
        return fail(makeWorkspaceError(mapPath_, "no map is open"));
    }

    auto value = reader_->readTile(tile);
    if (!value)
    {
        clearHover();
        return fail(value.error());
    }

    hoveredTile_ = tile;
    hoveredValue_ = *value;
    error_.reset();

    return {};
}

std::expected<void, uomul::Error> MapWorkspace::updateSelectedTile(uomul::TileCoord tile)
{
    if (!reader_)
    {
        clearSelectedTile();
        return fail(makeWorkspaceError(mapPath_, "no map is open"));
    }

    auto value = reader_->readTile(tile);
    if (!value)
    {
        clearSelectedTile();
        return fail(value.error());
    }

    selectedTile_ = tile;
    selectedValue_ = *value;
    error_.reset();

    return {};
}

void MapWorkspace::clearHover() noexcept
{
    hoveredTile_.reset();
    hoveredValue_.reset();
}

void MapWorkspace::clearSelectedTile() noexcept
{
    selectedTile_.reset();
    selectedValue_.reset();
}

void MapWorkspace::setSelectionMode(MapSelectionMode mode)
{
    if (selectionMode_ == mode)
    {
        return;
    }

    if (mode == MapSelectionMode::blocks && selectionMask_.granularity() == ToolSelectionGranularity::tiles)
    {
        selectionMask_.convertTilesToCoveringBlocks(dimensions_);
        tileSelection_.reset();
        blockSelection_.reset();
        selectionMode_ = mode;
        return;
    }

    selectionMode_ = mode;
    tileSelection_.reset();
    blockSelection_.reset();
    selectionMask_.setGranularity(mode == MapSelectionMode::tiles ? ToolSelectionGranularity::tiles
                                                                    : ToolSelectionGranularity::blocks);
}

void MapWorkspace::setTileSelection(uomul::TileRegion region)
{
    tileSelection_ = clampTileRegion(region, dimensions_);
    blockSelection_.reset();
    selectionMode_ = MapSelectionMode::tiles;
    selectionMask_.replaceTileRegion(*tileSelection_, dimensions_);
}

void MapWorkspace::applyTileSelection(uomul::TileRegion region, SelectionCombineMode mode)
{
    const uomul::TileRegion clamped = clampTileRegion(region, dimensions_);
    if (mode == SelectionCombineMode::replace)
    {
        setTileSelection(clamped);
        return;
    }

    MapSelectionMask selection;
    selection.replaceTileRegion(clamped, dimensions_);
    selectionMode_ = MapSelectionMode::tiles;
    tileSelection_.reset();
    blockSelection_.reset();
    if (mode == SelectionCombineMode::add)
    {
        selectionMask_.add(selection);
    }
    else
    {
        selectionMask_.subtract(selection);
    }
}

void MapWorkspace::setBlockSelection(uomul::BlockRegion region)
{
    blockSelection_ = clampBlockRegion(region, dimensions_);
    tileSelection_.reset();
    selectionMode_ = MapSelectionMode::blocks;
    selectionMask_.replaceBlockRegion(*blockSelection_, dimensions_);
}

void MapWorkspace::applyBlockSelection(uomul::BlockRegion region, SelectionCombineMode mode)
{
    const uomul::BlockRegion clamped = clampBlockRegion(region, dimensions_);
    if (mode == SelectionCombineMode::replace)
    {
        setBlockSelection(clamped);
        return;
    }

    MapSelectionMask selection;
    selection.replaceBlockRegion(clamped, dimensions_);
    selectionMode_ = MapSelectionMode::blocks;
    tileSelection_.reset();
    blockSelection_.reset();
    if (mode == SelectionCombineMode::add)
    {
        selectionMask_.add(selection);
    }
    else
    {
        selectionMask_.subtract(selection);
    }
}

void MapWorkspace::applyLassoSelection(const std::vector<uomul::TileCoord> & path, SelectionCombineMode mode)
{
    MapSelectionMask selection;
    selection.setGranularity(selectionMask_.granularity());
    selection.replaceLasso(path, dimensions_);
    if (mode == SelectionCombineMode::replace)
    {
        selectionMask_ = std::move(selection);
    }
    else if (mode == SelectionCombineMode::add)
    {
        selectionMask_.add(selection);
    }
    else
    {
        selectionMask_.subtract(selection);
    }
    selectionMode_ = selectionMask_.granularity() == ToolSelectionGranularity::tiles ? MapSelectionMode::tiles
                                                                                        : MapSelectionMode::blocks;
    tileSelection_.reset();
    blockSelection_.reset();
}

void MapWorkspace::clearSelection()
{
    tileSelection_.reset();
    blockSelection_.reset();
    selectionMask_.clear();
}

void MapWorkspace::selectAll()
{
    tileSelection_.reset();
    blockSelection_.reset();
    selectionMask_.selectAll(dimensions_);
    selectionMode_ = selectionMask_.granularity() == ToolSelectionGranularity::tiles ? MapSelectionMode::tiles
                                                                                       : MapSelectionMode::blocks;
}

void MapWorkspace::invertSelection()
{
    selectionMask_.invert(dimensions_);
    tileSelection_.reset();
    blockSelection_.reset();
}

void MapWorkspace::convertTileSelectionToBlocks()
{
    selectionMask_.convertTilesToCoveringBlocks(dimensions_);
    tileSelection_.reset();
    blockSelection_.reset();
    selectionMode_ = MapSelectionMode::blocks;
}

const MapSelectionMask & MapWorkspace::selectionMask() const noexcept
{
    return selectionMask_;
}

const uomul::MapReader * MapWorkspace::reader() const noexcept
{
    return reader_ ? &*reader_ : nullptr;
}

uomul::Dimensions MapWorkspace::dimensions() const noexcept
{
    return dimensions_;
}

const std::filesystem::path & MapWorkspace::mapPath() const noexcept
{
    return mapPath_;
}

const std::filesystem::path & MapWorkspace::originalPath() const noexcept
{
    return originalPath_;
}

const std::optional<std::filesystem::path> & MapWorkspace::workingPath() const noexcept
{
    return workingPath_;
}

bool MapWorkspace::dirty() const noexcept
{
    return workingPath_.has_value();
}

MapDisplayMode MapWorkspace::displayMode() const noexcept
{
    return displayMode_;
}

void MapWorkspace::setDisplayMode(MapDisplayMode mode) noexcept
{
    displayMode_ = mode;
}

MapSelectionMode MapWorkspace::selectionMode() const noexcept
{
    return selectionMode_;
}

const std::optional<uomul::TileRegion> & MapWorkspace::tileSelection() const noexcept
{
    return tileSelection_;
}

const std::optional<uomul::BlockRegion> & MapWorkspace::blockSelection() const noexcept
{
    return blockSelection_;
}

const std::optional<uomul::TileCoord> & MapWorkspace::hoveredTile() const noexcept
{
    return hoveredTile_;
}

const std::optional<uomul::MapTile> & MapWorkspace::hoveredValue() const noexcept
{
    return hoveredValue_;
}

const std::optional<uomul::TileCoord> & MapWorkspace::selectedTile() const noexcept
{
    return selectedTile_;
}

const std::optional<uomul::MapTile> & MapWorkspace::selectedValue() const noexcept
{
    return selectedValue_;
}

const std::optional<uomul::Error> & MapWorkspace::error() const noexcept
{
    return error_;
}

void MapWorkspace::clearError() noexcept
{
    error_.reset();
}

std::expected<void, uomul::Error> MapWorkspace::resolvePendingChanges(PendingChangesAction action,
                                                                      const std::optional<std::filesystem::path> & savePath,
                                                                      bool overwriteSave)
{
    if (!dirty())
    {
        return {};
    }

    if (action == PendingChangesAction::cancel)
    {
        return fail(makeWorkspaceError(mapPath_, "pending map changes require save or discard"));
    }

    if (action == PendingChangesAction::discard)
    {
        return discard();
    }

    if (!savePath)
    {
        return fail(makeWorkspaceError(mapPath_, "a save path is required to resolve pending map changes"));
    }

    return save(*savePath, overwriteSave);
}

std::expected<void, uomul::Error> MapWorkspace::applyEdit(const EditOperation & operation)
{
    if (!reader_)
    {
        return fail(makeWorkspaceError(mapPath_, "no map is open"));
    }

    const auto sourcePath = mapPath_;
    auto outputPath = createWorkingPath(sourcePath);

    if (!outputPath)
    {
        return fail(outputPath.error());
    }

    auto editor = uomul::MapEditor::create(sourcePath, *outputPath, dimensions_);
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

    auto opened = uomul::MapReader::open(*outputPath, dimensions_);
    if (!opened)
    {
        std::error_code ignoredError;
        std::filesystem::remove(*outputPath, ignoredError);
        return fail(opened.error());
    }

    const auto selectionMode = selectionMode_;
    const auto tileSelection = tileSelection_;
    const auto blockSelection = blockSelection_;
    const auto selectionMask = selectionMask_;
    replaceDocument(std::move(*opened), *outputPath, originalPath_, *outputPath);
    selectionMode_ = selectionMode;
    tileSelection_ = tileSelection;
    blockSelection_ = blockSelection;
    selectionMask_ = selectionMask;
    error_.reset();

    return {};
}

std::expected<MapEditResult, uomul::Error> MapWorkspace::applyCountedEdit(const CountedEditOperation & operation)
{
    if (!reader_)
    {
        const auto failure = fail(makeWorkspaceError(mapPath_, "no map is open"));
        return std::unexpected(failure.error());
    }

    const auto sourcePath = mapPath_;
    auto outputPath = createWorkingPath(sourcePath);
    if (!outputPath)
    {
        const auto failure = fail(outputPath.error());
        return std::unexpected(failure.error());
    }

    auto editor = uomul::MapEditor::create(sourcePath, *outputPath, dimensions_);
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

    auto opened = uomul::MapReader::open(*outputPath, dimensions_);
    if (!opened)
    {
        std::error_code ignoredError;
        std::filesystem::remove(*outputPath, ignoredError);
        const auto failure = fail(opened.error());
        return std::unexpected(failure.error());
    }

    const auto selectionMode = selectionMode_;
    const auto tileSelection = tileSelection_;
    const auto blockSelection = blockSelection_;
    const auto selectionMask = selectionMask_;
    replaceDocument(std::move(*opened), *outputPath, originalPath_, *outputPath);
    selectionMode_ = selectionMode;
    tileSelection_ = tileSelection;
    blockSelection_ = blockSelection;
    selectionMask_ = selectionMask;
    error_.reset();

    return MapEditResult{.changedTiles = *changed};
}

std::expected<void, uomul::Error> MapWorkspace::fail(uomul::Error error)
{
    error_ = error;
    return std::unexpected(std::move(error));
}

void MapWorkspace::replaceDocument(uomul::MapReader reader,
                                   std::filesystem::path path,
                                   std::filesystem::path originalPath,
                                   std::optional<std::filesystem::path> workingPath)
{
    const auto previousWorkingPath = workingPath_;
    reader_ = std::move(reader);
    dimensions_ = reader_->dimensions();
    mapPath_ = std::move(path);
    originalPath_ = std::move(originalPath);
    workingPath_ = std::move(workingPath);
    tileSelection_.reset();
    blockSelection_.reset();
    clearHover();
    clearSelectedTile();
    error_.reset();

    if (previousWorkingPath && (!workingPath_ || *previousWorkingPath != *workingPath_))
    {
        std::error_code ignoredError;
        std::filesystem::remove(*previousWorkingPath, ignoredError);
    }
}

void MapWorkspace::removeWorkingFile() noexcept
{
    if (workingPath_)
    {
        std::error_code ignoredError;
        std::filesystem::remove(*workingPath_, ignoredError);
        workingPath_.reset();
    }
}

} // namespace uo_mule::gui
