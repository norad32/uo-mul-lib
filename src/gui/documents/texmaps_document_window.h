#pragma once

#include "gui/workflows/texmap_export_workflow.h"
#include "gui/workflows/texmap_replacement_workflow.h"
#include "gui_model/documents/texmaps_document.h"
#include "gui_model/model/assets/asset_window_contracts.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/providers/data_providers.h"
#include "gui_model/world/map_color.h"
#include "image_codec/image_codec.h"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace uo_mule::gui
{

struct PendingPreferenceChanges;

struct TexmapsTexturePreview
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<MapColor> pixels;
};

struct TexmapsReplacementState
{
    bool visible = false;
    DocumentId documentId = 0;
    std::uint32_t textureId = 0;
    std::filesystem::path sourcePath;
    std::optional<image::RgbImage> image;
    std::optional<uomul::TextureImage> texture;
    std::string status;
};

struct TexmapsFolderReplacementState
{
    bool visible = false;
    DocumentId documentId = 0;
    std::vector<std::uint32_t> textureIds;
    TexmapFolderReplacement replacement;
    std::string status;
};

struct TexmapsExportState
{
    bool formatVisible = false;
    bool overwriteVisible = false;
    bool sourceIsDocument = false;
    std::optional<DocumentId> sourceDocumentId;
    std::vector<std::uint32_t> textureIds;
    assets::AssetExportFormat format = assets::AssetExportFormat::bmp;
    std::filesystem::path outputPath;
    std::string status;
};

struct TexmapsDocumentWindowState
{
    assets::AssetWindowState assetView;
    std::unordered_map<DocumentId, assets::AssetWindowState> documentAssetViews;
    std::unordered_map<std::uint32_t, TexmapsTexturePreview> texturePreviews;
    std::unordered_map<DocumentId, std::unordered_map<std::uint32_t, TexmapsTexturePreview>> documentTexturePreviews;
    std::unordered_map<DocumentId, std::uint64_t> documentTexturePreviewRevisions;
    std::uint64_t texturePreviewRevision = 0;
    bool hideUnused = false;
    std::unordered_map<DocumentId, bool> documentHideUnused;
    TexmapsReplacementState replacement;
    TexmapsFolderReplacementState folderReplacement;
    TexmapsExportState exportState;
    std::function<void(const assets::AssetItemId &)> itemPicked;
    std::function<void(DocumentId, std::uint32_t)> replaceRequested;
    std::function<void(DocumentId, std::vector<std::uint32_t>)> folderReplacementRequested;
    std::function<void()> replacementFileRequested;
    std::function<void(const std::filesystem::path &)> replacementFilePicked;
    std::function<void(const std::filesystem::path &)> folderReplacementFolderPicked;
    std::function<void(std::string, bool)> replacementStatusReported;
    std::function<std::expected<uomul::TextureImage, uomul::Error>(const assets::AssetDragPayloadValue &)> resolveDragTexture;
    std::function<void(std::string)> dragDropErrorReported;
    std::function<void(std::optional<DocumentId>, std::vector<std::uint32_t>, assets::AssetExportFormat)> exportRequested;
    std::function<void(std::optional<DocumentId>, std::vector<std::uint32_t>)> exportFormatRequested;
    std::function<void(const std::filesystem::path &)> exportFilePicked;
    std::function<void(const std::filesystem::path &)> exportFolderPicked;
    std::function<void()> exportOverwriteConfirmed;
    std::function<void()> exportOverwriteCancelled;
    std::function<bool(std::uint32_t)> itemSourcePicked;
    std::function<void(std::string_view)> hoveredContext;
};

void drawTexmapsDocumentWindow(TexmapsDocumentWindowState & state,
                               TexmapsDocument & document,
                               bool & visible,
                               IdFormat idFormat,
                               const Preferences & preferences,
                               PendingPreferenceChanges & pendingChanges,
                               std::string windowIdentity,
                               const std::function<void()> & becameVisible = {});

void loadTexmapsReplacement(TexmapsDocumentWindowState & state, const std::filesystem::path & path);

} // namespace uo_mule::gui
