#include "gui/map_document.h"
#include "gui/map_workspace.h"
#include <optional>

namespace uo_mule::gui
{

void DocumentManager::sync(const MapWorkspace & workspace)
{
    if (workspace.reader() == nullptr)
    {
        close();
        return;
    }

    const auto & workingPath = workspace.workingPath();
    if (!active_ || active_->sourcePath != workspace.originalPath())
    {
        active_ = MapDocument{
            .id = nextId_++,
            .sourcePath = workspace.originalPath(),
            .workingPath = workingPath.value_or(std::filesystem::path{}),
            .dimensions = workspace.dimensions(),
            .dirty = workspace.dirty(),
        };
        return;
    }

    active_->workingPath = workingPath.value_or(std::filesystem::path{});
    active_->dimensions = workspace.dimensions();
    active_->dirty = workspace.dirty();
}

void DocumentManager::close() noexcept
{
    active_.reset();
}

const std::optional<MapDocument> & DocumentManager::active() const noexcept
{
    return active_;
}

bool DocumentManager::hasActiveDocument() const noexcept
{
    return active_.has_value();
}

} // namespace uo_mule::gui
