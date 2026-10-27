#pragma once

#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/map_document.h"
#include "gui_model/documents/map_document_manager.h"
#include "gui_model/persistence/recent_files.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/source/source_locks.h"
#include "gui_model/world/map_context.h"
#include <cstdint>

namespace uo_mule::gui
{

enum class ProfileReadinessState : std::uint8_t
{
    noProfile,
    incomplete,
    complete,
};

enum class StartupState : std::uint8_t
{
    configureProfile,
    mapWelcome,
    workbench,
};

[[nodiscard]] ProfileReadinessState profileReadiness(const DataProfile & profile);
[[nodiscard]] bool mapReady(const DataProfile & profile);

class WorkbenchSession
{
public:
    WorkbenchSession();
    explicit WorkbenchSession(std::filesystem::path recentPath);

    void updateProfile(const DataProfile & profile);
    void markProfileIncomplete() noexcept;
    [[nodiscard]] ProfileReadinessState profileState() const noexcept;
    [[nodiscard]] bool mapReady() const noexcept;

    [[nodiscard]] DocumentId openDocument(DocumentSummary summary);
    [[nodiscard]] std::optional<DocumentId> lockedDocument(const DocumentSourceIdentity & identity) const;

    void setStartupState(StartupState state) noexcept;
    [[nodiscard]] StartupState startupState() const noexcept;

    [[nodiscard]] const std::vector<RecentFile> & recentFiles() const noexcept;
    void recordRecentFile(const std::filesystem::path & path, uomul::Dimensions dimensions);
    void recordRecentFile(const std::filesystem::path & path,
                          DocumentKind kind,
                          std::filesystem::path companionPath = {},
                          std::uint32_t itemCount = 0);
    void refreshRecentFiles();
    void syncDocument(MapDocumentManager & mapDocument);
    void syncDocument(const MapDocumentState & mapDocument);
    [[nodiscard]] std::expected<void, uomul::Error> activateDocument(DocumentId id, MapDocumentManager & mapDocument);
    [[nodiscard]] std::expected<void, uomul::Error> activateDocument(DocumentId id, MapDocumentState & mapDocument);
    [[nodiscard]] std::expected<void, uomul::Error> closeDocument(DocumentId id, MapDocumentManager & mapDocument);
    [[nodiscard]] std::expected<void, uomul::Error> closeDocument(DocumentId id, MapDocumentState & mapDocument);
    [[nodiscard]] const std::vector<DocumentRecord> & documents() const noexcept;
    [[nodiscard]] std::vector<DocumentRecord> & documentsMutable() noexcept;
    void syncAssetDocuments() noexcept;
    [[nodiscard]] DocumentRecord * activeDocumentRecord() noexcept;
    [[nodiscard]] const DocumentRecord * activeDocumentRecord() const noexcept;
    [[nodiscard]] std::optional<DocumentId> activeDocumentId() const noexcept;
    [[nodiscard]] std::optional<DocumentIdentity> activeDocumentIdentity() const;
    [[nodiscard]] bool hasDocuments() const noexcept;
    [[nodiscard]] const std::optional<MapDocument> & activeDocument() const noexcept;
    [[nodiscard]] MapContextSnapshot contextSnapshot(const MapDocumentManager & mapDocument) const;
    [[nodiscard]] MapContextSnapshot contextSnapshot(const MapDocumentState & mapDocument) const;

private:
    ProfileReadinessState profileState_ = ProfileReadinessState::noProfile;
    StartupState startupState_ = StartupState::configureProfile;
    RecentFileStore recentFiles_;
    DocumentManager documents_;
    SourceLockRegistry sourceLocks_;
};

} // namespace uo_mule::gui
