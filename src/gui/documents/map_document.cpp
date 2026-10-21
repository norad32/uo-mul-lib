#include "gui/documents/map_document.h"
#include "gui/documents/document_identity.h"
#include "gui/metadata/document_contracts.h"
#include "gui/documents/map_workspace.h"
#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace uo_mule::gui
{

void DocumentManager::sync(const MapWorkspace & workspace)
{
    if (workspace.reader() == nullptr)
    {
        const std::optional<DocumentId> activeId = activeId_;
        const DocumentRecord * record = activeId ? find(*activeId) : nullptr;
        if (activeId && record != nullptr && record->map)
        {
            static_cast<void>(close(*activeId));
        }
        return;
    }

    const auto & workingPath = workspace.workingPath();
    const DocumentIdentity identity{
        .id = 0,
        .kind = DocumentKind::map,
        .source = makeDocumentSourceIdentity({workspace.originalPath()}),
    };
    DocumentRecord * record = findByIdentity(identity);
    if (record == nullptr)
    {
        const DocumentId id = nextId_++;
        MapDocument map{
            .id = id,
            .identity = {.id = id, .kind = identity.kind, .source = identity.source},
            .sourcePath = workspace.originalPath(),
            .workingPath = workingPath.value_or(std::filesystem::path{}),
            .dimensions = workspace.dimensions(),
            .dirty = workspace.dirty(),
        };
        record = &documents_.emplace_back(DocumentRecord{.summary = map.summary(), .map = std::move(map)});
    }

    if (!record->map)
    {
        return;
    }

    record->map->workingPath = workingPath.value_or(std::filesystem::path{});
    record->map->dimensions = workspace.dimensions();
    record->map->dirty = workspace.dirty();
    record->summary = record->map->summary();
    activeId_ = record->summary.identity.id;
}

void DocumentManager::close() noexcept
{
    if (activeId_)
    {
        static_cast<void>(close(*activeId_));
    }
}

DocumentId DocumentManager::open(DocumentSummary summary)
{
    if (summary.identity.kind == DocumentKind::map)
    {
        return 0;
    }

    if (DocumentRecord * existing = findByIdentity(summary.identity); existing != nullptr)
    {
        activeId_ = existing->summary.identity.id;
        return *activeId_;
    }

    summary.identity.id = nextId_++;
    DocumentRecord & record = documents_.emplace_back(DocumentRecord{.summary = std::move(summary), .map = std::nullopt});
    activeId_ = record.summary.identity.id;
    return *activeId_;
}

bool DocumentManager::activate(DocumentId id) noexcept
{
    if (find(id) == nullptr)
    {
        return false;
    }
    activeId_ = id;
    return true;
}

bool DocumentManager::close(DocumentId id) noexcept
{
    const auto record = std::ranges::find_if(documents_, [id](const DocumentRecord & candidate) {
        return candidate.summary.identity.id == id;
    });
    if (record == documents_.end())
    {
        return false;
    }

    documents_.erase(record);
    if (activeId_ && *activeId_ == id)
    {
        activeId_.reset();
    }
    return true;
}

const std::vector<DocumentRecord> & DocumentManager::documents() const noexcept
{
    return documents_;
}

const DocumentRecord * DocumentManager::find(DocumentId id) const noexcept
{
    const auto record = std::ranges::find_if(documents_, [id](const DocumentRecord & candidate) {
        return candidate.summary.identity.id == id;
    });
    return record == documents_.end() ? nullptr : &*record;
}

std::optional<DocumentIdentity> DocumentManager::activeIdentity() const
{
    const DocumentRecord * record = activeRecord();
    if (record == nullptr)
    {
        return std::nullopt;
    }
    return record->summary.identity;
}

const std::optional<MapDocument> & DocumentManager::active() const noexcept
{
    const DocumentRecord * record = activeRecord();
    if (record == nullptr || !record->map)
    {
        return emptyMap_;
    }
    return record->map;
}

bool DocumentManager::hasActiveDocument() const noexcept
{
    return activeId_.has_value() && activeRecord() != nullptr;
}

DocumentRecord * DocumentManager::findMutable(DocumentId id) noexcept
{
    const auto record = std::ranges::find_if(documents_, [id](const DocumentRecord & candidate) {
        return candidate.summary.identity.id == id;
    });
    return record == documents_.end() ? nullptr : &*record;
}

DocumentRecord * DocumentManager::findByIdentity(const DocumentIdentity & identity) noexcept
{
    const auto record = std::ranges::find_if(documents_, [&identity](const DocumentRecord & candidate) {
        return sameDocumentIdentity(candidate.summary.identity, identity);
    });
    return record == documents_.end() ? nullptr : &*record;
}

const DocumentRecord * DocumentManager::activeRecord() const noexcept
{
    return activeId_ ? find(*activeId_) : nullptr;
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
