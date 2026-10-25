#pragma once

#include "gui_model/model/assets/asset_export_contracts.h"
#include "image_codec/image_codec.h"
#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace uo_mule::gui
{

struct TexmapExportItem
{
    std::uint32_t textureId = 0;
    uomul::TextureImage texture;
};

struct TexmapExportOptions
{
    assets::AssetExportFormat format = assets::AssetExportFormat::bmp;
    assets::AssetExportDestination destination = assets::AssetExportDestination::file;
    std::filesystem::path outputPath;
    bool overwrite = false;
};

struct TexmapExportFailure
{
    std::uint32_t textureId = 0;
    std::filesystem::path outputPath;
    std::string message;
};

struct TexmapExportReport
{
    std::size_t exported = 0;
    std::vector<TexmapExportFailure> failures;
};

[[nodiscard]] std::string texmapExportFilename(std::uint32_t textureId, assets::AssetExportFormat format);

[[nodiscard]] std::expected<std::vector<std::filesystem::path>, uomul::Error>
texmapExportOutputPaths(const std::vector<TexmapExportItem> & items, const TexmapExportOptions & options);

[[nodiscard]] std::expected<TexmapExportReport, uomul::Error> exportTexmaps(const std::vector<TexmapExportItem> & items,
                                                                            const TexmapExportOptions & options);

} // namespace uo_mule::gui
