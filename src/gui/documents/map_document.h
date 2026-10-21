#pragma once

#include "gui/documents/document_identity.h"
#include "gui/documents/map_workspace.h"
#include <cstdint>
#include <filesystem>
#include <optional>

namespace uo_mule::gui
{

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

class DocumentManager
{
public:
    void sync(const MapWorkspace & workspace);
    void close() noexcept;

    [[nodiscard]] const std::optional<MapDocument> & active() const noexcept;
    [[nodiscard]] bool hasActiveDocument() const noexcept;

private:
    DocumentId nextId_ = 1;
    std::optional<MapDocument> active_;
};

} // namespace uo_mule::gui
