#pragma once

#include "gui_model/world/map_color.h"
#include "gui_model/world/map_selection.h"
#include "uomul/map/map.h"
#include "uomul/mul_common/mul_common.h"
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>

namespace uo_mule::gui
{

enum class MapDisplayMode : std::uint8_t
{
    tileIds,
    z,
    combined,
};

enum class MapSelectionMode : std::uint8_t
{
    tiles,
    blocks,
};

enum class PendingChangesAction : std::uint8_t
{
    cancel,
    discard,
    save,
};

enum class MapEditScope : std::uint8_t
{
    selection,
    wholeMap,
};

struct MapEditResult
{
    std::uint64_t changedTiles = 0;
};

using FuzzyTiledataNameLookup = std::function<std::optional<std::string>(std::uint16_t)>;

[[nodiscard]] uomul::BlockCoord tileToBlock(uomul::TileCoord tile) noexcept;
[[nodiscard]] uomul::TileCoord selectionInspectorTile(MapSelectionMode mode, uomul::TileCoord tile) noexcept;
[[nodiscard]] std::optional<uomul::TileCoord> screenToTile(double screenX,
                                                           double screenY,
                                                           double canvasX,
                                                           double canvasY,
                                                           double panX,
                                                           double panY,
                                                           double tileSize,
                                                           uomul::Dimensions dimensions) noexcept;
[[nodiscard]] uomul::TileRegion clampTileRegion(uomul::TileRegion region, uomul::Dimensions dimensions) noexcept;
[[nodiscard]] uomul::BlockRegion clampBlockRegion(uomul::BlockRegion region, uomul::Dimensions dimensions) noexcept;
[[nodiscard]] uomul::TileRegion
tileSelectionFromDrag(uomul::TileCoord first, uomul::TileCoord last, uomul::Dimensions dimensions) noexcept;
[[nodiscard]] uomul::BlockRegion
blockSelectionFromDrag(uomul::TileCoord first, uomul::TileCoord last, uomul::Dimensions dimensions) noexcept;
[[nodiscard]] MapColor tileIdColor(std::uint16_t id) noexcept;
[[nodiscard]] MapColor
tileDisplayColor(MapDisplayMode mode, uomul::MapTile tile, std::optional<MapColor> radarColor = std::nullopt) noexcept;
[[nodiscard]] std::uint8_t zGrayscale(std::int8_t z) noexcept;

class MapWorkspace
{
public:
    MapWorkspace() = default;
    MapWorkspace(const MapWorkspace &) = delete;
    MapWorkspace & operator=(const MapWorkspace &) = delete;
    MapWorkspace(MapWorkspace &&) = delete;
    MapWorkspace & operator=(MapWorkspace &&) = delete;
    ~MapWorkspace();

    [[nodiscard]] std::expected<void, uomul::Error>
    openMap(const std::filesystem::path & path,
            uomul::Dimensions dimensions,
            PendingChangesAction action = PendingChangesAction::cancel,
            const std::optional<std::filesystem::path> & savePath = std::nullopt,
            bool overwriteSave = false);
    [[nodiscard]] std::expected<void, uomul::Error>
    createEmptyMap(const std::filesystem::path & path,
                   uomul::Dimensions dimensions,
                   PendingChangesAction action = PendingChangesAction::cancel,
                   const std::optional<std::filesystem::path> & savePath = std::nullopt,
                   bool overwriteOutput = false,
                   uomul::MapTile initialTile = {});
    [[nodiscard]] std::expected<void, uomul::Error> save(const std::filesystem::path & outputPath,
                                                         bool overwriteOutput = false);
    [[nodiscard]] std::expected<void, uomul::Error> discard();
    [[nodiscard]] std::expected<void, uomul::Error> closeMap();

    [[nodiscard]] std::expected<void, uomul::Error> fill(uomul::MapTile value, MapEditScope scope);
    [[nodiscard]] std::expected<void, uomul::Error> replaceTileId(std::uint16_t from, std::uint16_t to, MapEditScope scope);
    [[nodiscard]] std::expected<void, uomul::Error> replaceZ(std::int8_t from, std::int8_t to, MapEditScope scope);
    [[nodiscard]] std::expected<MapEditResult, uomul::Error> fillMasked(uomul::MapTile value, FillMode mode);
    [[nodiscard]] std::expected<MapEditResult, uomul::Error>
    replaceMasked(ReplaceMode mode, uomul::MapTile source, uomul::MapTile target);
    [[nodiscard]] std::expected<std::size_t, uomul::Error>
    fuzzySelect(FuzzySelectionOptions options,
                uomul::TileCoord origin,
                const FuzzyTiledataNameLookup & tiledataNameLookup = {});

