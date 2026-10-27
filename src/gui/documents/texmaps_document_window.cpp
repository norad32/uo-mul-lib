#include "gui/documents/texmaps_document_window.h"
#include "gui/presentation/asset_browser.h"
#include "gui/presentation/asset_browser_helpers.h"
#include "gui/presentation/dialog_presentation.h"
#include "gui/presentation/iconography.h"
#include "gui/presentation/strings.h"
#include "gui/presentation/texture_preview.h"
#include "gui/presentation/theme.h"
#include "gui/workflows/texmap_replacement_workflow.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/documents/texmaps_document.h"
#include "gui_model/model/assets/asset_export_contracts.h"
#include "gui_model/model/assets/asset_window_contracts.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/providers/color_conversion.h"
#include "gui_model/providers/data_providers.h"
#include "image_codec/image_codec.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/texmaps/texmaps.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <imgui.h>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace uo_mule::gui
{
namespace
{

constexpr float thumbnailSize = 64.0F;
constexpr float replacementPreviewSize = 180.0F;
constexpr float folderReplacementRowsHeight = 260.0F;
constexpr int folderReplacementColumnCount = 5;
constexpr float borderThickness = 2.0F;
constexpr std::size_t assetCellWidth = 72U;
constexpr std::size_t assetCellHeight = 72U;

[[nodiscard]] const TexmapsTexturePreview & texturePreview(std::unordered_map<std::uint32_t, TexmapsTexturePreview> & previews,
                                                           const TextureProvider & provider,
                                                           std::uint32_t id)
{
    const auto [iterator, inserted] = previews.try_emplace(id);
    if (inserted)
    {
        const auto texture = provider.texture(id);
        if (texture)
        {
            iterator->second.width = texture->width;
            iterator->second.height = texture->height;
            iterator->second.pixels.reserve(texture->pixels.size());
            for (const auto pixel : texture->pixels)
            {
                iterator->second.pixels.push_back(pixel);
            }
        }
    }
    return iterator->second;
}

void drawTextureAsset(std::unordered_map<std::uint32_t, TexmapsTexturePreview> & previews,
                      const TextureProvider & provider,
                      const assets::AssetItemId & item,
                      float size)
{
    const auto id = assets::itemIndex(item);
    if (!id)
    {
        return;
    }
    const auto & preview = texturePreview(previews, provider, *id);
    drawTexturePreview({.width = preview.width, .height = preview.height, .pixels = preview.pixels}, size, borderThickness);
}

[[nodiscard]] std::vector<assets::AssetItem>
loadTextureRange(const TextureProvider & provider, const assets::AssetRangeRequest & request, IdFormat idFormat)
{
    std::vector<assets::AssetItem> items;
    const std::size_t end = std::min(request.first + request.count, static_cast<std::size_t>(provider.textureCount()));
    for (std::size_t id = request.first; id < end; id++)
    {
        const auto present = provider.hasTexture(static_cast<std::uint32_t>(id));
        items.push_back({
            .id = {.value = std::to_string(id)},
            .label = strings::formatId(static_cast<std::uint32_t>(id), idFormat),
            .state = present && *present ? assets::AssetItemState::available : assets::AssetItemState::empty,
            .stateMessage = {},
        });
    }
    return items;
}

[[nodiscard]] std::vector<assets::AssetSearchItem>
loadTextureSearch(bool hideEmpty, const TextureProvider & provider, IdFormat idFormat)
{
    std::vector<assets::AssetSearchItem> items;
    for (std::size_t id = 0; id < static_cast<std::size_t>(provider.textureCount()); id++)
    {
        const auto present = provider.hasTexture(static_cast<std::uint32_t>(id));
        if (hideEmpty && (!present || !*present))
        {
            continue;
        }
        const auto label = strings::formatId(static_cast<std::uint32_t>(id), idFormat);
        items.push_back({
            .item =
                {
                    .id = {.value = std::to_string(id)},
                    .label = label,
                    .state = present && *present ? assets::AssetItemState::available : assets::AssetItemState::empty,
                    .stateMessage = {},
                },
            .values = {"ID: " + label},
        });
    }
    return items;
}

[[nodiscard]] std::optional<assets::AssetDetails>
loadTextureDetails(const TextureProvider & provider, const assets::AssetItemId & item, IdFormat idFormat)
{
    const auto id = assets::itemIndex(item);
    if (!id)
    {
        return std::nullopt;
    }
    const auto texture = provider.texture(*id);
    if (!texture)
    {
        return std::nullopt;
    }
    return assets::AssetDetails{
        .item = item,
        .title = "ID: " + strings::formatId(*id, idFormat),
        .fields =
            {
                std::string(strings::dimensions()) + ": " + std::to_string(texture->width) + "x" +
                    std::to_string(texture->height),
            },
        .fieldTooltips = {},
    };
}

[[nodiscard]] TexmapsTexturePreview previewFromRgb(const image::RgbImage & image)
{
    TexmapsTexturePreview preview{
        .width = image.width,
        .height = image.height,
        .pixels = {},
    };
    preview.pixels.reserve(image.pixels.size() / 3U);
    for (std::size_t offset = 0; offset < image.pixels.size(); offset += 3U)
    {
        preview.pixels.push_back({
            .red = image.pixels.at(offset),
            .green = image.pixels.at(offset + 1U),
            .blue = image.pixels.at(offset + 2U),
        });
    }
    return preview;
}

[[nodiscard]] TexmapsTexturePreview previewFromTexture(const uomul::TextureImage & texture)
{
    TexmapsTexturePreview preview{
        .width = texture.dimension,
        .height = texture.dimension,
        .pixels = {},
    };
    preview.pixels.reserve(texture.pixels.size());
    for (const auto pixel : texture.pixels)
    {
        preview.pixels.push_back(texturePixelToMapColor(pixel));
    }
    return preview;
}

void drawReplacementDialog(TexmapsDocumentWindowState & state, TexmapsDocument & document)
{
    auto & replacement = state.replacement;
    if (!replacement.visible || replacement.documentId != document.id())
    {
        return;
    }

    bool visible = replacement.visible;
    const std::string popupId = "Replace Texture###document.texmaps.replace." + std::to_string(document.id());
    if (!beginAutoSizeDialog(popupId.c_str(), &visible))
    {
        replacement.visible = visible;
        return;
    }

    constexpr std::size_t replacementCount = 1U;
    const std::string replacingLabel =
        "Replacing " + std::to_string(replacementCount) + " texture" + (replacementCount == 1U ? "" : "s");
    ImGui::TextUnformatted(replacingLabel.c_str());
    ImGui::TextColored(theme::imguiColor(theme::mutedText), "%s", "Supported texture sizes: 64x64, 128x128.");
    ImGui::TextColored(theme::imguiColor(theme::mutedText), "%s", "Alpha is ignored, colors are converted to RGB555.");

    if (ImGui::BeginTable("ReplacementPreview", 2, ImGuiTableFlags_BordersInnerV))
    {
        const auto current = document.effectiveTexture(replacement.textureId);
        ImGui::TableNextColumn();
        ImGui::TextColored(theme::imguiColor(theme::heading), "%s", "Current");
        if (current && current->has_value())
        {
            const auto preview = previewFromTexture(**current);
            drawTexturePreview({.width = preview.width, .height = preview.height, .pixels = preview.pixels},
                               replacementPreviewSize);
            ImGui::TextColored(theme::imguiColor(theme::mutedText), "%ux%u", preview.width, preview.height);
        }
        else
        {
            ImGui::TextUnformatted("No texture");
        }

        ImGui::TableNextColumn();
        ImGui::TextColored(theme::imguiColor(theme::heading), "%s", "Replacement");
        if (replacement.image)
        {
            const auto preview = previewFromRgb(*replacement.image);
            drawTexturePreview({.width = preview.width, .height = preview.height, .pixels = preview.pixels},
                               replacementPreviewSize);
            ImGui::TextColored(theme::imguiColor(theme::mutedText), "%ux%u", preview.width, preview.height);
        }
        else
        {
            const TexturePreviewData emptyPreview{};
            drawTexturePreview(emptyPreview, replacementPreviewSize);
            ImGui::TextUnformatted("No image loaded");
        }

        const std::string openLabel = iconLabel(IconId::folderOpen, "Open");
        const float openWidth = ImGui::CalcTextSize(openLabel.c_str()).x + (ImGui::GetStyle().FramePadding.x * 2.0F);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0F, ImGui::GetContentRegionAvail().x - openWidth));
        if (state.replacementFileRequested && ImGui::Button(openLabel.c_str()))
        {
            state.replacementFileRequested();
            visible = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndTable();
    }
    if (!replacement.status.empty())
    {
        ImGui::TextWrapped("%s", replacement.status.c_str());
    }

    const std::string cancelLabel = iconLabel(IconId::cancel, "Cancel");
    const std::string applyLabel = iconLabel(IconId::check, "Apply");
    beginDialogFooter({cancelLabel.c_str(), applyLabel.c_str()});
    if (dialogButton(cancelLabel.c_str()))
    {
        visible = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    const auto texture = replacement.texture;
    if (!texture)
    {
        static_cast<void>(dialogButton(applyLabel.c_str(), false));
    }
    else if (dialogButton(applyLabel.c_str()))
    {
        TexmapsDocumentIntent intent;
        intent.kind = TexmapsDocumentIntentKind::replace;
        intent.textureId = replacement.textureId;
        intent.texture = texture.value();
        document.requestIntent(std::move(intent));
        visible = false;
        ImGui::CloseCurrentPopup();
    }
    endDialogFooter();
    endDialog();
    replacement.visible = visible;
}

[[nodiscard]] const char * folderRowStateLabel(const TexmapReplacementRow & row) noexcept
{
    if (row.state == TexmapReplacementRowState::valid)
    {
        return row.noOp ? "No change" : "Valid";
    }
    if (row.state == TexmapReplacementRowState::ambiguous)
    {
        return "Ambiguous";
    }
    if (row.state == TexmapReplacementRowState::invalid)
    {
        return "Invalid";
    }
    return "Missing";
}

void drawFolderReplacementRows(const TexmapFolderReplacement & replacement, IdFormat idFormat)
{
    const auto tableFlags = static_cast<ImGuiTableFlags>(static_cast<unsigned int>(ImGuiTableFlags_Borders) |
                                                         static_cast<unsigned int>(ImGuiTableFlags_RowBg) |
                                                         static_cast<unsigned int>(ImGuiTableFlags_SizingStretchProp));
    if (!ImGui::BeginChild("FolderReplacementRows", ImVec2(0.0F, folderReplacementRowsHeight), ImGuiChildFlags_Borders))
    {
        return;
    }
    if (ImGui::BeginTable("FolderReplacementOverview", folderReplacementColumnCount, tableFlags))
    {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Filename");
        ImGui::TableSetupColumn("Current");
        ImGui::TableSetupColumn("Replacement");
        ImGui::TableSetupColumn("Status");
        ImGui::TableHeadersRow();
        for (const auto & row : replacement.rows)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const auto id = strings::formatId(row.textureId, idFormat);
            ImGui::TextUnformatted(id.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(row.filename.empty() ? "-" : row.filename.c_str());
            ImGui::TableNextColumn();
            if (row.currentDimension == 0U)
            {
                ImGui::TextUnformatted("-");
            }
            else
            {
                ImGui::TextColored(theme::imguiColor(theme::mutedText), "%ux%u", row.currentDimension, row.currentDimension);
            }
            ImGui::TableNextColumn();
            if (row.replacementWidth == 0U || row.replacementHeight == 0U)
            {
                ImGui::TextUnformatted("-");
            }
            else
            {
                ImGui::TextColored(theme::imguiColor(theme::mutedText), "%ux%u", row.replacementWidth, row.replacementHeight);
            }
            ImGui::TableNextColumn();
            const auto stateColor = row.state == TexmapReplacementRowState::valid ? theme::success : theme::error;
            ImGui::TextColored(theme::imguiColor(stateColor), "%s", folderRowStateLabel(row));
            if (!row.message.empty())
            {
                ImGui::SetItemTooltip("%s", row.message.c_str());
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
}

void applyFolderReplacement(TexmapsDocumentWindowState & state, TexmapsDocument & document)
{
    auto & folderReplacement = state.folderReplacement;
    std::vector<TexmapsTextureEdit> edits;
    std::size_t validCount = 0;
    for (const auto & row : folderReplacement.replacement.rows)
    {
        if (!texmapReplacementRowValid(row) || !row.texture)
        {
            continue;
        }
        validCount++;
        edits.push_back({.textureId = row.textureId, .texture = row.texture.value()});
    }
    const std::size_t skippedCount = folderReplacement.replacement.rows.size() - validCount;
    if (!edits.empty())
    {
        TexmapsDocumentIntent intent;
        intent.kind = TexmapsDocumentIntentKind::replace;
        intent.textureEdits = std::move(edits);
        document.requestIntent(std::move(intent));
    }
    const std::string status = "Replaced " + std::to_string(validCount) + " texture" + (validCount == 1U ? "" : "s") +
                               ", skipped " + std::to_string(skippedCount) + ".";
    folderReplacement.status = status;
    if (state.replacementStatusReported)
    {
        state.replacementStatusReported(status, true);
    }
}

void drawFolderReplacementDialog(TexmapsDocumentWindowState & state, TexmapsDocument & document, IdFormat idFormat)
{
    auto & folderReplacement = state.folderReplacement;
    if (!folderReplacement.visible || folderReplacement.documentId != document.id())
    {
        return;
    }

    bool visible = folderReplacement.visible;
    const std::string popupId = "Replace from Folder###document.texmaps.folder-replace." + std::to_string(document.id());
    if (!beginAutoSizeDialog(popupId.c_str(), &visible))
    {
        folderReplacement.visible = visible;
        return;
    }

    const std::size_t replacementCount = folderReplacement.textureIds.size();
    const std::string replacingLabel =
        "Replacing " + std::to_string(replacementCount) + " texture" + (replacementCount == 1U ? "" : "s");
    ImGui::TextUnformatted(replacingLabel.c_str());
    ImGui::TextColored(theme::imguiColor(theme::mutedText), "%s", "BMP is preferred; PNG is used as a fallback.");
    if (folderReplacement.replacement.folder.empty())
    {
        ImGui::TextUnformatted("Choose a folder to scan for replacement images.");
    }
    else
    {
        ImGui::Text(strings::folderFormat(), folderReplacement.replacement.folder.string().c_str());
        drawFolderReplacementRows(folderReplacement.replacement, idFormat);
    }
    if (!folderReplacement.status.empty())
    {
        ImGui::TextWrapped("%s", folderReplacement.status.c_str());
    }

    const std::string cancelLabel = iconLabel(IconId::cancel, "Cancel");
    const std::string applyLabel = iconLabel(IconId::check, "Apply");
    beginDialogFooter({cancelLabel.c_str(), applyLabel.c_str()});
    if (dialogButton(cancelLabel.c_str()))
    {
        visible = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (dialogButton(applyLabel.c_str()))
    {
        applyFolderReplacement(state, document);
        visible = false;
        ImGui::CloseCurrentPopup();
    }
    endDialogFooter();
    endDialog();
    folderReplacement.visible = visible;
}

[[nodiscard]] std::vector<std::uint32_t> exportSelection(const assets::AssetSelection & selection,
                                                         const assets::AssetItemId & contextItem)
{
    std::vector<std::uint32_t> ids;
    for (const auto & item : selection.selected)
    {
        if (const auto id = assets::itemIndex(item))
        {
            ids.push_back(*id);
        }
    }
    if (ids.empty())
    {
        if (const auto id = assets::itemIndex(contextItem))
        {
            ids.push_back(*id);
        }
    }
    return ids;
}

void requestTexmapExport(TexmapsDocumentWindowState & state, assets::AssetExportFormat format)
{
    if (state.exportRequested)
    {
        state.exportRequested(state.exportState.sourceDocumentId, state.exportState.textureIds, format);
    }
    state.exportState.formatVisible = false;
    ImGui::CloseCurrentPopup();
}

void drawTexmapExportFormatDialog(TexmapsDocumentWindowState & state)
{
    if (!state.exportState.formatVisible)
    {
        return;
    }
    bool visible = state.exportState.formatVisible;
    if (!beginAutoSizeDialog("Export Texmaps###document.texmaps.export-format", &visible))
    {
        state.exportState.formatVisible = visible;
        return;
    }
    ImGui::TextUnformatted("Choose an export format.");
    const std::string bmpLabel = iconLabel(IconId::exportIcon, "Export as BMP");
    const std::string pngLabel = iconLabel(IconId::exportIcon, "Export as PNG");
    if (dialogButton(bmpLabel.c_str()))
    {
        requestTexmapExport(state, assets::AssetExportFormat::bmp);
    }
    ImGui::SameLine();
    if (dialogButton(pngLabel.c_str()))
    {
        requestTexmapExport(state, assets::AssetExportFormat::png);
    }
    ImGui::SameLine();
    if (dialogButton("Cancel"))
    {
        state.exportState.formatVisible = false;
        ImGui::CloseCurrentPopup();
    }
    endDialog();
}

void drawTexmapOverwriteDialog(TexmapsDocumentWindowState & state)
{
    if (!state.exportState.overwriteVisible)
    {
        return;
    }
    bool visible = state.exportState.overwriteVisible;
    if (!beginAutoSizeDialog("Overwrite Texmap Exports###document.texmaps.export-overwrite", &visible))
    {
        state.exportState.overwriteVisible = visible;
        return;
    }
    ImGui::TextWrapped("One or more export files already exist. Overwrite them?");
    if (dialogButton("Overwrite"))
    {
        if (state.exportOverwriteConfirmed)
        {
            state.exportOverwriteConfirmed();
        }
        state.exportState.overwriteVisible = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (dialogButton("Cancel"))
    {
        if (state.exportOverwriteCancelled)
        {
            state.exportOverwriteCancelled();
        }
        state.exportState.overwriteVisible = false;
        ImGui::CloseCurrentPopup();
    }
    endDialog();
}

void drawTexmapExportDialogs(TexmapsDocumentWindowState & state)
{
    drawTexmapExportFormatDialog(state);
    drawTexmapOverwriteDialog(state);
}

void drawHideEmptyControl(bool & hideEmpty)
{
    const std::string eyeLabel = iconFontAvailable() ? iconGlyph(IconId::eye) : strings::hideEmpty();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(eyeLabel.c_str());
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", strings::hideEmpty());
    }
    ImGui::SameLine();
    ImGui::Checkbox("##TexmapsHideEmpty", &hideEmpty);
}

void configureTextureAssetAdapter(assets::AssetAdapter & adapter,
                                  std::unordered_map<std::uint32_t, TexmapsTexturePreview> & previews,
                                  const TextureProvider & provider,
                                  bool & hideEmpty,
                                  IdFormat idFormat)
{
    adapter.descriptor.queryWhenEmpty = hideEmpty;
    adapter.descriptor.formatSelectionId = [idFormat](const assets::AssetItemId & item)
    { return formatAssetItemId(item, idFormat); };
    adapter.descriptor.renderSelectionPreview = [&previews, &provider](const assets::AssetItemId & item, float size)
    { drawTextureAsset(previews, provider, item, size); };
    adapter.descriptor.selectionPreviewSize = thumbnailSize;
    adapter.descriptor.capabilities = {
        .presentations = static_cast<std::uint8_t>(assets::AssetPresentationCapability::grid) |
                         static_cast<std::uint8_t>(assets::AssetPresentationCapability::table),
        .queries = static_cast<std::uint8_t>(assets::AssetQueryCapability::filter),
        .selection = assets::AssetSelectionMode::multiple,
    };
    adapter.descriptor.itemCount = provider.textureCount();
    adapter.descriptor.fixedCellWidth = assetCellWidth;
    adapter.descriptor.fixedCellHeight = assetCellHeight;
    adapter.loadRange = [&provider, idFormat](const assets::AssetRangeRequest & request, const assets::AssetQuery &)
    { return loadTextureRange(provider, request, idFormat); };
    adapter.loadSearchItems = [&provider, &hideEmpty, idFormat] { return loadTextureSearch(hideEmpty, provider, idFormat); };
    adapter.loadDetails = [&provider, idFormat](const assets::AssetItemId & item)
    { return loadTextureDetails(provider, item, idFormat); };
    adapter.renderItem = [&previews, &provider](const assets::AssetItem & item, bool)
    { drawTextureAsset(previews, provider, item.id, thumbnailSize); };
}

class DocumentTextureProvider final : public TextureProvider
{
public:
    explicit DocumentTextureProvider(const TexmapsDocument & document) : document_(&document) {}

    [[nodiscard]] ProviderStatus status() const override
    {
        return {
            .state = document_->reader() ? ProviderAssetState::available : ProviderAssetState::invalid,
            .source = ProviderSourceKind::externalConfigured,
            .message = document_->error(),
            .sourceIdentity = document_->sourceIdentity(),
            .revision = document_->displayRevision(),
        };
    }

    [[nodiscard]] ProviderStatus status(std::uint32_t id) const override
    {
        auto result = status();
        const auto present = document_->hasTexture(id);
        if (!present)
        {
            result.state = ProviderAssetState::invalid;
            result.message = present.error().message;
        }
        else if (!*present)
        {
            result.state = ProviderAssetState::empty;
            result.message = "No texture entry";
        }
        return result;
    }

    [[nodiscard]] std::optional<ProviderTexture> texture(std::uint32_t id) const override
    {
        const auto result = document_->effectiveTexture(id);
        if (!result || !result->has_value())
        {
            return std::nullopt;
        }
        ProviderTexture texture{
            .width = result->value().dimension,
            .height = result->value().dimension,
            .pixels = {},
        };
        texture.pixels.reserve(result->value().pixels.size());
        for (const auto pixel : result->value().pixels)
        {
            texture.pixels.push_back(texturePixelToMapColor(pixel));
        }
        return texture;
    }

    [[nodiscard]] std::uint32_t textureCount() const override
    {
        return document_->reader() ? document_->reader()->textureCount() : 0U;
    }

    [[nodiscard]] std::optional<bool> hasTexture(std::uint32_t id) const override
    {
        const auto result = document_->hasTexture(id);
        return result ? std::optional<bool>(*result) : std::nullopt;
    }

private:
    const TexmapsDocument * document_ = nullptr;
};

void refreshDocumentTexturePreviews(TexmapsDocumentWindowState & state, DocumentId documentId, std::uint64_t revision)
{
    if (state.documentTexturePreviewRevisions[documentId] == revision)
    {
        return;
    }
    state.documentTexturePreviews[documentId].clear();
    state.documentTexturePreviewRevisions[documentId] = revision;
}

void requestDocumentReplacement(const TexmapsDocumentWindowState & state,
                                const TexmapsDocument & document,
                                const assets::AssetItemId & item)
{
    const auto id = assets::itemIndex(item);
    if (!id || !state.replaceRequested)
    {
        return;
    }
    const auto present = document.hasTexture(*id);
    if (present && *present)
    {
        state.replaceRequested(document.id(), *id);
    }
}

[[nodiscard]] std::vector<std::uint32_t> documentReplacementSelection(const TexmapsDocumentWindowState & state,
                                                                      DocumentId documentId,
                                                                      const assets::AssetItemId & contextItem)
{
    std::vector<std::uint32_t> ids;
    for (const auto & selected : state.documentAssetViews.at(documentId).selection.selected)
    {
        if (const auto id = assets::itemIndex(selected))
        {
            ids.push_back(*id);
        }
    }
    if (ids.empty())
    {
        if (const auto id = assets::itemIndex(contextItem))
        {
            ids.push_back(*id);
        }
    }
    return ids;
}

void requestDocumentFolderReplacement(const TexmapsDocumentWindowState & state,
                                      const TexmapsDocument & document,
                                      const assets::AssetItemId & item)
{
    if (!state.folderReplacementRequested)
    {
        return;
    }
    auto ids = documentReplacementSelection(state, document.id(), item);
    if (!ids.empty())
    {
        state.folderReplacementRequested(document.id(), std::move(ids));
    }
}

void drawDocumentReplacementContextMenu(const TexmapsDocumentWindowState & state,
                                        const TexmapsDocument & document,
                                        const assets::AssetItemId & item)
{
    if (ImGui::MenuItem("Replace..."))
    {
        requestDocumentReplacement(state, document, item);
    }
    if (ImGui::MenuItem("Replace from Folder..."))
    {
        requestDocumentFolderReplacement(state, document, item);
    }
}

void drawDocumentExportContextMenu(const TexmapsDocumentWindowState & state,
                                   const TexmapsDocument & document,
                                   const assets::AssetItemId & item)
{
    const auto ids = exportSelection(state.documentAssetViews.at(document.id()).selection, item);
    if (ImGui::MenuItem("Export as BMP", nullptr, false, !ids.empty() && state.exportRequested))
    {
        state.exportRequested(document.id(), ids, assets::AssetExportFormat::bmp);
    }
    if (ImGui::MenuItem("Export as PNG", nullptr, false, !ids.empty() && state.exportRequested))
    {
        state.exportRequested(document.id(), ids, assets::AssetExportFormat::png);
    }
}

[[nodiscard]] std::optional<assets::AssetDragPayload> makeDocumentDragPayload(const TexmapsDocument & document,
                                                                              const assets::AssetSelection & selection)
{
    if (selection.selected.size() != 1U)
    {
        return std::nullopt;
    }
    const auto id = assets::itemIndex(selection.selected.front());
    if (!id)
    {
        return std::nullopt;
    }
    const auto texture = document.effectiveTexture(*id);
    if (!texture || !texture->has_value())
    {
        return std::nullopt;
    }
    return makeTexmapDragPayload(selection, true, assets::AssetDragPayloadSource::document, document.id());
}

void applyDocumentTextureDrop(const TexmapsDocumentWindowState & state,
                              TexmapsDocument & document,
                              const assets::AssetItemId & item,
                              const assets::AssetDragPayloadValue & payload)
{
    const auto targetId = assets::itemIndex(item);
    if (!targetId || !state.resolveDragTexture)
    {
        return;
    }
    const auto texture = state.resolveDragTexture(payload);
    if (!texture)
    {
        if (state.dragDropErrorReported)
        {
            state.dragDropErrorReported(texture.error().message);
        }
        return;
    }
    TexmapsDocumentIntent intent;
    intent.kind = TexmapsDocumentIntentKind::replace;
    intent.textureId = *targetId;
    intent.texture = *texture;
    document.requestIntent(std::move(intent));
}

void configureDocumentTextureDrop(assets::AssetAdapter & adapter,
                                  TexmapsDocumentWindowState & state,
                                  TexmapsDocument & document)
{
    adapter.descriptor.acceptedDragPayloadType = std::string(assets::texmapValueDragPayloadType);
    adapter.descriptor.itemValueDropped =
        [&state, &document](const assets::AssetItemId & item, const assets::AssetDragPayloadValue & payload)
    { applyDocumentTextureDrop(state, document, item, payload); };
}

[[nodiscard]] assets::AssetAdapter makeDocumentAdapter(TexmapsDocumentWindowState & state,
                                                       TexmapsDocument & document,
                                                       const TextureProvider & provider,
                                                       IdFormat idFormat,
                                                       assets::AssetWindowState & assetView,
                                                       std::string windowIdentity)
{
    bool & hideEmpty = assetView.hideEmpty;
    const auto status = provider.status();
    auto & previews = state.documentTexturePreviews[document.id()];
    const auto revision = document.displayRevision();
    refreshDocumentTexturePreviews(state, document.id(), revision);

    assets::AssetAdapter adapter;
    configureAssetAdapter(adapter.descriptor,
                          {
                              .viewId = "document.texmaps." + std::to_string(document.id()),
                              .windowIdentity = std::move(windowIdentity),
                              .title = "Texmaps",
                              .sourceTooltip = "Source: " + document.sourceIndexPath().string(),
                              .cacheContext = std::to_string(static_cast<int>(idFormat)) +
                                              "|hide=" + std::to_string(static_cast<int>(hideEmpty)) +
                                              "|revision=" + std::to_string(revision),
                              .source =
                                  {
                                      .kind = DocumentKind::texmaps,
                                      .contextKind = assets::AssetSourceContext::Kind::document,
                                      .source = document.sourceIdentity(),
                                      .contextId = "document.texmaps." + std::to_string(document.id()),
                                  },
                              .itemPicked = state.itemPicked,
                              .hoveredContext = state.hoveredContext,
                          });
    configureTextureAssetAdapter(adapter, previews, provider, hideEmpty, idFormat);
    adapter.descriptor.inlinePresentationControl = [&hideEmpty] { drawHideEmptyControl(hideEmpty); };
    adapter.descriptor.itemStaged = [&document](const assets::AssetItemId & item)
    {
        return assets::itemIndex(item)
            .transform([&document](std::uint32_t id) { return document.hasStagedTexture(id); })
            .value_or(false);
    };
    adapter.descriptor.itemRevert = [&document](const assets::AssetSelection & selection)
    {
        std::vector<std::uint32_t> textureIds;
        textureIds.reserve(selection.selected.size());
        for (const auto & selectedItem : selection.selected)
        {
            if (const auto id = assets::itemIndex(selectedItem))
            {
                textureIds.push_back(*id);
            }
        }
        TexmapsDocumentIntent intent;
        intent.kind = TexmapsDocumentIntentKind::revert;
        intent.textureIds = std::move(textureIds);
        document.requestIntent(std::move(intent));
    };
    adapter.descriptor.itemDoubleClicked = [&state, &document](const assets::AssetItemId & item)
    { requestDocumentReplacement(state, document, item); };
    adapter.descriptor.itemContextMenu = [&state, &document](const assets::AssetItemId & item)
    {
        drawDocumentReplacementContextMenu(state, document, item);
        drawDocumentExportContextMenu(state, document, item);
    };
    if (!status.message.empty())
    {
        adapter.descriptor.sourceTooltip += "\nStatus: " + status.message;
    }
    adapter.makeDragPayload = [&document](const assets::AssetSelection & selection)
    { return makeDocumentDragPayload(document, selection); };
    configureDocumentTextureDrop(adapter, state, document);
    return adapter;
}

} // namespace

void drawTexmapsDocumentWindow(TexmapsDocumentWindowState & state,
                               TexmapsDocument & document,
                               bool & visible,
                               IdFormat idFormat,
                               std::string windowIdentity,
                               const std::function<void()> & becameVisible)
{
    auto & assetView = state.documentAssetViews[document.id()];
    if (!document.reader())
    {
        if (ImGui::Begin(windowIdentity.c_str(), &visible))
        {
            ImGui::TextUnformatted(document.error().c_str());
        }
        ImGui::End();
        return;
    }

    const DocumentTextureProvider provider(document);
    const auto adapter = makeDocumentAdapter(state, document, provider, idFormat, assetView, std::move(windowIdentity));
    assets::drawAssetBrowser(adapter, assetView, visible, becameVisible);
    drawReplacementDialog(state, document);
    drawFolderReplacementDialog(state, document, idFormat);
    if (state.exportState.sourceIsDocument && state.exportState.sourceDocumentId == document.id())
    {
        drawTexmapExportDialogs(state);
    }
}

void loadTexmapsReplacement(TexmapsDocumentWindowState & state, const std::filesystem::path & path)
{
    state.replacement.sourcePath = path;
    state.replacement.image.reset();
    state.replacement.texture.reset();

    auto decoded = image::decode(path);
    if (!decoded)
    {
        state.replacement.status = decoded.error().message;
        return;
    }
    state.replacement.image = *decoded;
    auto texture = image::toTexture(*decoded, path);
    if (!texture)
    {
        state.replacement.status = texture.error().message;
        return;
    }
    state.replacement.texture = std::move(*texture);
    state.replacement.status = "Loaded " + path.filename().string();
}

} // namespace uo_mule::gui
