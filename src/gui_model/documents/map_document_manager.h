#pragma once

#include "gui_model/documents/document_identity.h"
#include "gui_model/documents/map_document_state.h"
#include "gui_model/world/map_canvas_state.h"
#include <expected>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>

namespace uo_mule::gui
{

class MapDocumentManager
{
public:
    MapDocumentManager();
    MapDocumentManager(const MapDocumentManager &) = delete;
    MapDocumentManager & operator=(const MapDocumentManager &) = delete;
    MapDocumentManager(MapDocumentManager &&) = delete;
    MapDocumentManager & operator=(MapDocumentManager &&) = delete;
    ~MapDocumentManager() = default;

    [[nodiscard]] MapDocumentState & active() noexcept;
    [[nodiscard]] const MapDocumentState & active() const noexcept;
    [[nodiscard]] MapCanvasViewState & canvasState() noexcept;
    [[nodiscard]] const MapCanvasViewState & canvasState() const noexcept;
    [[nodiscard]] std::optional<DocumentId> activeDocumentId() const noexcept;

    [[nodiscard]] std::expected<std::optional<uomul::MapStats>, uomul::Error> selectionStats()
    {
        return active_->selectionStats();
    }
    [[nodiscard]] std::expected<void, uomul::Error> updateHover(uomul::TileCoord tile) { return active_->updateHover(tile); }
    [[nodiscard]] std::expected<void, uomul::Error> updateSelectedTile(uomul::TileCoord tile)
    {
        return active_->updateSelectedTile(tile);
    }
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
    [[nodiscard]] std::expected<MapEditResult, uomul::Error> fillMasked(uomul::MapTile value, FillMode mode)
    {
        return active_->fillMasked(value, mode);
    }
    [[nodiscard]] std::expected<MapEditResult, uomul::Error>
    replaceMasked(ReplaceMode mode, uomul::MapTile source, uomul::MapTile target)
    {
        return active_->replaceMasked(mode, source, target);
    }
    [[nodiscard]] std::expected<std::size_t, uomul::Error> fuzzySelect(const FuzzySelectionOptions & options,
                                                                       uomul::TileCoord origin,
                                                                       const FuzzyTiledataNameLookup & tiledataNameLookup = {})
    {
        return active_->fuzzySelect(options, origin, tiledataNameLookup);
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
    using DocumentStatePtr = std::unique_ptr<MapDocumentState>;
    using CanvasStatePtr = std::unique_ptr<MapCanvasViewState>;

    [[nodiscard]] std::expected<void, uomul::Error>
    replaceActive(DocumentStatePtr replacement, CanvasStatePtr canvas, std::optional<DocumentId> newId);
    void stashActive() noexcept;

    DocumentStatePtr active_;
    CanvasStatePtr activeCanvas_;
    std::optional<DocumentId> activeId_;
    std::unordered_map<DocumentId, DocumentStatePtr> documentStates_;
    std::unordered_map<DocumentId, CanvasStatePtr> canvasStates_;
};

} // namespace uo_mule::gui
