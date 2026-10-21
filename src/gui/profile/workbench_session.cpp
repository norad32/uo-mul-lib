#include "gui/profile/workbench_session.h"
#include "core/facet_sizes.h"
#include "gui/profile/data_profile.h"
#include "gui/profile/data_validation.h"
#include "gui/persistence/recent_maps.h"
#include "gui/context/map_context.h"
#include "gui/documents/map_document.h"
#include "gui/documents/map_workspace.h"
#include "uomul/map.h"
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{

[[nodiscard]] std::string facetForDimensions(uomul::Dimensions dimensions)
{
    for (const auto & facet : uo_mule::core::knownFacetSizes())
    {
        if (uomul::Dimensions{.widthTiles = facet.widthTiles, .heightTiles = facet.heightTiles} == dimensions)
        {
            return std::string(facet.name);
        }
    }
    return {};
}

} // namespace

namespace uo_mule::gui
{

ProfileReadinessState profileReadiness(const DataProfile & profile)
{
    if (profile.name.empty() && profile.files.empty() && profile.sourceRoot.empty())
    {
        return ProfileReadinessState::needsProfile;
    }

    const auto validation = validateDataProfile(profile);
    const auto * radarcol = validation.find(DataFileRole::radarcol);
    if (radarcol == nullptr || radarcol->status == DataFileStatus::missing)
    {
        return ProfileReadinessState::profileIncomplete;
    }
    if (radarcol->status == DataFileStatus::invalid)
    {
        std::error_code error;
        if (radarcol->path.empty() || !std::filesystem::exists(radarcol->path, error))
        {
            return ProfileReadinessState::profileIncomplete;
        }
    }
    if (radarcol->status == DataFileStatus::invalid || radarcol->status == DataFileStatus::ambiguous)
    {
        return ProfileReadinessState::profileInvalid;
    }
    if (radarcol->status == DataFileStatus::foundAndValidated || radarcol->status == DataFileStatus::overridden)
    {
        return ProfileReadinessState::ready;
    }
    return ProfileReadinessState::profileInvalid;
}

bool mapReady(const DataProfile & profile)
{
    return profileReadiness(profile) == ProfileReadinessState::ready;
}

WorkbenchSession::WorkbenchSession() = default;

WorkbenchSession::WorkbenchSession(std::filesystem::path recentPath) : recentMaps_(std::move(recentPath))
{
}

void WorkbenchSession::updateProfile(const DataProfile & profile)
{
    profileState_ = profileReadiness(profile);
    if (profileState_ != ProfileReadinessState::ready)
    {
        startupState_ = StartupState::configureProfile;
    }
    else if (startupState_ == StartupState::configureProfile)
    {
        startupState_ = StartupState::mapWelcome;
    }
}

void WorkbenchSession::markProfileInvalid() noexcept
{
    profileState_ = ProfileReadinessState::profileInvalid;
    startupState_ = StartupState::configureProfile;
}

ProfileReadinessState WorkbenchSession::profileState() const noexcept
{
    return profileState_;
}

bool WorkbenchSession::mapReady() const noexcept
{
    return profileState_ == ProfileReadinessState::ready;
}

void WorkbenchSession::setStartupState(StartupState state) noexcept
{
    startupState_ = state;
}

StartupState WorkbenchSession::startupState() const noexcept
{
    return startupState_;
}

const std::vector<RecentMap> & WorkbenchSession::recentMaps() const noexcept
{
    return recentMaps_.maps();
}

void WorkbenchSession::recordRecentMap(const std::filesystem::path & path, uomul::Dimensions dimensions)
{
    recentMaps_.record(path, dimensions, facetForDimensions(dimensions));
}

void WorkbenchSession::refreshRecentMaps()
{
    recentMaps_.refreshValidity();
}

void WorkbenchSession::syncDocument(const MapWorkspace & workspace)
{
    documents_.sync(workspace);
}

void WorkbenchSession::closeDocument() noexcept
{
    documents_.close();
}

const std::optional<MapDocument> & WorkbenchSession::activeDocument() const noexcept
{
    return documents_.active();
}

MapContextSnapshot WorkbenchSession::contextSnapshot(const MapWorkspace & workspace) const
{
    return makeMapContextSnapshot(workspace, documents_.active());
}

} // namespace uo_mule::gui
