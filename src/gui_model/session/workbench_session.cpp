#include "gui_model/session/workbench_session.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/documents/map_document.h"
#include "gui_model/documents/map_document_manager.h"
#include "gui_model/documents/map_document_state.h"
#include "gui_model/persistence/recent_files.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/profile/data_validation.h"
#include "gui_model/world/map_context.h"
#include "uomul/map/map.h"
#include "uomul/mul_common/mul_common.h"
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{

[[nodiscard]] uomul::Error makeDocumentSessionError(const std::filesystem::path & path, std::string message)
{
    return {.code = uomul::ErrorCode::io, .path = path, .message = std::move(message)};
}

} // namespace

namespace uo_mule::gui
{

ProfileReadinessState profileReadiness(const DataProfile & profile)
{
    if (profile.name.empty() && profile.files.empty() && profile.sourceRoot.empty())
    {
        return ProfileReadinessState::noProfile;
    }

    const auto validation = validateDataProfile(profile);
    const auto * radarcol = validation.find(DataFileRole::radarcol);
    if (radarcol == nullptr || radarcol->status == DataFileStatus::missing)
    {
        return ProfileReadinessState::incomplete;
    }
    if (radarcol->status == DataFileStatus::invalid)
    {
        std::error_code error;
        if (radarcol->path.empty() || !std::filesystem::exists(radarcol->path, error))
        {
            return ProfileReadinessState::incomplete;
        }
    }
    if (radarcol->status == DataFileStatus::invalid || radarcol->status == DataFileStatus::ambiguous)
    {
        return ProfileReadinessState::incomplete;
    }
    if (radarcol->status == DataFileStatus::found || radarcol->status == DataFileStatus::foundAndValidated)
    {
        return ProfileReadinessState::complete;
    }
    return ProfileReadinessState::incomplete;
}

bool mapReady(const DataProfile & profile)
{
    static_cast<void>(profile);
    return true;
}

WorkbenchSession::WorkbenchSession() = default;

WorkbenchSession::WorkbenchSession(std::filesystem::path recentPath) : recentFiles_(std::move(recentPath))
{
}

void WorkbenchSession::updateProfile(const DataProfile & profile)
{
    documents_.updateMetadataSources(profile);
    profileState_ = profileReadiness(profile);
    if (startupState_ == StartupState::configureProfile && ::uo_mule::gui::mapReady(profile))
    {
        startupState_ = StartupState::mapWelcome;
    }
}

DocumentId WorkbenchSession::openDocument(DocumentSummary summary)
{
    if (const auto locked = sourceLocks_.owner(summary.identity.source))
    {
        static_cast<void>(documents_.activate(*locked));
        return *locked;
    }

    const DocumentId id = documents_.open(std::move(summary));
    if (id != 0)
    {
        const auto * record = documents_.find(id);
        if (record != nullptr && !sourceLocks_.acquire(id, record->summary.identity.source))
        {
            return 0;
        }
    }
    return id;
}

std::optional<DocumentId> WorkbenchSession::lockedDocument(const DocumentSourceIdentity & identity) const
{
    return sourceLocks_.owner(identity);
}

void WorkbenchSession::markProfileIncomplete() noexcept
{
    profileState_ = ProfileReadinessState::incomplete;
    // A broken profile must not prevent map use; providers will use embedded Radarcol data.
    startupState_ = StartupState::mapWelcome;
}

ProfileReadinessState WorkbenchSession::profileState() const noexcept
{
    return profileState_;
}

bool WorkbenchSession::mapReady() const noexcept
{
    switch (profileState_)
    {
        case ProfileReadinessState::noProfile :
        case ProfileReadinessState::incomplete :
        case ProfileReadinessState::complete :
            return true;
    }
    return false;
}

void WorkbenchSession::setStartupState(StartupState state) noexcept
{
    startupState_ = state;
}

StartupState WorkbenchSession::startupState() const noexcept
{
    return startupState_;
}

const std::vector<RecentFile> & WorkbenchSession::recentFiles() const noexcept
{
    return recentFiles_.files();
}

void WorkbenchSession::recordRecentFile(const std::filesystem::path & path, uomul::Dimensions dimensions)
{
    recentFiles_.record(path, dimensions, mapFacetKey(dimensions));
}

void WorkbenchSession::recordRecentFile(const std::filesystem::path & path,
                                        DocumentKind kind,
                                        std::filesystem::path companionPath,
                                        std::uint32_t itemCount)
{
    recentFiles_.record(path, std::nullopt, {}, kind, std::move(companionPath), itemCount);
}

void WorkbenchSession::refreshRecentFiles()
{
    recentFiles_.refreshValidity();
}

void WorkbenchSession::syncDocument(MapDocumentManager & mapDocument)
{
    const auto previousActiveId = documents_.activeId();
    documents_.sync(mapDocument.active());
    const auto mapDocumentId = documents_.activeId();
    sourceLocks_.clear();
    for (const auto & document : documents_.documents())
    {
        static_cast<void>(sourceLocks_.acquire(document.summary.identity.id, document.summary.identity.source));
    }
    if (mapDocument.active().changes().reader)
    {
        if (mapDocumentId)
        {
            mapDocument.bindActiveDocument(*mapDocumentId);
        }
    }
    if (previousActiveId && previousActiveId != mapDocumentId)
    {
        if (const auto * previous = documents_.find(*previousActiveId); previous != nullptr && !previous->map)
        {
            static_cast<void>(documents_.activate(*previousActiveId));
        }
    }
}

void WorkbenchSession::syncDocument(const MapDocumentState & mapDocument)
{
    documents_.sync(mapDocument);
    sourceLocks_.clear();
    for (const auto & document : documents_.documents())
    {
        static_cast<void>(sourceLocks_.acquire(document.summary.identity.id, document.summary.identity.source));
    }
}

std::expected<void, uomul::Error> WorkbenchSession::activateDocument(DocumentId id, MapDocumentManager & mapDocument)
{
    const DocumentRecord * record = documents_.find(id);
    if (record == nullptr)
    {
        return std::unexpected(makeDocumentSessionError({}, "document is not open"));
    }

    if (record->map)
    {
        if (auto activated = mapDocument.activateMap(id, record->map->sourcePath, record->map->dimensions); !activated)
        {
            return std::unexpected(activated.error());
        }
        static_cast<void>(documents_.activate(id));
        documents_.sync(mapDocument.active());
        return {};
    }

    static_cast<void>(documents_.activate(id));
    return {};
}

std::expected<void, uomul::Error> WorkbenchSession::activateDocument(DocumentId id, MapDocumentState & mapDocument)
{
    const DocumentRecord * record = documents_.find(id);
    if (record == nullptr)
    {
        return std::unexpected(makeDocumentSessionError({}, "document is not open"));
    }
    if (record->map && documents_.activeId() != id)
    {
        if (auto opened = mapDocument.openMap(record->map->sourcePath, record->map->dimensions); !opened)
        {
            return std::unexpected(opened.error());
        }
    }
    documents_.sync(mapDocument);
    return {};
}

std::expected<void, uomul::Error> WorkbenchSession::closeDocument(DocumentId id, MapDocumentManager & mapDocument)
{
    const DocumentRecord * record = documents_.find(id);
    if (record == nullptr)
    {
        return std::unexpected(makeDocumentSessionError({}, "document is not open"));
    }

    if (record->map && mapDocument.activeDocumentId() == id && mapDocument.active().changes().reader)
    {
        if (auto closed = mapDocument.closeMap(); !closed)
        {
            return std::unexpected(closed.error());
        }
    }
    if (!documents_.close(id))
    {
        return std::unexpected(makeDocumentSessionError({}, "document could not be closed"));
    }
    sourceLocks_.release(id);
    return {};
}

std::expected<void, uomul::Error> WorkbenchSession::closeDocument(DocumentId id, MapDocumentState & mapDocument)
{
    const DocumentRecord * record = documents_.find(id);
    if (record == nullptr)
    {
        return std::unexpected(makeDocumentSessionError({}, "document is not open"));
    }
    if (record->map && documents_.activeId() == id && mapDocument.changes().reader)
    {
        if (auto closed = mapDocument.closeMap(); !closed)
        {
            return std::unexpected(closed.error());
        }
    }
    if (!documents_.close(id))
    {
        return std::unexpected(makeDocumentSessionError({}, "document could not be closed"));
    }
    sourceLocks_.release(id);
    return {};
}

const std::vector<DocumentRecord> & WorkbenchSession::documents() const noexcept
{
    return documents_.documents();
}

std::vector<DocumentRecord> & WorkbenchSession::documentsMutable() noexcept
{
    return documents_.documentsMutable();
}

void WorkbenchSession::syncAssetDocuments() noexcept
{
    documents_.syncAssetDocuments();
}

DocumentRecord * WorkbenchSession::activeDocumentRecord() noexcept
{
    const auto activeId = documents_.activeId();
    return activeId ? documents_.findMutable(*activeId) : nullptr;
}

const DocumentRecord * WorkbenchSession::activeDocumentRecord() const noexcept
{
    const auto activeId = documents_.activeId();
    return activeId ? documents_.find(*activeId) : nullptr;
}

std::optional<DocumentId> WorkbenchSession::activeDocumentId() const noexcept
{
    return documents_.activeId();
}

std::optional<DocumentIdentity> WorkbenchSession::activeDocumentIdentity() const
{
    return documents_.activeIdentity();
}

bool WorkbenchSession::hasDocuments() const noexcept
{
    return !documents_.documents().empty();
}

const std::optional<MapDocument> & WorkbenchSession::activeDocument() const noexcept
{
    return documents_.active();
}

MapContextSnapshot WorkbenchSession::contextSnapshot(const MapDocumentManager & mapDocument) const
{
    return makeMapContextSnapshot(mapDocument.active(), documents_.active());
}

MapContextSnapshot WorkbenchSession::contextSnapshot(const MapDocumentState & mapDocument) const
{
    return makeMapContextSnapshot(mapDocument, documents_.active());
}

} // namespace uo_mule::gui
