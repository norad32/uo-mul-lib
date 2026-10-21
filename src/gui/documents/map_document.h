#pragma once

#include "gui/documents/document_identity.h"
#include "gui/documents/map_workspace.h"
#include <filesystem>
#include <optional>
#include <vector>

namespace uo_mule::gui
{

struct DataProfile;

struct MapDocument
{
    DocumentId id = 0;
    DocumentIdentity identity;
    std::filesystem::path sourcePath;
    std::filesystem::path workingPath;
    uomul::Dimensions dimensions{};
    bool dirty = false;

    [[nodiscard]] DocumentSummary summary() const;
};

struct DocumentRecord
{
    DocumentSummary summary;
    std::optional<MapDocument> map;
};

class DocumentManager
{
public:
    void sync(const MapWorkspace & workspace);
    void close() noexcept;
    void updateMetadataSources(const DataProfile & profile);

    [[nodiscard]] DocumentId open(DocumentSummary summary);
    [[nodiscard]] bool activate(DocumentId id) noexcept;
    [[nodiscard]] bool close(DocumentId id) noexcept;

    [[nodiscard]] const std::vector<DocumentRecord> & documents() const noexcept;
    [[nodiscard]] const DocumentRecord * find(DocumentId id) const noexcept;
    [[nodiscard]] std::optional<DocumentId> activeId() const noexcept;
    [[nodiscard]] std::optional<DocumentIdentity> activeIdentity() const;

    [[nodiscard]] const std::optional<MapDocument> & active() const noexcept;
    [[nodiscard]] bool hasActiveDocument() const noexcept;

private:
    [[nodiscard]] DocumentRecord * findMutable(DocumentId id) noexcept;
    [[nodiscard]] DocumentRecord * findByIdentity(const DocumentIdentity & identity) noexcept;
    [[nodiscard]] const DocumentRecord * activeRecord() const noexcept;

    DocumentId nextId_ = 1;
    std::vector<DocumentRecord> documents_;
    std::optional<DocumentId> activeId_;
    std::optional<MapDocument> emptyMap_;
    std::optional<DocumentSourceIdentity> configuredRadarcolSource_;
    std::optional<DocumentSourceIdentity> configuredTexmapPairSource_;
};

} // namespace uo_mule::gui
