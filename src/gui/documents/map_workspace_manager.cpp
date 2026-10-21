#include "gui/documents/map_workspace_manager.h"
#include "gui/documents/document_identity.h"
#include "gui/documents/map_workspace.h"
#include "gui/views/map_canvas_panel.h"
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <uomul/map.h>
#include <uomul/mul_common/mul_common.h>
#include <utility>

namespace uo_mule::gui
{

MapWorkspaceManager::MapWorkspaceManager()
    : active_(std::make_unique<MapWorkspace>()), activeCanvas_(std::make_unique<MapCanvasPanelState>())
{
}

MapWorkspace & MapWorkspaceManager::active() noexcept
{
    return *active_;
}

const MapWorkspace & MapWorkspaceManager::active() const noexcept
{
    return *active_;
}

MapCanvasPanelState & MapWorkspaceManager::canvasState() noexcept
{
    return *activeCanvas_;
}

const MapCanvasPanelState & MapWorkspaceManager::canvasState() const noexcept
{
    return *activeCanvas_;
}

std::optional<DocumentId> MapWorkspaceManager::activeDocumentId() const noexcept
{
    return activeId_;
}

void MapWorkspaceManager::stashActive() noexcept
{
    if (activeId_ && active_->reader() != nullptr)
    {
        workspaces_[*activeId_] = std::move(active_);
        canvasStates_[*activeId_] = std::move(activeCanvas_);
    }
    active_ = std::make_unique<MapWorkspace>();
    activeCanvas_ = std::make_unique<MapCanvasPanelState>();
    activeId_.reset();
}

std::expected<void, uomul::Error> MapWorkspaceManager::replaceActive(WorkspacePtr replacement,
                                                                       CanvasStatePtr canvas,
                                                                       std::optional<DocumentId> newId)
{
    stashActive();
    active_ = std::move(replacement);
    activeCanvas_ = std::move(canvas);
    activeId_ = newId;
    return {};
}

std::expected<void, uomul::Error> MapWorkspaceManager::openMap(const std::filesystem::path & path,
                                                                uomul::Dimensions dimensions,
                                                                PendingChangesAction action,
                                                                const std::optional<std::filesystem::path> & savePath,
                                                                bool overwriteSave)
{
    if (active_->reader() != nullptr && active_->dirty())
    {
        if (action == PendingChangesAction::cancel)
        {
            return std::unexpected(uomul::Error{.code = uomul::ErrorCode::io,
                                                .path = active_->mapPath(),
                                                .message = "pending map changes require save or discard before opening another map",});
        }
        if (action == PendingChangesAction::save)
        {
            if (!savePath)
            {
                return std::unexpected(uomul::Error{.code = uomul::ErrorCode::io,
                                                    .path = active_->mapPath(),
                                                    .message = "save path is required before replacing a dirty map",});
            }
            if (auto saved = active_->save(*savePath, overwriteSave); !saved)
            {
                return saved;
            }
        }
        else if (auto discarded = active_->discard(); !discarded)
        {
            return discarded;
        }
    }
    auto replacement = std::make_unique<MapWorkspace>();
    if (auto opened = replacement->openMap(path, dimensions); !opened)
    {
        return opened;
    }
    return replaceActive(std::move(replacement), std::make_unique<MapCanvasPanelState>(), std::nullopt);
}

std::expected<void, uomul::Error> MapWorkspaceManager::createEmptyMap(
    const std::filesystem::path & path,
    uomul::Dimensions dimensions,
    PendingChangesAction action,
    const std::optional<std::filesystem::path> & savePath,
    bool overwriteOutput,
    uomul::MapTile initialTile)
{
    if (active_->reader() != nullptr && active_->dirty())
    {
        if (action == PendingChangesAction::cancel)
        {
            return std::unexpected(uomul::Error{.code = uomul::ErrorCode::io,
                                                .path = active_->mapPath(),
                                                .message = "pending map changes require save or discard before creating another map",});
        }
        if (action == PendingChangesAction::save)
        {
            if (!savePath)
            {
                return std::unexpected(uomul::Error{.code = uomul::ErrorCode::io,
                                                    .path = active_->mapPath(),
                                                    .message = "save path is required before replacing a dirty map",});
            }
            if (auto saved = active_->save(*savePath, overwriteOutput); !saved)
            {
                return saved;
            }
        }
        else if (auto discarded = active_->discard(); !discarded)
        {
            return discarded;
        }
    }
    auto replacement = std::make_unique<MapWorkspace>();
    if (auto created = replacement->createEmptyMap(path, dimensions, PendingChangesAction::cancel, std::nullopt, overwriteOutput, initialTile); !created)
    {
        return created;
    }
    return replaceActive(std::move(replacement), std::make_unique<MapCanvasPanelState>(), std::nullopt);
}

std::expected<void, uomul::Error> MapWorkspaceManager::save(const std::filesystem::path & path, bool overwrite)
{
    return active_->save(path, overwrite);
}

std::expected<void, uomul::Error> MapWorkspaceManager::discard()
{
    return active_->discard();
}

std::expected<void, uomul::Error> MapWorkspaceManager::closeMap()
{
    auto closed = active_->closeMap();
    if (closed)
    {
        activeId_.reset();
    }
    return closed;
}

void MapWorkspaceManager::deactivateMap() noexcept
{
    stashActive();
}

std::expected<void, uomul::Error> MapWorkspaceManager::activateMap(DocumentId id,
                                                                     const std::filesystem::path & path,
                                                                     uomul::Dimensions dimensions)
{
    if (activeId_ == id)
    {
        return {};
    }

    if (activeId_ && active_->reader() != nullptr)
    {
        workspaces_[*activeId_] = std::move(active_);
        canvasStates_[*activeId_] = std::move(activeCanvas_);
    }

    const auto workspace = workspaces_.find(id);
    const auto canvas = canvasStates_.find(id);
    if (workspace != workspaces_.end())
    {
        active_ = std::move(workspace->second);
        workspaces_.erase(workspace);
        activeCanvas_ = std::move(canvas->second);
        canvasStates_.erase(canvas);
        activeId_ = id;
        return {};
    }

    active_ = std::make_unique<MapWorkspace>();
    activeCanvas_ = std::make_unique<MapCanvasPanelState>();
    activeId_ = id;
    if (auto opened = active_->openMap(path, dimensions); !opened)
    {
        activeId_.reset();
        return opened;
    }
    return {};
}

std::expected<void, uomul::Error> MapWorkspaceManager::closeDocument(DocumentId id)
{
    if (activeId_ && *activeId_ == id)
    {
        if (auto closed = active_->closeMap(); !closed)
        {
            return closed;
        }
        active_ = std::make_unique<MapWorkspace>();
        activeCanvas_ = std::make_unique<MapCanvasPanelState>();
        activeId_.reset();
        return {};
    }
    workspaces_.erase(id);
    canvasStates_.erase(id);
    return {};
}

void MapWorkspaceManager::bindActiveDocument(DocumentId id) noexcept
{
    if (active_->reader() != nullptr)
    {
        activeId_ = id;
    }
}

void MapWorkspaceManager::invalidateProviderCaches() noexcept
{
    activeCanvas_->lodCacheValid = false;
    for (auto & [id, canvas] : canvasStates_)
    {
        static_cast<void>(id);
        canvas->lodCacheValid = false;
    }
}

} // namespace uo_mule::gui
