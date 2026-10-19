#pragma once

#include "gui/data_profile.h"
#include "gui/map_context.h"
#include "gui/map_document.h"
#include "gui/recent_maps.h"
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

    void setStartupState(StartupState state) noexcept;
    [[nodiscard]] StartupState startupState() const noexcept;

    [[nodiscard]] const std::vector<RecentMap> & recentMaps() const noexcept;
    void recordRecentMap(const std::filesystem::path & path, uomul::Dimensions dimensions);
    void refreshRecentMaps();
    void syncDocument(const MapWorkspace & workspace);
    void closeDocument() noexcept;
    [[nodiscard]] const std::optional<MapDocument> & activeDocument() const noexcept;
    [[nodiscard]] MapContextSnapshot contextSnapshot(const MapWorkspace & workspace) const;

private:
    ProfileReadinessState profileState_ = ProfileReadinessState::needsProfile;
    StartupState startupState_ = StartupState::configureProfile;
    RecentMapStore recentMaps_;
    DocumentManager documents_;
};

} // namespace uo_mule::gui
