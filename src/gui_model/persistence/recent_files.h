#pragma once

#include "gui_model/documents/document_contracts.h"
#include "uomul/map/map.h"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace uo_mule::gui
{

inline constexpr std::size_t recentFileLimit = 5;

struct RecentFile
{
    std::filesystem::path path;
    std::filesystem::path companionPath;
    std::string displayName;
    DocumentKind kind = DocumentKind::map;
    std::uint32_t itemCount = 0;
    std::optional<uomul::Dimensions> dimensions;
    std::string facet;
    std::string failure;
    std::int64_t lastOpened = 0;
    std::int64_t lastModified = 0;
};

[[nodiscard]] std::filesystem::path recentFilesPath();
[[nodiscard]] std::vector<RecentFile> loadRecentFiles(const std::filesystem::path & path);
void saveRecentFiles(const std::vector<RecentFile> & files, const std::filesystem::path & path);

class RecentFileStore
{
public:
    explicit RecentFileStore(std::filesystem::path path = {});

    [[nodiscard]] const std::vector<RecentFile> & files() const noexcept;
    void record(std::filesystem::path path,
                std::optional<uomul::Dimensions> dimensions = std::nullopt,
                std::string facet = {},
                DocumentKind kind = DocumentKind::map,
                std::filesystem::path companionPath = {},
                std::uint32_t itemCount = 0);
    void refreshValidity();

private:
    std::filesystem::path path_;
    std::vector<RecentFile> files_;
};

} // namespace uo_mule::gui
