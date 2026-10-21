#pragma once

#include "uomul/map.h"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace uo_mule::gui
{

inline constexpr std::size_t recentFileLimit = 5;

struct RecentMap
{
    std::filesystem::path path;
    std::string displayName;
    std::optional<uomul::Dimensions> dimensions;
    std::string facet;
    std::string failure;
    std::int64_t lastOpened = 0;
    std::int64_t lastModified = 0;
};

[[nodiscard]] std::filesystem::path recentMapsPath();
[[nodiscard]] std::vector<RecentMap> loadRecentMaps(const std::filesystem::path & path);
void saveRecentMaps(const std::vector<RecentMap> & maps, const std::filesystem::path & path);

class RecentMapStore
{
public:
    explicit RecentMapStore(std::filesystem::path path = {});

    [[nodiscard]] const std::vector<RecentMap> & maps() const noexcept;
    void record(std::filesystem::path path, std::optional<uomul::Dimensions> dimensions = std::nullopt, std::string facet = {});
    void refreshValidity();

private:
    std::filesystem::path path_;
    std::vector<RecentMap> maps_;
};

} // namespace uo_mule::gui
