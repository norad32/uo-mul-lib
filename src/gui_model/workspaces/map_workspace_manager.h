#pragma once

#include "gui_model/documents/document_identity.h"
#include "gui_model/workspaces/map_workspace.h"
#include "gui_model/world/map_canvas_state.h"
#include <expected>
#include <memory>
#include <optional>
#include <unordered_map>

namespace uo_mule::gui
{

class MapWorkspaceManager
{
public:
    MapWorkspaceManager();
    MapWorkspaceManager(const MapWorkspaceManager &) = delete;
    MapWorkspaceManager & operator=(const MapWorkspaceManager &) = delete;
    MapWorkspaceManager(MapWorkspaceManager &&) = delete;
    MapWorkspaceManager & operator=(MapWorkspaceManager &&) = delete;
    ~MapWorkspaceManager() = default;

    [[nodiscard]] MapWorkspace & active() noexcept;
    [[nodiscard]] const MapWorkspace & active() const noexcept;
    [[nodiscard]] MapCanvasViewState & canvasState() noexcept;
    [[nodiscard]] const MapCanvasViewState & canvasState() const noexcept;
    [[nodiscard]] std::optional<DocumentId> activeDocumentId() const noexcept;

    [[nodiscard]] const uomul::MapReader * reader() const noexcept { return active_->reader(); }
    [[nodiscard]] uomul::Dimensions dimensions() const noexcept { return active_->dimensions(); }
    [[nodiscard]] const std::filesystem::path & mapPath() const noexcept { return active_->mapPath(); }
    [[nodiscard]] const std::filesystem::path & originalPath() const noexcept { return active_->originalPath(); }
    [[nodiscard]] const std::optional<std::filesystem::path> & workingPath() const noexcept { return active_->workingPath(); }
    [[nodiscard]] bool dirty() const noexcept { return active_->dirty(); }
    [[nodiscard]] MapDisplayMode displayMode() const noexcept { return active_->displayMode(); }
    void setDisplayMode(MapDisplayMode mode) noexcept { active_->setDisplayMode(mode); }
    [[nodiscard]] MapSelectionMode selectionMode() const noexcept { return active_->selectionMode(); }
    void setSelectionMode(MapSelectionMode mode) noexcept { active_->setSelectionMode(mode); }
    void setTileSelection(uomul::TileRegion region) noexcept { active_->setTileSelection(region); }
    void setBlockSelection(uomul::BlockRegion region) noexcept { active_->setBlockSelection(region); }
    [[nodiscard]] const std::optional<uomul::TileRegion> & tileSelection() const noexcept { return active_->tileSelection(); }
    [[nodiscard]] const std::optional<uomul::BlockRegion> & blockSelection() const noexcept
    {
        return active_->blockSelection();
    }
    [[nodiscard]] const std::optional<uomul::TileCoord> & hoveredTile() const noexcept { return active_->hoveredTile(); }
    [[nodiscard]] const std::optional<uomul::MapTile> & hoveredValue() const noexcept { return active_->hoveredValue(); }
    [[nodiscard]] const std::optional<uomul::TileCoord> & selectedTile() const noexcept { return active_->selectedTile(); }
    [[nodiscard]] const std::optional<uomul::MapTile> & selectedValue() const noexcept { return active_->selectedValue(); }
    [[nodiscard]] const std::optional<uomul::Error> & error() const noexcept { return active_->error(); }
    [[nodiscard]] std::expected<std::optional<uomul::MapStats>, uomul::Error> selectionStats()
    {
        return active_->selectionStats();
    }
    [[nodiscard]] std::expected<void, uomul::Error> updateHover(uomul::TileCoord tile) { return active_->updateHover(tile); }
    [[nodiscard]] std::expected<void, uomul::Error> updateSelectedTile(uomul::TileCoord tile)
    {
        return active_->updateSelectedTile(tile);
    }
    void clearHover() noexcept { active_->clearHover(); }
    void clearSelectedTile() noexcept { active_->clearSelectedTile(); }
    [[nodiscard]] std::expected<void, uomul::Error> fill(uomul::MapTile value, MapEditScope scope)
    {
        return active_->fill(value, scope);
    }
    [[nodiscard]] std::expected<void, uomul::Error> replaceTileId(std::uint16_t from, std::uint16_t to, MapEditScope scope)
    {
        return active_->replaceTileId(from, to, scope);
    }
    [[nodiscard]] std::expected<void, uomul::Error> replaceZ(std::int8_t from, std::int8_t to, MapEditScope scope)
    {
        return active_->replaceZ(from, to, scope);
    }

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
    [[nodiscard]] std::expected<void, uomul::Error> save(const std::filesystem::path & path, bool overwrite = false);
    [[nodiscard]] std::expected<void, uomul::Error> discard();
    [[nodiscard]] std::expected<void, uomul::Error> closeMap();
    void deactivateMap() noexcept;

    [[nodiscard]] std::expected<void, uomul::Error>
    activateMap(DocumentId id, const std::filesystem::path & path, uomul::Dimensions dimensions);
    [[nodiscard]] std::expected<void, uomul::Error> closeDocument(DocumentId id);
    void bindActiveDocument(DocumentId id) noexcept;
    void invalidateProviderCaches() noexcept;

private:
    using WorkspacePtr = std::unique_ptr<MapWorkspace>;
    using CanvasStatePtr = std::unique_ptr<MapCanvasViewState>;

    [[nodiscard]] std::expected<void, uomul::Error>
    replaceActive(WorkspacePtr replacement, CanvasStatePtr canvas, std::optional<DocumentId> newId);
    void stashActive() noexcept;

    WorkspacePtr active_;
    CanvasStatePtr activeCanvas_;
    std::optional<DocumentId> activeId_;
    std::unordered_map<DocumentId, WorkspacePtr> workspaces_;
    std::unordered_map<DocumentId, CanvasStatePtr> canvasStates_;
};

} // namespace uo_mule::gui
