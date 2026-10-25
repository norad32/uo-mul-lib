#pragma once

#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/model/assets/asset_window_contracts.h"
#include <filesystem>
#include <vector>

namespace uo_mule::gui::assets
{

enum class AssetExportFormat : std::uint8_t
{
    bmp,
    png,
};

enum class AssetExportDestination : std::uint8_t
{
    file,
    directory,
};

struct AssetExportRequest
{
    DocumentKind kind = DocumentKind::map;
    AssetSourceContext source;
    std::vector<AssetItemId> items;
    AssetExportFormat format = AssetExportFormat::bmp;
    AssetExportDestination destination = AssetExportDestination::file;
    std::filesystem::path outputPath;
    bool overwrite = false;
};

} // namespace uo_mule::gui::assets