    [[nodiscard]] std::expected<std::optional<uomul::MapStats>, uomul::Error> selectionStats();
    [[nodiscard]] std::expected<void, uomul::Error> updateHover(uomul::TileCoord tile);
    [[nodiscard]] std::expected<void, uomul::Error> updateSelectedTile(uomul::TileCoord tile);
    void clearHover() noexcept;
    void clearSelectedTile() noexcept;
    void setSelectionMode(MapSelectionMode mode);
    void setTileSelection(uomul::TileRegion region);
    void setBlockSelection(uomul::BlockRegion region);
    void applyTileSelection(uomul::TileRegion region, SelectionCombineMode mode);
    void applyBlockSelection(uomul::BlockRegion region, SelectionCombineMode mode);
    void applyLassoSelection(const std::vector<uomul::TileCoord> & path, SelectionCombineMode mode);
    void clearSelection();
    void selectAll();
    void invertSelection();
    void convertTileSelectionToBlocks();
    [[nodiscard]] const MapSelectionMask & selectionMask() const noexcept;
    [[nodiscard]] std::optional<uomul::TileRegion> selectionBounds() const;

    [[nodiscard]] const uomul::MapReader * reader() const noexcept;
    [[nodiscard]] uomul::Dimensions dimensions() const noexcept;
    [[nodiscard]] const std::filesystem::path & mapPath() const noexcept;
    [[nodiscard]] const std::filesystem::path & originalPath() const noexcept;
    [[nodiscard]] const std::optional<std::filesystem::path> & workingPath() const noexcept;
    [[nodiscard]] bool dirty() const noexcept;
    [[nodiscard]] MapDisplayMode displayMode() const noexcept;
    void setDisplayMode(MapDisplayMode mode) noexcept;
    [[nodiscard]] MapSelectionMode selectionMode() const noexcept;
    [[nodiscard]] const std::optional<uomul::TileRegion> & tileSelection() const noexcept;
    [[nodiscard]] const std::optional<uomul::BlockRegion> & blockSelection() const noexcept;
    [[nodiscard]] const std::optional<uomul::TileCoord> & hoveredTile() const noexcept;
    [[nodiscard]] const std::optional<uomul::MapTile> & hoveredValue() const noexcept;
    [[nodiscard]] const std::optional<uomul::TileCoord> & selectedTile() const noexcept;
    [[nodiscard]] const std::optional<uomul::MapTile> & selectedValue() const noexcept;
    [[nodiscard]] const std::optional<uomul::Error> & error() const noexcept;
    void clearError() noexcept;

private:
    using EditOperation = std::function<std::expected<void, uomul::Error>(uomul::MapEditor &, const MapWorkspace &)>;
    using CountedEditOperation =
        std::function<std::expected<std::uint64_t, uomul::Error>(uomul::MapEditor &, const MapWorkspace &)>;

    [[nodiscard]] std::expected<void, uomul::Error> resolvePendingChanges(PendingChangesAction action,
                                                                          const std::optional<std::filesystem::path> & savePath,
                                                                          bool overwriteSave);
    [[nodiscard]] std::expected<void, uomul::Error> applyEdit(const EditOperation & operation);
    [[nodiscard]] std::expected<MapEditResult, uomul::Error> applyCountedEdit(const CountedEditOperation & operation);
    [[nodiscard]] std::expected<void, uomul::Error> fail(uomul::Error error);
    void replaceDocument(uomul::MapReader reader,
                         std::filesystem::path path,
                         std::filesystem::path originalPath,
                         std::optional<std::filesystem::path> workingPath = std::nullopt);
    void removeWorkingFile() noexcept;

    std::optional<uomul::MapReader> reader_;
    uomul::Dimensions dimensions_{};
    std::filesystem::path mapPath_;
    std::filesystem::path originalPath_;
    std::optional<std::filesystem::path> workingPath_;
    MapDisplayMode displayMode_ = MapDisplayMode::tileIds;
    MapSelectionMode selectionMode_ = MapSelectionMode::tiles;
    std::optional<uomul::TileRegion> tileSelection_;
    std::optional<uomul::BlockRegion> blockSelection_;
    MapSelectionMask selectionMask_;
    std::optional<uomul::TileCoord> hoveredTile_;
    std::optional<uomul::MapTile> hoveredValue_;
    std::optional<uomul::TileCoord> selectedTile_;
    std::optional<uomul::MapTile> selectedValue_;
    std::optional<uomul::Error> error_;
};

} // namespace uo_mule::gui
