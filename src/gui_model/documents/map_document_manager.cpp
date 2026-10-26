#include "gui_model/documents/map_document_manager.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/documents/map_document_state.h"
#include "gui_model/world/map_canvas_state.h"
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <uomul/map/map.h>
#include <uomul/mul_common/mul_common.h>
#include <utility>

namespace uo_mule::gui
{

namespace
{

[[nodiscard]] std::expected<void, uomul::Error> resolveActiveChanges(MapDocumentState & mapDocument,
                                                                     PendingChangesAction action,
                                                                     const std::optional<std::filesystem::path> & savePath,
                                                                     bool overwrite,
                                                                     std::string_view replacement)
{
    if (!mapDocument.changes().reader || !mapDocument.changes().dirty())
    {
        return {};
    }

    if (action == PendingChangesAction::cancel)
    {
        return std::unexpected(uomul::Error{
            .code = uomul::ErrorCode::io,
            .path = mapDocument.changes().mapPath,
            .message = "pending map changes require save or discard before " + std::string(replacement),
        });
    }

    if (action == PendingChangesAction::save)
    {
        if (!savePath)
        {
            return std::unexpected(uomul::Error{
                .code = uomul::ErrorCode::io,
                .path = mapDocument.changes().mapPath,
                .message = "save path is required before replacing a dirty map",
            });
        }
        return mapDocument.save(*savePath, overwrite);
    }

    return mapDocument.discard();
}

} // namespace

MapDocumentManager::MapDocumentManager() :
    active_(std::make_unique<MapDocumentState>()), activeCanvas_(std::make_unique<MapCanvasViewState>())
{
}

MapDocumentState & MapDocumentManager::active() noexcept
{
    return *active_;
}

const MapDocumentState & MapDocumentManager::active() const noexcept
{
    return *active_;
}

MapCanvasViewState & MapDocumentManager::canvasState() noexcept
{
    return *activeCanvas_;
}

const MapCanvasViewState & MapDocumentManager::canvasState() const noexcept
{
    return *activeCanvas_;
}

std::optional<DocumentId> MapDocumentManager::activeDocumentId() const noexcept
{
    return activeId_;
}

void MapDocumentManager::stashActive() noexcept
{
    if (activeId_ && active_->changes().reader)
    {
        documentStates_[*activeId_] = std::move(active_);
        canvasStates_[*activeId_] = std::move(activeCanvas_);
    }
    active_ = std::make_unique<MapDocumentState>();
    activeCanvas_ = std::make_unique<MapCanvasViewState>();
    activeId_.reset();
}

std::expected<void, uomul::Error>
MapDocumentManager::replaceActive(DocumentStatePtr replacement, CanvasStatePtr canvas, std::optional<DocumentId> newId)
{
    stashActive();
    active_ = std::move(replacement);
    activeCanvas_ = std::move(canvas);
    activeId_ = newId;
    return {};
}

std::expected<void, uomul::Error> MapDocumentManager::openMap(const std::filesystem::path & path,
                                                              uomul::Dimensions dimensions,
                                                              PendingChangesAction action,
                                                              const std::optional<std::filesystem::path> & savePath,
                                                              bool overwriteSave)
{
    if (auto resolved = resolveActiveChanges(*active_, action, savePath, overwriteSave, "opening another map"); !resolved)
    {
        return resolved;
    }
    auto replacement = std::make_unique<MapDocumentState>();
    if (auto opened = replacement->openMap(path, dimensions); !opened)
    {
        return opened;
    }
    return replaceActive(std::move(replacement), std::make_unique<MapCanvasViewState>(), std::nullopt);
}

std::expected<void, uomul::Error> MapDocumentManager::createEmptyMap(const std::filesystem::path & path,
                                                                     uomul::Dimensions dimensions,
                                                                     PendingChangesAction action,
                                                                     const std::optional<std::filesystem::path> & savePath,
                                                                     bool overwriteOutput,
                                                                     uomul::MapTile initialTile)
{
    if (auto resolved = resolveActiveChanges(*active_, action, savePath, overwriteOutput, "creating another map"); !resolved)
    {
        return resolved;
    }
    auto replacement = std::make_unique<MapDocumentState>();
    if (auto created = replacement->createEmptyMap(
            path, dimensions, PendingChangesAction::cancel, std::nullopt, overwriteOutput, initialTile);
        !created)
    {
        return created;
    }
    return replaceActive(std::move(replacement), std::make_unique<MapCanvasViewState>(), std::nullopt);
}

std::expected<void, uomul::Error> MapDocumentManager::save(const std::filesystem::path & path, bool overwrite)
{
    return active_->save(path, overwrite);
}

std::expected<void, uomul::Error> MapDocumentManager::discard()
{
    return active_->discard();
}

std::expected<void, uomul::Error> MapDocumentManager::closeMap()
{
    auto closed = active_->closeMap();
    if (closed)
    {
        activeId_.reset();
    }
    return closed;
}

void MapDocumentManager::deactivateMap() noexcept
{
    stashActive();
}

std::expected<void, uomul::Error>
MapDocumentManager::activateMap(DocumentId id, const std::filesystem::path & path, uomul::Dimensions dimensions)
{
    if (activeId_ == id)
    {
        return {};
    }

    if (activeId_ && active_->changes().reader)
    {
        documentStates_[*activeId_] = std::move(active_);
        canvasStates_[*activeId_] = std::move(activeCanvas_);
    }

    const auto documentState = documentStates_.find(id);
    const auto canvas = canvasStates_.find(id);
    if (documentState != documentStates_.end())
    {
        active_ = std::move(documentState->second);
        documentStates_.erase(documentState);
        activeCanvas_ = std::move(canvas->second);
        canvasStates_.erase(canvas);
        activeId_ = id;
        return {};
    }

    active_ = std::make_unique<MapDocumentState>();
    activeCanvas_ = std::make_unique<MapCanvasViewState>();
    activeId_ = id;
    if (auto opened = active_->openMap(path, dimensions); !opened)
    {
        activeId_.reset();
        return opened;
    }
    return {};
}

std::expected<void, uomul::Error> MapDocumentManager::closeDocument(DocumentId id)
{
    if (activeId_ && *activeId_ == id)
    {
        if (auto closed = active_->closeMap(); !closed)
        {
            return closed;
        }
        active_ = std::make_unique<MapDocumentState>();
        activeCanvas_ = std::make_unique<MapCanvasViewState>();
        activeId_.reset();
        return {};
    }
    documentStates_.erase(id);
    canvasStates_.erase(id);
    return {};
}

void MapDocumentManager::bindActiveDocument(DocumentId id) noexcept
{
    if (active_->changes().reader)
    {
        activeId_ = id;
    }
}

void MapDocumentManager::invalidateProviderCaches() noexcept
{
    activeCanvas_->lodCacheValid = false;
    activeCanvas_->blockCache.clear();
    activeCanvas_->blockCacheIndex.clear();
    activeCanvas_->cacheClock = 0;
    activeCanvas_->lodReadError.reset();
    for (auto & [id, canvas] : canvasStates_)
    {
        static_cast<void>(id);
        canvas->lodCacheValid = false;
        canvas->blockCache.clear();
        canvas->blockCacheIndex.clear();
        canvas->cacheClock = 0;
        canvas->lodReadError.reset();
    }
}

} // namespace uo_mule::gui
