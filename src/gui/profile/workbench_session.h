#pragma once

#include "gui/profile/data_profile.h"
#include "gui/context/map_context.h"
#include "gui/documents/map_document.h"
#include "gui/documents/map_workspace_manager.h"
#include "gui/persistence/recent_files.h"
#include "gui/metadata/document_contracts.h"
#include <cstdint>

namespace uo_mule::gui
{

enum class ProfileReadinessState : std::uint8_t
{
    needsProfile,
    profileInvalid,
    profileIncomplete,
    ready,
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
    void markProfileInvalid() noexcept;
    [[nodiscard]] ProfileReadinessState profileState() const noexcept;
    [[nodiscard]] bool mapReady() const noexcept;

    [[nodiscard]] DocumentId openDocument(DocumentSummary summary);

    void setStartupState(StartupState state) noexcept;
    [[nodiscard]] StartupState startupState() const noexcept;

    [[nodiscard]] const std::vector<RecentFile> & recentFiles() const noexcept;
    void recordRecentFile(const std::filesystem::path & path, uomul::Dimensions dimensions);
    void recordRecentFile(const std::filesystem::path & path,
                          DocumentKind kind,
                          std::filesystem::path companionPath = {},
                          std::uint32_t itemCount = 0);
    void refreshRecentFiles();
    void syncDocument(MapWorkspaceManager & workspace);
    void syncDocument(const MapWorkspace & workspace);
    [[nodiscard]] std::expected<void, uomul::Error> activateDocument(DocumentId id, MapWorkspaceManager & workspace);
    [[nodiscard]] std::expected<void, uomul::Error> activateDocument(DocumentId id, MapWorkspace & workspace);
    [[nodiscard]] std::expected<void, uomul::Error> closeDocument(DocumentId id, MapWorkspaceManager & workspace);
    [[nodiscard]] std::expected<void, uomul::Error> closeDocument(DocumentId id, MapWorkspace & workspace);
    [[nodiscard]] const std::vector<DocumentRecord> & documents() const noexcept;
    [[nodiscard]] std::vector<DocumentRecord> & documentsMutable() noexcept;
    void syncAssetDocuments() noexcept;
    [[nodiscard]] DocumentRecord * activeDocumentRecord() noexcept;
    [[nodiscard]] const DocumentRecord * activeDocumentRecord() const noexcept;
    [[nodiscard]] std::optional<DocumentId> activeDocumentId() const noexcept;
    [[nodiscard]] std::optional<DocumentIdentity> activeDocumentIdentity() const;
    [[nodiscard]] bool hasDocuments() const noexcept;
    [[nodiscard]] const std::optional<MapDocument> & activeDocument() const noexcept;
    [[nodiscard]] MapContextSnapshot contextSnapshot(const MapWorkspaceManager & workspace) const;
    [[nodiscard]] MapContextSnapshot contextSnapshot(const MapWorkspace & workspace) const;

private:
    ProfileReadinessState profileState_ = ProfileReadinessState::needsProfile;
    StartupState startupState_ = StartupState::configureProfile;
    RecentFileStore recentFiles_;
    DocumentManager documents_;
};

} // namespace uo_mule::gui
