#include "gui/documents/map_document.h"
#include "gui/documents/document_identity.h"
#include "gui/metadata/document_contracts.h"
#include "gui/documents/map_workspace.h"
#include <optional>
#include <string>

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
    const DocumentIdentity identity{
        .id = nextId_,
        .kind = DocumentKind::map,
        .source = makeDocumentSourceIdentity({workspace.originalPath()}),
    };
    if (!active_ || !sameDocumentIdentity(active_->identity, identity))
    {
        active_ = MapDocument{
            .id = nextId_++,
            .identity = identity,
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
    active_->identity.id = active_->id;
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

DocumentSummary MapDocument::summary() const
{
    DocumentSummary result;
    result.identity = identity;
    result.identity.id = id;
    result.displayName = sourcePath.filename().string();
    if (result.displayName.empty())
    {
        result.displayName = std::string(documentKindName(DocumentKind::map));
    }
    result.sourceKind = DocumentSourceKind::standalone;
    result.access = DocumentAccess::editable;
    result.validation = DocumentValidationState::valid;
    result.dirty = dirty;
    return result;
}

} // namespace uo_mule::gui
