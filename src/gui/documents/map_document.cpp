#include "gui/documents/map_document.h"
#include "gui/documents/document_identity.h"
#include "gui/metadata/document_contracts.h"
#include "gui/documents/map_workspace.h"
#include "gui/documents/radarcol_document.h"
#include "gui/profile/data_profile.h"
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
        record = &documents_.emplace_back(
            DocumentRecord{.summary = map.summary(), .map = std::move(map), .radarcol = std::nullopt});
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

void DocumentManager::updateMetadataSources(const DataProfile & profile)
{
    configuredRadarcolSource_ = configuredDocumentSourceIdentity(profile, DocumentKind::radarcol);
    configuredTexmapPairSource_ = configuredDocumentSourceIdentity(profile, DocumentKind::texmapPair);

    for (auto & document : documents_)
    {
        const auto & identity = document.summary.identity;
        bool configured = false;
        switch (identity.kind)
        {
            case DocumentKind::radarcol :
                configured = configuredRadarcolSource_.has_value() && identity.source == configuredRadarcolSource_.value();
                break;
            case DocumentKind::texmapPair :
                configured = configuredTexmapPairSource_.has_value() &&
                             identity.source == configuredTexmapPairSource_.value();
                break;
            case DocumentKind::map :
                break;
        }
        if (identity.kind == DocumentKind::radarcol)
        {
            document.summary.sourceKind = configured ? DocumentSourceKind::configuredMetadata : DocumentSourceKind::standalone;
            document.summary.access = configured ? DocumentAccess::readOnly : DocumentAccess::editable;
            if (document.radarcol)
            {
                document.radarcol->setAccess(document.summary.access);
            }
        }
        else if (identity.kind == DocumentKind::texmapPair)
        {
            document.summary.sourceKind = configured ? DocumentSourceKind::configuredMetadata : DocumentSourceKind::standalone;
            document.summary.access = DocumentAccess::readOnly;
        }
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
    bool configured = false;
    switch (summary.identity.kind)
    {
        case DocumentKind::radarcol :
            configured = configuredRadarcolSource_.has_value() &&
                         summary.identity.source == configuredRadarcolSource_.value();
            break;
        case DocumentKind::texmapPair :
            configured = configuredTexmapPairSource_.has_value() &&
                         summary.identity.source == configuredTexmapPairSource_.value();
            break;
        case DocumentKind::map :
            break;
    }
    summary.sourceKind = configured ? DocumentSourceKind::configuredMetadata : DocumentSourceKind::standalone;
    summary.access = summary.identity.kind == DocumentKind::radarcol && !configured
                         ? DocumentAccess::editable
                         : DocumentAccess::readOnly;

    std::optional<RadarcolDocument> radarcol;
    if (summary.identity.kind == DocumentKind::radarcol && !summary.identity.source.canonicalPaths.empty())
    {
        auto opened = RadarcolDocument::open(summary.identity.id,
                                             summary.identity.source.canonicalPaths.front(),
                                             summary.access);
        if (opened)
        {
            summary.validation = DocumentValidationState::valid;
            radarcol = std::move(*opened);
        }
        else
        {
            summary.validation = DocumentValidationState::invalid;
            radarcol = RadarcolDocument::invalid(summary.identity.id,
                                                 summary.identity.source.canonicalPaths.front(),
                                                 summary.access,
                                                 opened.error().message);
        }
    }

    DocumentRecord & record = documents_.emplace_back(
        DocumentRecord{.summary = std::move(summary), .map = std::nullopt, .radarcol = std::move(radarcol)});
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

std::vector<DocumentRecord> & DocumentManager::documentsMutable() noexcept
{
    return documents_;
}

DocumentRecord * DocumentManager::findMutable(DocumentId id) noexcept
{
    const auto record = std::ranges::find_if(documents_, [id](const DocumentRecord & candidate) {
        return candidate.summary.identity.id == id;
    });
    return record == documents_.end() ? nullptr : &*record;
}

void DocumentManager::syncAssetDocuments() noexcept
{
    for (auto & document : documents_)
    {
        if (document.radarcol)
        {
            const auto source = makeDocumentSourceIdentity({document.radarcol->sourcePath()});
            const bool configured = configuredRadarcolSource_.has_value() && source == configuredRadarcolSource_.value();
            document.summary.identity.source = source;
            document.summary.displayName = document.radarcol->sourcePath().filename().string();
            document.summary.sourceKind = configured ? DocumentSourceKind::configuredMetadata : DocumentSourceKind::standalone;
            document.summary.access = configured ? DocumentAccess::readOnly : DocumentAccess::editable;
            document.radarcol->setAccess(document.summary.access);
            document.summary.dirty = document.radarcol->dirty();
            document.summary.validation = document.radarcol->reader().has_value() ? DocumentValidationState::valid
                                                                                    : DocumentValidationState::invalid;
        }
    }
}

const DocumentRecord * DocumentManager::find(DocumentId id) const noexcept
{
    const auto record = std::ranges::find_if(documents_, [id](const DocumentRecord & candidate) {
        return candidate.summary.identity.id == id;
    });
    return record == documents_.end() ? nullptr : &*record;
}

std::optional<DocumentId> DocumentManager::activeId() const noexcept
{
    return activeId_;
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
