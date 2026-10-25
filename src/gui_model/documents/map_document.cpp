#include "gui_model/documents/map_document.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/documents/map_document_state.h"
#include "gui_model/documents/radarcol_document.h"
#include "gui_model/documents/texmaps_document.h"
#include "gui_model/documents/tiledata_document.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/source/source_identity.h"
#include "gui_model/source/tiledata_source.h"
#include "uomul/facet_sizes.h"
#include "uomul/map/map.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/tiledata/tiledata.h"
#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace uo_mule::gui
{

namespace
{

[[nodiscard]] bool configuredMetadataSource(const DocumentIdentity & identity,
                                            const std::optional<DocumentSourceIdentity> & radarcolSource,
                                            const std::optional<DocumentSourceIdentity> & texmapPairSource,
                                            const std::optional<DocumentSourceIdentity> & tiledataSource)
{
    switch (identity.kind)
    {
        case DocumentKind::radarcol :
            return radarcolSource.has_value() && identity.source == radarcolSource.value();
        case DocumentKind::texmapPair :
            return texmapPairSource.has_value() && identity.source == texmapPairSource.value();
        case DocumentKind::tiledata :
            return tiledataSource.has_value() && identity.source == tiledataSource.value();
        case DocumentKind::map :
        case DocumentKind::statics :
        case DocumentKind::combined :
        case DocumentKind::unavailable :
            return false;
    }
    return false;
}

void updateMetadataSource(DocumentRecord & document, bool configured)
{
    const auto kind = document.summary.identity.kind;
    if (kind != DocumentKind::radarcol && kind != DocumentKind::texmapPair && kind != DocumentKind::tiledata)
    {
        return;
    }

    document.summary.sourceKind = configured ? DocumentSourceKind::configuredMetadata : DocumentSourceKind::standalone;
}

[[nodiscard]] std::optional<RadarcolDocument> openRadarcolDocument(DocumentSummary & summary)
{
    if (summary.identity.kind != DocumentKind::radarcol)
    {
        return std::nullopt;
    }
    const auto * sourcePath = summary.identity.source.path(SourceRole::primary);
    if (sourcePath == nullptr)
    {
        return std::nullopt;
    }

    auto opened = RadarcolDocument::open(summary.identity.id, *sourcePath);
    if (opened)
    {
        summary.validation = DocumentValidationState::valid;
        return std::move(*opened);
    }

    summary.validation = DocumentValidationState::invalid;
    return RadarcolDocument::invalid(summary.identity.id, *sourcePath, opened.error().message);
}

[[nodiscard]] std::optional<TexmapsDocument> openTexmapsDocument(DocumentSummary & summary)
{
    if (summary.identity.kind != DocumentKind::texmapPair)
    {
        return std::nullopt;
    }
    const auto * indexPath = summary.identity.source.path(SourceRole::index);
    const auto * dataPath = summary.identity.source.path(SourceRole::data);
    if (indexPath == nullptr || dataPath == nullptr)
    {
        summary.validation = DocumentValidationState::invalid;
        return TexmapsDocument::invalid(summary.identity.id,
                                        indexPath == nullptr ? std::filesystem::path{} : *indexPath,
                                        dataPath == nullptr ? std::filesystem::path{} : *dataPath,
                                        "Texmaps file set is not available");
    }

    auto opened = TexmapsDocument::open(summary.identity.id, *indexPath, *dataPath);
    if (opened)
    {
        summary.validation = DocumentValidationState::valid;
        return std::move(*opened);
    }

    summary.validation = DocumentValidationState::invalid;
    return TexmapsDocument::invalid(summary.identity.id, *indexPath, *dataPath, opened.error().message);
}

[[nodiscard]] std::optional<TiledataDocument> openTiledataDocument(DocumentSummary & summary)
{
    if (summary.identity.kind != DocumentKind::tiledata)
    {
        return std::nullopt;
    }
    const auto * sourcePath = summary.identity.source.path(SourceRole::primary);
    if (sourcePath == nullptr)
    {
        return std::nullopt;
    }

    const auto layout =
        resolveTiledataLayout(*sourcePath, TiledataLayoutSelection::autoDetect).value_or(uomul::TiledataLayout::extended64);
    auto opened = TiledataDocument::open(summary.identity.id, *sourcePath, layout);
    if (opened)
    {
        summary.validation = DocumentValidationState::valid;
        return std::move(*opened);
    }

    summary.validation = DocumentValidationState::invalid;
    return TiledataDocument::invalid(summary.identity.id, *sourcePath, layout, opened.error().message);
}

} // namespace

std::string mapFacetKey(uomul::Dimensions dimensions)
{
    for (const auto & facet : uomul::knownFacetSizes())
    {
        if (facet.widthTiles == dimensions.widthTiles && facet.heightTiles == dimensions.heightTiles)
        {
            return std::string(facet.name);
        }
    }
    return "custom-" + std::to_string(dimensions.widthTiles) + "x" + std::to_string(dimensions.heightTiles);
}

void DocumentManager::sync(const MapDocumentState & mapDocument)
{
    if (!mapDocument.changes().reader)
    {
        const auto record =
            std::ranges::find_if(documents_, [](const DocumentRecord & candidate) { return candidate.map.has_value(); });
        if (record != documents_.end())
        {
            static_cast<void>(close(record->summary.identity.id));
        }
        return;
    }

    const auto & workingPath = mapDocument.changes().workingPath;
    const DocumentIdentity identity{
        .id = 0,
        .kind = DocumentKind::map,
        .source = makeDocumentSourceIdentity({mapDocument.changes().originalPath}),
    };
    DocumentRecord * record = findByIdentity(identity);
    if (record == nullptr)
    {
        const DocumentId id = nextId_++;
        MapDocument map{
            .id = id,
            .identity = {.id = id, .kind = identity.kind, .source = identity.source},
            .sourcePath = mapDocument.changes().originalPath,
            .workingPath = workingPath.value_or(std::filesystem::path{}),
            .facetKey = mapFacetKey(mapDocument.changes().dimensions),
            .dimensions = mapDocument.changes().dimensions,
            .dirty = mapDocument.changes().dirty(),
        };
        record = &documents_.emplace_back(DocumentRecord{
            .summary = map.summary(),
            .map = std::move(map),
            .radarcol = std::nullopt,
            .tiledata = std::nullopt,
            .texmaps = std::nullopt,
        });
    }

    if (!record->map)
    {
        return;
    }

    record->map->workingPath = workingPath.value_or(std::filesystem::path{});
    record->map->facetKey = mapFacetKey(mapDocument.changes().dimensions);
    record->map->dimensions = mapDocument.changes().dimensions;
    record->map->dirty = mapDocument.changes().dirty();
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
    configuredTiledataSource_ = configuredDocumentSourceIdentity(profile, DocumentKind::tiledata);

    for (auto & document : documents_)
    {
        const bool configured = configuredMetadataSource(
            document.summary.identity, configuredRadarcolSource_, configuredTexmapPairSource_, configuredTiledataSource_);
        updateMetadataSource(document, configured);
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
    const bool configured = configuredMetadataSource(
        summary.identity, configuredRadarcolSource_, configuredTexmapPairSource_, configuredTiledataSource_);
    summary.sourceKind = configured ? DocumentSourceKind::configuredMetadata : DocumentSourceKind::standalone;

    auto radarcol = openRadarcolDocument(summary);
    auto texmaps = openTexmapsDocument(summary);
    auto tiledata = openTiledataDocument(summary);

    DocumentRecord & record = documents_.emplace_back(DocumentRecord{
        .summary = std::move(summary),
        .map = std::nullopt,
        .radarcol = std::move(radarcol),
        .tiledata = std::move(tiledata),
        .texmaps = std::move(texmaps),
    });
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
    const auto record = std::ranges::find_if(
        documents_, [id](const DocumentRecord & candidate) { return candidate.summary.identity.id == id; });
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
    const auto record = std::ranges::find_if(
        documents_, [id](const DocumentRecord & candidate) { return candidate.summary.identity.id == id; });
    return record == documents_.end() ? nullptr : &*record;
}

void DocumentManager::syncAssetDocuments() noexcept
{
    for (auto & document : documents_)
    {
        if (document.radarcol)
        {
            const auto source = makeDocumentSourceIdentity({document.radarcol->sourcePath()});
            const bool configured = configuredRadarcolSource_.has_value() && source == *configuredRadarcolSource_;
            document.summary.identity.source = source;
            document.summary.displayName = document.radarcol->sourcePath().filename().string();
            document.summary.sourceKind = configured ? DocumentSourceKind::configuredMetadata : DocumentSourceKind::standalone;
            document.summary.dirty = document.radarcol->dirty();
            document.summary.validation =
                document.radarcol->reader().has_value() ? DocumentValidationState::valid : DocumentValidationState::invalid;
        }
        if (document.texmaps)
        {
            const auto source = document.texmaps->sourceIdentity();
            const bool configured = configuredTexmapPairSource_.has_value() && source == *configuredTexmapPairSource_;
            document.summary.identity.source = source;
            document.summary.displayName = document.texmaps->sourceIndexPath().filename().string();
            document.summary.sourceKind = configured ? DocumentSourceKind::configuredMetadata : DocumentSourceKind::standalone;
            document.summary.dirty = document.texmaps->dirty();
            document.summary.validation =
                document.texmaps->reader().has_value() ? DocumentValidationState::valid : DocumentValidationState::invalid;
        }
    }
}

const DocumentRecord * DocumentManager::find(DocumentId id) const noexcept
{
    const auto record = std::ranges::find_if(
        documents_, [id](const DocumentRecord & candidate) { return candidate.summary.identity.id == id; });
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
    const auto record = std::ranges::find_if(documents_,
                                             [&identity](const DocumentRecord & candidate)
                                             { return sameDocumentIdentity(candidate.summary.identity, identity); });
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
    result.validation = DocumentValidationState::valid;
    result.facetKey = facetKey;
    result.dimensions = dimensions;
    result.dirty = dirty;
    return result;
}

} // namespace uo_mule::gui
