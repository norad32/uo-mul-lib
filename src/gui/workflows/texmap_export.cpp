#include "gui/workflows/texmap_export.h"
#include "gui_model/model/assets/asset_export_contracts.h"
#include "gui_model/providers/color_conversion.h"
#include "gui_model/world/map_color.h"
#include "image_codec/image_codec.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/texmaps/texmaps.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <iomanip>
#include <ios>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace uo_mule::gui
{
namespace
{

constexpr int exportFilenameWidth = 4;

[[nodiscard]] std::string_view extension(assets::AssetExportFormat format)
{
    return format == assets::AssetExportFormat::bmp ? ".bmp" : ".png";
}

[[nodiscard]] uomul::Error exportError(const std::filesystem::path & path, std::string message)
{
    return {.code = uomul::ErrorCode::io, .path = path, .message = std::move(message)};
}

[[nodiscard]] std::expected<std::vector<std::uint8_t>, uomul::Error> rgbPixels(const TexmapExportItem & item,
                                                                               const std::filesystem::path & outputPath)
{
    const auto dimension = static_cast<std::uint32_t>(item.texture.dimension);
    if (dimension != uomul::textureSmallDimension && dimension != uomul::textureLargeDimension)
    {
        return std::unexpected(exportError(outputPath, "texture dimensions are not supported"));
    }
    const auto pixelCount = static_cast<std::size_t>(dimension) * dimension;
    if (item.texture.pixels.size() != pixelCount)
    {
        return std::unexpected(exportError(outputPath, "texture pixel count is invalid"));
    }
    std::vector<std::uint8_t> pixels;
    pixels.reserve(pixelCount * 3U);
    for (const auto pixel : item.texture.pixels)
    {
        const auto color = texturePixelToMapColor(pixel);
        pixels.push_back(color.red);
        pixels.push_back(color.green);
        pixels.push_back(color.blue);
    }
    return pixels;
}

} // namespace

std::string texmapExportFilename(std::uint32_t textureId, assets::AssetExportFormat format)
{
    std::ostringstream filename;
    filename << "0x" << std::uppercase << std::hex << std::setfill('0') << std::setw(exportFilenameWidth) << textureId
             << extension(format);
    return filename.str();
}

std::expected<std::vector<std::filesystem::path>, uomul::Error>
texmapExportOutputPaths(const std::vector<TexmapExportItem> & items, const TexmapExportOptions & options)
{
    if (items.empty())
    {
        return std::unexpected(exportError(options.outputPath, "no textures are selected for export"));
    }
    if (options.outputPath.empty())
    {
        return std::unexpected(exportError(options.outputPath, "an export path is required"));
    }
    if (options.destination == assets::AssetExportDestination::file && items.size() != 1U)
    {
        return std::unexpected(exportError(options.outputPath, "multiple textures require a directory destination"));
    }
    if (options.destination == assets::AssetExportDestination::directory && !std::filesystem::is_directory(options.outputPath))
    {
        return std::unexpected(exportError(options.outputPath, "export destination is not a directory"));
    }

    std::vector<std::filesystem::path> paths;
    paths.reserve(items.size());
    for (const auto & item : items)
    {
        paths.push_back(options.destination == assets::AssetExportDestination::file
                            ? options.outputPath
                            : options.outputPath / texmapExportFilename(item.textureId, options.format));
    }
    return paths;
}

std::expected<TexmapExportReport, uomul::Error> exportTexmaps(const std::vector<TexmapExportItem> & items,
                                                              const TexmapExportOptions & options)
{
    const auto paths = texmapExportOutputPaths(items, options);
    if (!paths)
    {
        return std::unexpected(paths.error());
    }

    std::error_code error;
    for (const auto & path : *paths)
    {
        if (std::filesystem::exists(path, error) && !options.overwrite)
        {
            return std::unexpected(exportError(path, "overwrite confirmation required"));
        }
        if (error)
        {
            return std::unexpected(exportError(path, error.message()));
        }
    }

    TexmapExportReport report;
    report.failures.reserve(items.size());
    for (std::size_t index = 0; index < items.size(); index++)
    {
        const auto pixels = rgbPixels(items.at(index), paths->at(index));
        if (!pixels)
        {
            report.failures.push_back({
                .textureId = items.at(index).textureId,
                .outputPath = paths->at(index),
                .message = pixels.error().message,
            });
            continue;
        }
        const auto result = options.format == assets::AssetExportFormat::bmp
                                ? image::writeBmp(paths->at(index),
                                                  static_cast<std::uint32_t>(items.at(index).texture.dimension),
                                                  static_cast<std::uint32_t>(items.at(index).texture.dimension),
                                                  *pixels)
                                : image::writePng(paths->at(index),
                                                  static_cast<std::uint32_t>(items.at(index).texture.dimension),
                                                  static_cast<std::uint32_t>(items.at(index).texture.dimension),
                                                  *pixels);
        if (!result)
        {
            report.failures.push_back({
                .textureId = items.at(index).textureId,
                .outputPath = paths->at(index),
                .message = result.error().message,
            });
            continue;
        }
        report.exported++;
    }
    return report;
}

} // namespace uo_mule::gui
