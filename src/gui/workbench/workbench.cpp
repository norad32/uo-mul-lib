#include "gui/workbench/workbench.h"
#include "gui/application/layout_store.h"
#include "gui/application/welcome_view.h"
#include "gui/commands/command_registry.h"
#include "gui/commands/menu_bar.h"
#include "gui/dialogs/document_close_dialog.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/dialogs/map_workflow_dialog.h"
#include "gui/dialogs/preferences_dialog.h"
#include "gui/documents/document_window_name.h"
#include "gui/documents/map_document_window.h"
#include "gui/documents/radarcol_document_window.h"
#include "gui/documents/texmaps_document_window.h"
#include "gui/documents/tiledata_document_window.h"
#include "gui/panels/block_inspector_panel.h"
#include "gui/panels/icon_gallery_panel.h"
#include "gui/panels/log_panel.h"
#include "gui/panels/tile_inspector_panel.h"
#include "gui/panels/tool_palette_panel.h"
#include "gui/presentation/asset_browser_helpers.h"
#include "gui/presentation/dialog_presentation.h"
#include "gui/presentation/iconography.h"
#include "gui/presentation/strings.h"
#include "gui/presentation/theme.h"
#include "gui/workbench/notification_state.h"
#include "gui/workbench/workbench_state.h"
#include "gui/workflows/texmap_export_workflow.h"
#include "gui/workflows/texmap_replacement_workflow.h"
#include "gui_model/dialogs/dialog_contracts.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/documents/map_document.h"
#include "gui_model/documents/map_document_state.h"
#include "gui_model/documents/radarcol_document.h"
#include "gui_model/documents/texmaps_document.h"
#include "gui_model/layout/panel_registry.h"
#include "gui_model/layout/window_registry.h"
#include "gui_model/lifecycle/document_lifecycle.h"
#include "gui_model/model/assets/asset_export_contracts.h"
#include "gui_model/model/assets/asset_window_contracts.h"
#include "gui_model/operations/operation_controller.h"
#include "gui_model/persistence/asset_state_store.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/providers/data_providers.h"
#include "gui_model/providers/texture_conversion.h"
#include "gui_model/session/workbench_session.h"
#include "gui_model/source/source_identity.h"
#include "gui_model/tools/tool_contracts.h"
#include "gui_model/world/map_canvas_state.h"
#include "gui_model/world/map_context.h"
#include "logger/gui_log_sink.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <expected>
#include <filesystem>
#include <functional>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <uomul/map/map.h>
#include <uomul/mul_common/mul_common.h>
#include <uomul/radarcol/radarcol.h>
#include <uomul/texmaps/texmaps.h>
#include <utility>
#include <vector>

namespace uo_mule::gui
{
namespace
{

// Defaults mirror the user's persisted workbench arrangement: inspectors on
// the left, documents in the center, and asset palettes stacked on the right.
constexpr float dockLeftWidthRatio = 0.306F;
constexpr float dockRightWidthRatio = 0.447F;
constexpr float dockLeftBottomRatio = 0.5F;
constexpr float toolDockSplitRatio = 0.38F;
constexpr float statusBarHeight = 36.0F;
constexpr std::size_t shortcutCount = 10;
constexpr std::size_t graveShortcutIndex = 0;
constexpr std::size_t saveAsShortcutIndex = 1;
constexpr std::size_t saveShortcutIndex = 2;
constexpr std::size_t openShortcutIndex = 3;
constexpr std::size_t newShortcutIndex = 4;
constexpr std::size_t quitShortcutIndex = 5;
constexpr std::size_t goToShortcutIndex = 6;

void ensureConfiguredMetadataViewDocument(WorkbenchState & state, DocumentKind kind);
void ensureConfiguredMetadataWindows(WorkbenchState & state);
void focusConfiguredMetadataWindow(WorkbenchState & state, DocumentKind kind);
[[nodiscard]] bool metadataViewVisible(const WorkbenchState & state, DocumentKind kind) noexcept;
void restoreConfiguredMetadataDocumentAssetState(const WorkbenchState & state,
                                                 const DocumentRecord & document,
                                                 assets::AssetWindowState & assetView);
constexpr std::size_t fullscreenShortcutIndex = 7;
constexpr std::size_t assetEditShortcutIndex = 8;
constexpr std::size_t assetCopyShortcutIndex = 9;

struct ToolShortcut
{
    ImGuiKey key;
    std::string_view shortcut;
    bool shift = false;
};

constexpr std::array<ToolShortcut, 6> toolShortcuts{
    ToolShortcut{.key = ImGuiKey_P, .shortcut = "P"},
    ToolShortcut{.key = ImGuiKey_R, .shortcut = "R"},
    ToolShortcut{.key = ImGuiKey_F, .shortcut = "F"},
    ToolShortcut{.key = ImGuiKey_U, .shortcut = "U"},
    ToolShortcut{.key = ImGuiKey_B, .shortcut = "Shift+B", .shift = true},
    ToolShortcut{.key = ImGuiKey_R, .shortcut = "Shift+R", .shift = true},
};

void drawDockSpace(WorkbenchState & state, float bottomPanelHeight)
{
    const ImGuiViewport * viewport = ImGui::GetMainViewport();
    const float dockSpaceHeight = std::max(0.0F, viewport->WorkSize.y - bottomPanelHeight - statusBarHeight);
    ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, dockSpaceHeight), ImGuiCond_Always);
    constexpr auto dockSpaceWindowFlags = static_cast<ImGuiWindowFlags>(
        static_cast<unsigned int>(ImGuiWindowFlags_NoDocking) | static_cast<unsigned int>(ImGuiWindowFlags_NoTitleBar) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoCollapse) | static_cast<unsigned int>(ImGuiWindowFlags_NoResize) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoMove) | static_cast<unsigned int>(ImGuiWindowFlags_NoBringToFrontOnFocus) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoNavFocus) | static_cast<unsigned int>(ImGuiWindowFlags_NoBackground) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoSavedSettings) | static_cast<unsigned int>(ImGuiWindowFlags_NoScrollbar) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoScrollWithMouse));
    ImGui::Begin("BackgroundDockSpace###workbench.dock-space", nullptr, dockSpaceWindowFlags);
    const ImVec2 dockPosition(viewport->WorkPos.x, viewport->WorkPos.y);
    const float availableDockHeight =
        std::max(0.0F, viewport->WorkSize.y - bottomPanelHeight - statusBarHeight - (dockPosition.y - viewport->WorkPos.y));
    constexpr ImGuiDockNodeFlags dockSpaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;
    const ImGuiID dockSpaceId = ImGui::GetID("BackgroundDockSpace");
    const ImGuiDockNode * existingDockNode = ImGui::DockBuilderGetNode(dockSpaceId);
    if (state.layoutResetRequested || existingDockNode == nullptr)
    {
        if (state.layoutResetRequested)
        {
            ImGui::ClearIniSettings();
        }
        state.layoutResetRequested = false;
        ImGui::DockBuilderRemoveNode(dockSpaceId);
        ImGui::DockBuilderAddNode(dockSpaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodePos(dockSpaceId, dockPosition);
        ImGui::DockBuilderSetNodeSize(dockSpaceId, ImVec2(viewport->WorkSize.x, availableDockHeight));
        ImGuiID leftNode = 0;
        ImGuiID centerNode = 0;
        ImGuiID rightNode = 0;
        ImGuiID leftBottomNode = 0;
        ImGuiID leftTopNode = 0;
        ImGuiID toolNode = 0;
        ImGui::DockBuilderSplitNode(dockSpaceId, ImGuiDir_Left, dockLeftWidthRatio, &leftNode, &centerNode);
        ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Right, dockRightWidthRatio, &rightNode, &centerNode);
        ImGui::DockBuilderSplitNode(leftNode, ImGuiDir_Down, dockLeftBottomRatio, &leftBottomNode, &leftTopNode);
        ImGui::DockBuilderSplitNode(leftTopNode, ImGuiDir_Up, toolDockSplitRatio, &toolNode, &leftTopNode);
        for (const auto & document : state.session.documents())
        {
            ImGui::DockBuilderDockWindow(documentWindowName(document.summary).c_str(), centerNode);
        }
        ImGui::DockBuilderDockWindow("Tool Palette###panel.tool-palette", toolNode);
        ImGui::DockBuilderDockWindow("Tile Inspector###panel.tile-inspector", leftBottomNode);
        ImGui::DockBuilderDockWindow("Block Inspector###panel.block-inspector", leftTopNode);
        ImGui::DockBuilderFinish(dockSpaceId);
    }
    ImGui::DockSpace(dockSpaceId, ImVec2(viewport->WorkSize.x, availableDockHeight), dockSpaceFlags);
    ImGui::End();
}

void captureWindowGeometry(GLFWwindow * window, AppLayout & layout)
{
    glfwGetWindowPos(window, &layout.window.x, &layout.window.y);
    glfwGetWindowSize(window, &layout.window.width, &layout.window.height);
    layout.window.geometryValid = layout.window.width > 0 && layout.window.height > 0;
}

bool applyWorkbenchLayout(GLFWwindow * window, WorkbenchState & state)
{
    if (state.workbenchLayoutApplied)
    {
        return true;
    }

    if (state.preferences.fullscreen)
    {
        GLFWmonitor * monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode * mode = monitor == nullptr ? nullptr : glfwGetVideoMode(monitor);
        if (mode == nullptr)
        {
            return false;
        }
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    }
    else if (state.preferences.maximized)
    {
        glfwMaximizeWindow(window);
    }
    else if (state.layout.window.geometryValid)
    {
        glfwSetWindowPos(window, state.layout.window.x, state.layout.window.y);
        glfwSetWindowSize(window, state.layout.window.width, state.layout.window.height);
    }

    state.workbenchLayoutApplied = true;
    return true;
}

void toggleFullscreen(GLFWwindow * window, WorkbenchState & state)
{
    if (!state.fullscreenToggleRequested)
    {
        return;
    }

    state.fullscreenToggleRequested = false;
    if (!state.preferences.fullscreen)
    {
        captureWindowGeometry(window, state.layout);
        GLFWmonitor * monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode * mode = monitor == nullptr ? nullptr : glfwGetVideoMode(monitor);
        if (mode == nullptr)
        {
            return;
        }

        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        state.preferences.fullscreen = true;
        return;
    }

    const int width = state.layout.window.geometryValid ? state.layout.window.width : defaultWindowWidth;
    const int height = state.layout.window.geometryValid ? state.layout.window.height : defaultWindowHeight;
    const int x = state.layout.window.geometryValid ? state.layout.window.x : 0;
    const int y = state.layout.window.geometryValid ? state.layout.window.y : 0;
    glfwSetWindowMonitor(window, nullptr, x, y, width, height, 0);
    state.preferences.fullscreen = false;
}

void pickAssetItem(WorkbenchState & state, const assets::AssetItemId & item, bool tiledataSource)
{
    if (!state.toolState.tilePickRequested())
    {
        return;
    }
    const auto id = assets::itemIndex(item);
    if (!id)
    {
        return;
    }
    if (state.toolState.tilePickTarget() == TilePickTarget::fuzzyName)
    {
        if (!tiledataSource)
        {
            return;
        }
        if (const auto tile = state.providers.tiledata().mapTile(static_cast<std::uint16_t>(*id)))
        {
            state.toolState.setPickedTileName(tile->record.name);
        }
        return;
    }
    static_cast<void>(state.toolState.setPickedTileId(static_cast<std::int32_t>(*id)));
}

[[nodiscard]] const DocumentRecord * findDocument(const WorkbenchState & state, DocumentId documentId)
{
    for (const auto & record : state.session.documents())
    {
        if (record.summary.identity.id == documentId)
        {
            return &record;
        }
    }
    return nullptr;
}

[[nodiscard]] uomul::Error dragDropError(std::string message)
{
    return {.code = uomul::ErrorCode::invalidFormat, .path = {}, .offset = 0, .message = std::move(message)};
}

[[nodiscard]] std::expected<uomul::TextureImage, uomul::Error> textureFromProvider(const TextureProvider & provider,
                                                                                   std::uint32_t textureId)
{
    const auto source = provider.texture(textureId);
    if (!source)
    {
        return std::unexpected(dragDropError(strings::sourceTexmapUnavailable()));
    }
    const auto texture = providerTextureToTextureImage(*source);
    if (!texture)
    {
        const auto & error = texture.error();
        return std::unexpected(dragDropError(error.code == uomul::ErrorCode::invalidDimensions
                                                 ? strings::sourceTexmapUnsupportedDimensions()
                                                 : strings::sourceTexmapInvalidPixels()));
    }
    return *texture;
}

void configureTexmapDragCallbacks(WorkbenchState & state)
{
    state.texmapsState.resolveDragTexture =
        [&state](const assets::AssetDragPayloadValue & payload) -> std::expected<uomul::TextureImage, uomul::Error>
    {
        if (payload.source == assets::AssetDragPayloadSource::provider)
        {
            return textureFromProvider(state.providers.textures(), payload.item);
        }
        const auto * record = findDocument(state, payload.sourceDocumentId);
        if (record == nullptr || !record->texmaps)
        {
            return std::unexpected(dragDropError(strings::sourceTexmapsDocumentClosed()));
        }
        const auto source = record->texmaps->effectiveTexture(payload.item);
        if (!source)
        {
            return std::unexpected(source.error());
        }
        if (!source->has_value())
        {
            return std::unexpected(dragDropError(strings::sourceTexmapSlotEmpty()));
        }
        return **source;
    };
    state.texmapsState.dragDropErrorReported = [&state](std::string message)
    { state.notifications.add(NotificationSeverity::error, std::move(message)); };
}

[[nodiscard]] std::expected<std::vector<TexmapExportItem>, uomul::Error>
collectTexmapExportItems(const WorkbenchState & state,
                         const std::optional<DocumentId> & sourceDocumentId,
                         const std::vector<std::uint32_t> & textureIds)
{
    std::vector<TexmapExportItem> items;
    items.reserve(textureIds.size());
    if (sourceDocumentId)
    {
        const auto * record = findDocument(state, *sourceDocumentId);
        if (record == nullptr || !record->texmaps)
        {
            return std::unexpected(dragDropError(strings::sourceTexmapsDocumentClosed()));
        }
        for (const auto textureId : textureIds)
        {
            const auto texture = record->texmaps->effectiveTexture(textureId);
            if (!texture)
            {
                return std::unexpected(texture.error());
            }
            if (!texture->has_value())
            {
                return std::unexpected(dragDropError(strings::selectedTexmapSlotEmpty()));
            }
            items.push_back({.textureId = textureId, .texture = **texture});
        }
        return items;
    }

    for (const auto textureId : textureIds)
    {
        const auto texture = textureFromProvider(state.providers.textures(), textureId);
        if (!texture)
        {
            return std::unexpected(texture.error());
        }
        items.push_back({.textureId = textureId, .texture = *texture});
    }
    return items;
}

void executeTexmapExport(WorkbenchState & state, bool overwrite)
{
    const auto items = collectTexmapExportItems(
        state, state.texmapsState.exportState.sourceDocumentId, state.texmapsState.exportState.textureIds);
    if (!items)
    {
        state.operationStatus = items.error().message;
        state.operationStatusSeverity = NotificationSeverity::error;
        return;
    }

    const TexmapExportOptions options{
        .format = state.texmapsState.exportState.format,
        .destination = items->size() == 1U ? assets::AssetExportDestination::file : assets::AssetExportDestination::directory,
        .outputPath = state.texmapsState.exportState.outputPath,
        .overwrite = overwrite,
    };
    const auto report = exportTexmaps(*items, options);
    if (!report)
    {
        if (report.error().message == "overwrite confirmation required")
        {
            state.texmapsState.exportState.overwriteVisible = true;
            return;
        }
        state.operationStatus = report.error().message;
        state.operationStatusSeverity = NotificationSeverity::error;
        return;
    }

    const auto failureCount = report->failures.size();
    state.operationStatus = "Exported " + std::to_string(report->exported) + " texture" + (report->exported == 1U ? "" : "s") +
                            ", failed " + std::to_string(failureCount) + ".";
    state.operationStatusSeverity = failureCount == 0U ? NotificationSeverity::success : NotificationSeverity::warning;
    if (failureCount != 0U)
    {
        state.notifications.add(NotificationSeverity::warning, report->failures.front().message);
    }
}

void configureTexmapExportCallbacks(WorkbenchState & state)
{
    state.texmapsState.exportRequested = [&state](std::optional<DocumentId> sourceDocumentId,
                                                  std::vector<std::uint32_t> textureIds,
                                                  assets::AssetExportFormat format)
    {
        if (textureIds.empty())
        {
            return;
        }
        state.texmapsState.exportState = {};
        state.texmapsState.exportState.sourceIsDocument = sourceDocumentId.has_value();
        state.texmapsState.exportState.sourceDocumentId = sourceDocumentId;
        state.texmapsState.exportState.textureIds = std::move(textureIds);
        state.texmapsState.exportState.format = format;
        const auto & ids = state.texmapsState.exportState.textureIds;
        if (ids.size() == 1U)
        {
            const auto filename = texmapExportFilename(ids.front(), format);
            requestFileBrowser(state.fileBrowser,
                               {
                                   .mode = FileBrowserMode::saveFile,
                                   .target = FileBrowserTarget::exportTexmapFile,
                                   .initialPath = std::filesystem::current_path() / filename,
                                   .extension = filename.substr(filename.find_last_of('.')),
                                   .title = strings::exportTexmap(),
                               });
        }
        else
        {
            requestFileBrowser(state.fileBrowser,
                               {
                                   .mode = FileBrowserMode::chooseDirectory,
                                   .target = FileBrowserTarget::exportTexmapFolder,
                                   .initialPath = {},
                                   .extension = {},
                                   .title = strings::chooseTexmapExportFolder(),
                               });
        }
    };
    state.texmapsState.exportFormatRequested =
        [&state](std::optional<DocumentId> sourceDocumentId, std::vector<std::uint32_t> textureIds)
    {
        state.texmapsState.exportState = {};
        state.texmapsState.exportState.formatVisible = true;
        state.texmapsState.exportState.sourceIsDocument = sourceDocumentId.has_value();
        state.texmapsState.exportState.sourceDocumentId = sourceDocumentId;
        state.texmapsState.exportState.textureIds = std::move(textureIds);
    };
    state.texmapsState.exportFilePicked = [&state](const std::filesystem::path & path)
    {
        state.deferWorkflowAction(
            [&state, path]
            {
                state.texmapsState.exportState.outputPath = path;
                executeTexmapExport(state, false);
            });
    };
    state.texmapsState.exportFolderPicked = [&state](const std::filesystem::path & path)
    {
        state.deferWorkflowAction(
            [&state, path]
            {
                state.texmapsState.exportState.outputPath = path;
                executeTexmapExport(state, false);
            });
    };
    state.texmapsState.exportOverwriteConfirmed = [&state]
    { state.deferWorkflowAction([&state] { executeTexmapExport(state, true); }); };
    state.texmapsState.exportOverwriteCancelled = [&state]
    {
        state.operationStatus = strings::exportCancelled();
        state.operationStatusSeverity = NotificationSeverity::info;
    };
}

void configureFolderReplacementCallbacks(WorkbenchState & state)
{
    state.texmapsState.folderReplacementRequested = [&state](DocumentId documentId, std::vector<std::uint32_t> textureIds)
    {
        state.texmapsState.folderReplacement = {};
        state.texmapsState.folderReplacement.documentId = documentId;
        state.texmapsState.folderReplacement.textureIds = std::move(textureIds);
        requestFileBrowser(state.fileBrowser,
                           {
                               .mode = FileBrowserMode::chooseDirectory,
                               .target = FileBrowserTarget::chooseImageFolder,
                               .initialPath = {},
                               .extension = {},
                               .title = strings::chooseTextureReplacementFolder(),
                           });
    };
    state.texmapsState.folderReplacementFolderPicked = [&state](const std::filesystem::path & path)
    {
        state.deferWorkflowAction(
            [&state, path]
            {
                state.texmapsState.folderReplacement.visible = true;
                const auto * record = findDocument(state, state.texmapsState.folderReplacement.documentId);
                if (record == nullptr || !record->texmaps)
                {
                    state.texmapsState.folderReplacement.status = strings::sourceTexmapsDocumentClosed();
                    state.texmapsState.replacementStatusReported(state.texmapsState.folderReplacement.status, false);
                    return;
                }
                const auto scanned =
                    scanTexmapReplacementFolder(path, state.texmapsState.folderReplacement.textureIds, *record->texmaps);
                if (!scanned)
                {
                    state.texmapsState.folderReplacement.status = scanned.error().message;
                    state.texmapsState.replacementStatusReported(state.texmapsState.folderReplacement.status, false);
                    return;
                }
                state.texmapsState.folderReplacement.replacement = *scanned;
                state.texmapsState.folderReplacement.status.clear();
            });
    };
}

[[nodiscard]] DocumentRecord * openConfiguredMetadataDocument(WorkbenchState & state, DocumentKind kind)
{
    const auto source = configuredDocumentSourceIdentity(state.activeProfile, kind);
    if (!source)
    {
        state.notifications.add(NotificationSeverity::error, strings::metadataSourceNotConfigured());
        return nullptr;
    }

    const auto role = kind == DocumentKind::texmaps ? SourceRole::index : SourceRole::primary;
    const auto * path = source->path(role);
    if (path == nullptr || path->empty())
    {
        state.notifications.add(NotificationSeverity::error, strings::metadataSourceFileNotConfigured());
        return nullptr;
    }

    const auto * displayPath = kind == DocumentKind::texmaps ? source->path(SourceRole::data) : path;
    const DocumentSummary summary{
        .identity = {.kind = kind, .source = *source},
        .displayName = (displayPath == nullptr ? *path : *displayPath).filename().string(),
        .sourceKind = DocumentSourceKind::configuredMetadata,
        .validation = DocumentValidationState::unknown,
        .facetKey = std::nullopt,
        .dimensions = std::nullopt,
        .dirty = false,
    };
    const DocumentId id = state.session.openDocument(summary);
    if (id == 0)
    {
        state.notifications.add(NotificationSeverity::error, strings::metadataDocumentCouldNotOpen());
        return nullptr;
    }
    if (auto activated = state.session.activateDocument(id, state.mapDocuments); !activated)
    {
        state.notifications.add(NotificationSeverity::error, activated.error().message);
        return nullptr;
    }
    for (auto & document : state.session.documentsMutable())
    {
        if (document.summary.identity.id == id)
        {
            return &document;
        }
    }
    return nullptr;
}

void configureMapPanelCallbacks(WorkbenchState & state)
{
    state.radarcolState.clearStatus = [&state] { clearAssetNavigationStatus(state.notifications); };
    state.radarcolState.itemPicked = [&state](const assets::AssetItemId & item)
    {
        clearAssetNavigationStatus(state.notifications);
        pickAssetItem(state, item, false);
    };
    state.texmapsState.itemPicked = [&state](const assets::AssetItemId & item)
    {
        clearAssetNavigationStatus(state.notifications);
        if (state.texmapsState.replacement.visible || state.texmapsState.folderReplacement.visible ||
            !state.texmapsState.folderReplacement.status.empty())
        {
            state.texmapsState.replacement.status.clear();
            state.texmapsState.folderReplacement.status.clear();
            state.operationStatus.clear();
        }
        pickAssetItem(state, item, false);
    };
    state.texmapsState.replacementFileRequested = [&state]
    {
        state.texmapsState.replacement.visible = false;
        requestFileBrowser(state.fileBrowser,
                           {
                               .mode = FileBrowserMode::openFile,
                               .target = FileBrowserTarget::openImage,
                               .initialPath = state.texmapsState.replacement.sourcePath,
                               .extension = ".bmp,.png",
                               .title = strings::openTextureImage(),
                           });
    };
    state.texmapsState.replacementFilePicked = [&state](const std::filesystem::path & path)
    {
        state.deferWorkflowAction(
            [&state, path]
            {
                state.texmapsState.replacement.visible = true;
                loadTexmapsReplacement(state.texmapsState, path);
                state.operationStatus = state.texmapsState.replacement.status;
                state.operationStatusSeverity =
                    state.texmapsState.replacement.texture ? NotificationSeverity::success : NotificationSeverity::error;
            });
    };
    state.texmapsState.replacementStatusReported = [&state](std::string message, bool success)
    {
        state.operationStatus = std::move(message);
        state.operationStatusSeverity = success ? NotificationSeverity::success : NotificationSeverity::error;
    };
    state.texmapsState.replaceRequested = [&state](DocumentId documentId, std::uint32_t textureId)
    {
        state.texmapsState.replacement = {};
        state.texmapsState.replacement.visible = true;
        state.texmapsState.replacement.documentId = documentId;
        state.texmapsState.replacement.textureId = textureId;
    };
    state.tiledataState.itemPicked = [&state](const assets::AssetItemId & item)
    {
        clearAssetNavigationStatus(state.notifications);
        pickAssetItem(state, item, true);
    };
    state.radarcolState.showInTiledata = [&state](std::uint32_t id, bool statics)
    {
        state.windowRegistry.find(WindowId::tiledata)->visible = true;
        state.tiledataState.browserKind = statics ? TiledataBrowserKind::statics : TiledataBrowserKind::map;
        state.tiledataState.assetView.requestedItem = {.value = std::to_string(id)};
        focusConfiguredMetadataWindow(state, DocumentKind::tiledata);
    };
    state.radarcolState.showInTexmaps = [&state](std::uint32_t id)
    {
        state.windowRegistry.find(WindowId::texmaps)->visible = true;
        state.texmapsState.assetView.requestedItem = {.value = std::to_string(id)};
        focusConfiguredMetadataWindow(state, DocumentKind::texmaps);
    };
    state.radarcolState.activeTileColor = [&state] -> std::optional<uomul::RadarColor>
    {
        const auto activeTile = state.toolState.activeTile();
        if (!activeTile)
        {
            return std::nullopt;
        }
        return state.providers.radarColors().rawColor(activeTile->id).value_or(0);
    };
    state.radarcolState.copyToClipboard = [&state](const std::string & text) { state.pendingClipboardText = text; };
    state.radarcolState.useAsActiveTile = [&state](std::uint32_t id)
    {
        const auto result = state.toolState.setActiveTileId(static_cast<std::int32_t>(id));
        if (!result.accepted)
        {
            state.notifications.add(NotificationSeverity::error, result.reason);
        }
    };
    state.texmapsState.itemSourcePicked = [&state](std::uint32_t id)
    { return acceptRadarcolColorSource(state.radarcolState, state.providers.radarColors().rawColor(id).value_or(0)); };
    state.tiledataState.itemSourcePicked = [&state](std::uint32_t id, bool statics)
    {
        const std::uint32_t index = statics ? uomul::radarStaticColorOffset + id : id;
        return acceptRadarcolColorSource(state.radarcolState, state.providers.radarColors().rawColor(index).value_or(0));
    };
    configureFolderReplacementCallbacks(state);
    configureTexmapDragCallbacks(state);
    configureTexmapExportCallbacks(state);
}

void drawVisibleMapPanels(WorkbenchState & state, BlockInspectorPanelState & blockInspectorState)
{
    if (state.panelRegistry.find(PanelId::toolPalette)->visible)
    {
        drawToolPalettePanel(state, state.panelRegistry.find(PanelId::toolPalette)->visible);
    }
    if (state.panelRegistry.find(PanelId::iconGallery)->visible)
    {
        drawIconGalleryPanel(iconFontAvailable(), state.panelRegistry.find(PanelId::iconGallery)->visible);
    }

    MapDocumentState & mapDocument = state.mapDocuments.active();
    const MapContextSnapshot context = state.session.contextSnapshot(state.mapDocuments);
    const DocumentRecord * active = state.session.activeDocumentRecord();
    const bool mapActive =
        active != nullptr && active->map.has_value() && state.mapDocuments.active().changes().reader.has_value();

    if (mapActive && state.panelRegistry.find(PanelId::tileInspector)->visible)
    {
        drawTileInspectorPanel(mapDocument,
                               context,
                               state.providers,
                               state.preferences.idFormat,
                               iconFontAvailable(),
                               state.panelRegistry.find(PanelId::tileInspector)->visible);
    }
    if (mapActive && state.panelRegistry.find(PanelId::blockInspector)->visible)
    {
        drawBlockInspectorPanel(mapDocument,
                                context,
                                state.providers,
                                state.preferences.mapViewTileSource,
                                blockInspectorState,
                                state.panelRegistry.find(PanelId::blockInspector)->visible);
    }
}

void drawMapPanels(WorkbenchState & state, BlockInspectorPanelState & blockInspectorState)
{
    configureMapPanelCallbacks(state);
    drawVisibleMapPanels(state, blockInspectorState);
}

void applyPendingPreferenceChanges(WorkbenchState & state)
{
    if (state.pendingPreferenceChanges.empty())
    {
        return;
    }

    if (state.pendingPreferenceChanges.showWelcomeOnStartup)
    {
        state.preferences.showWelcomeOnStartup = *state.pendingPreferenceChanges.showWelcomeOnStartup;
    }
    state.pendingPreferenceChanges = {};
}

void evictClosedDocumentState(WorkbenchState & state, DocumentId id);

template <typename DocumentStates>
void captureConfiguredMetadataAssetState(WorkbenchState & state, DocumentKind kind, const DocumentStates & documentStates);

[[nodiscard]] bool prepareConfiguredMetadataForProfileSwitch(WorkbenchState & state)
{
    std::vector<DocumentId> configuredDocumentIds;
    for (const auto & document : state.session.documents())
    {
        if (document.summary.sourceKind != DocumentSourceKind::configuredMetadata)
        {
            continue;
        }
        if ((document.radarcol && document.radarcol->dirty()) || (document.texmaps && document.texmaps->dirty()))
        {
            state.notifications.add(NotificationSeverity::error,
                                    "Cannot switch profiles while a configured metadata document has unsaved changes");
            return false;
        }
        configuredDocumentIds.push_back(document.summary.identity.id);
    }

    captureConfiguredMetadataAssetState(state, DocumentKind::radarcol, state.radarcolState.documentAssetViews);
    captureConfiguredMetadataAssetState(state, DocumentKind::texmaps, state.texmapsState.documentAssetViews);
    captureConfiguredMetadataAssetState(state, DocumentKind::tiledata, state.tiledataState.documentAssetViews);
    try
    {
        DataProfile profile = state.activeProfile;
        profile.preferences = state.preferences;
        saveProfileLayout(profile.layout, state.panelRegistry, state.windowRegistry, state.layout);
        state.profileStore.save(profile);
        state.activeProfile = std::move(profile);
    }
    catch (const std::exception & error)
    {
        state.notifications.add(NotificationSeverity::error, "Profile save failed: " + std::string(error.what()));
        return false;
    }

    for (const auto id : configuredDocumentIds)
    {
        if (auto closed = state.session.closeDocument(id, state.mapDocuments); !closed)
        {
            state.notifications.add(NotificationSeverity::error, closed.error().message);
            return false;
        }
        evictClosedDocumentState(state, id);
    }
    return true;
}

[[nodiscard]] bool applyPendingPreferencesDialog(GLFWwindow * window, WorkbenchState & state)
{
    auto & preferencesDialog = state.menuBarState.preferencesDialog;
    if (preferencesDialog.profileSwitchRetryRequested)
    {
        preferencesDialog.profileSwitchRetryRequested = false;
        state.pendingPreferencesPersistenceBlocked = false;
    }
    if (state.pendingPreferencesPersistenceBlocked)
    {
        if (preferencesDialog.profilePersistenceOperation == ProfilePersistenceOperation::none)
        {
            state.pendingPreferencesResult.reset();
            state.pendingPreferencesPersistenceBlocked = false;
        }
        return false;
    }
    if (!state.pendingPreferencesResult)
    {
        return false;
    }

    const PreferencesDialogResult result = *state.pendingPreferencesResult;
    if (!result.applyProfile)
    {
        state.preferences = result.preferences;
        state.pendingPreferencesResult.reset();
        return false;
    }

    const bool profileChanged = result.profile.name != state.activeProfile.name;
    if (state.workbenchLayoutApplied && !state.nativeWelcomeWindowMode)
    {
        const bool nativeFullscreen = glfwGetWindowMonitor(window) != nullptr;
        state.preferences.fullscreen = nativeFullscreen;
        state.preferences.maximized = !nativeFullscreen && glfwGetWindowAttrib(window, GLFW_MAXIMIZED) == GLFW_TRUE;
        if (!nativeFullscreen && !state.preferences.maximized)
        {
            captureWindowGeometry(window, state.layout);
        }
        const char * dockingIni = ImGui::SaveIniSettingsToMemory();
        state.layout.dockingIni = dockingIni == nullptr ? std::string{} : std::string(dockingIni);
    }
    if (profileChanged && !state.activeProfile.name.empty() && !prepareConfiguredMetadataForProfileSwitch(state))
    {
        preferencesDialog.profileSaveRequested = false;
        preferencesDialog.profilePersistenceOperation = ProfilePersistenceOperation::switchProfile;
        preferencesDialog.profilePersistenceError = "Profile save failed. Retry or cancel the profile switch.";
        preferencesDialog.visible = true;
        state.pendingPreferencesPersistenceBlocked = true;
        activatePreferencesDialog(preferencesDialog);
        return false;
    }
    state.pendingPreferencesResult.reset();
    state.activeProfile = result.profile;
    if (profileChanged)
    {
        state.preferences = state.activeProfile.preferences;
        state.preferences.activeDataProfile = state.activeProfile.name;
        state.toolState.loadPreferences(state.preferences.tools);
        state.panelRegistry.resetToDefaults();
        state.windowRegistry.resetToDefaults();
        state.layout = {};
        loadProfileLayout(state.activeProfile.layout, state.panelRegistry, state.windowRegistry, state.layout);
        state.workbenchLayoutApplied = false;
    }
    else
    {
        state.preferences = result.preferences;
        state.preferences.activeDataProfile = state.activeProfile.name;
        state.activeProfile.preferences = state.preferences;
        state.workbenchLayoutApplied = false;
    }
    return true;
}

void applyLogPanelRequest(WorkbenchState & state, logger::GuiLogSink & logSink)
{
    const LogPanelRequest request = state.logPanelRequest;
    state.logPanelRequest = {};
    if (request.visible)
    {
        state.preferences.showLogPanel = *request.visible;
    }
    if (request.autoScroll)
    {
        state.preferences.autoScroll = *request.autoScroll;
    }
    if (request.collapsed)
    {
        state.preferences.logPanelCollapsed = *request.collapsed;
    }
    if (request.height)
    {
        state.preferences.logPanelHeight = *request.height;
    }
    if (request.clearEntries)
    {
        state.logEntries.clear();
        logSink.clear();
    }
}

void notifyRadarcolFallback(WorkbenchState & state)
{
    const ProviderStatus status = state.providers.radarColors().status();
    const bool usingEmbeddedFallback =
        status.source == ProviderSourceKind::embeddedDefault || status.source == ProviderSourceKind::embeddedDefaultWithWarning;
    if (!usingEmbeddedFallback || status.revision == state.radarcolNotificationRevision)
    {
        state.radarcolNotificationRevision = status.revision;
        if (!usingEmbeddedFallback)
        {
            state.radarcolFallbackPromptVisible = false;
        }
        return;
    }

    state.radarcolNotificationRevision = status.revision;
    state.radarcolFallbackPromptVisible = true;
    state.notifications.add(NotificationSeverity::warning,
                            status.source == ProviderSourceKind::embeddedDefaultWithWarning
                                ? strings::formatRadarcolFallbackWarning(status.message)
                                : strings::noExternalRadarcolFallbackWarning());
}

void drawRadarcolFallbackWorkflow(WorkbenchState & state)
{
    if (!state.radarcolFallbackPromptVisible)
    {
        return;
    }

    const std::string popupName = std::string(strings::radarcolFallbackTitle()) + "###dialog.radarcol-fallback";
    if (!beginAutoSizeDialog(popupName.c_str(), &state.radarcolFallbackPromptVisible))
    {
        return;
    }

    ImGui::TextUnformatted(strings::radarcolFallbackMessage());
    ImGui::TextUnformatted(strings::radarcolFallbackQuestion());
    ImGui::Separator();
    if (ImGui::Button(strings::createDefaultRadarcol()))
    {
        const auto initialPath =
            state.activeProfile.sourceRoot.empty() ? std::filesystem::path{} : state.activeProfile.sourceRoot / "radarcol.mul";
        requestFileBrowser(state.fileBrowser,
                           {
                               .mode = FileBrowserMode::saveFile,
                               .target = FileBrowserTarget::createDefaultRadarcol,
                               .initialPath = initialPath,
                               .extension = ".mul",
                               .title = strings::createDefaultRadarcol(),
                           });
        state.radarcolFallbackPromptVisible = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(strings::continueWithEmbedded()))
    {
        state.radarcolFallbackPromptVisible = false;
        ImGui::CloseCurrentPopup();
    }
    endDialog();
}

void drawStatusBar(const WorkbenchState & state, float bottomPanelHeight)
{
    const ImGuiViewport * viewport = ImGui::GetMainViewport();
    const float statusY = viewport->WorkPos.y + viewport->WorkSize.y - bottomPanelHeight - statusBarHeight;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, statusY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, statusBarHeight), ImGuiCond_Always);
    constexpr auto statusFlags = static_cast<ImGuiWindowFlags>(
        static_cast<unsigned int>(ImGuiWindowFlags_NoDecoration) | static_cast<unsigned int>(ImGuiWindowFlags_NoDocking) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoMove) | static_cast<unsigned int>(ImGuiWindowFlags_NoResize) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoSavedSettings) | static_cast<unsigned int>(ImGuiWindowFlags_NoScrollbar));
    ImGui::Begin("Workbench Status###workbench.status", nullptr, statusFlags);
    const auto drawStatusText = [](std::string_view text, NotificationSeverity severity)
    {
        ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        if (severity == NotificationSeverity::error)
        {
            color = theme::imguiColor(theme::error);
        }
        else if (severity == NotificationSeverity::warning)
        {
            color = theme::imguiColor(theme::warning);
        }
        else if (severity == NotificationSeverity::success)
        {
            color = theme::imguiColor(theme::success);
        }
        ImGui::TextColored(color, "%.*s", static_cast<int>(text.size()), text.data());
        ImGui::SameLine();
    };
    if (!state.hoveredPanelContext.empty())
    {
        drawStatusText(state.hoveredPanelContext, NotificationSeverity::info);
    }

    std::size_t selectedRadarcolColors = state.radarcolState.assetView.selection.selected.size();
    if (const DocumentRecord * active = state.session.activeDocumentRecord(); active != nullptr && active->radarcol)
    {
        if (const auto view = state.radarcolState.documentAssetViews.find(active->radarcol->id());
            view != state.radarcolState.documentAssetViews.end())
        {
            selectedRadarcolColors = view->second.selection.selected.size();
        }
    }
    const bool radarcolEditing = state.radarcolState.colorPicker.open || state.radarcolState.colorPicker.sourceMode;
    if (state.radarcolState.colorPicker.sourceMode)
    {
        drawStatusText(strings::pickMapTileOrAsset(), NotificationSeverity::info);
    }
    else if (radarcolEditing)
    {
        drawStatusText(strings::formatEditingRadarcols(state.radarcolState.colorPicker.targetIndices.size()),
                       NotificationSeverity::info);
    }
    else
    {
        drawStatusText(strings::formatSelectedColors(selectedRadarcolColors), NotificationSeverity::info);
    }
    if (!radarcolEditing && !state.operationStatus.empty())
    {
        drawStatusText(state.operationStatus, state.operationStatusSeverity);
    }
    if (!radarcolEditing && !state.notifications.entries.empty())
    {
        const Notification & notification = state.notifications.entries.back();
        drawStatusText(notification.message, notification.severity);
    }
    bool hasCanvasStatus = false;
    if (state.lastHoveredCanvasStatus)
    {
        hasCanvasStatus = true;
        const MapCanvasStatus & canvasStatus = *state.lastHoveredCanvasStatus;
        if (!canvasStatus.hasDocument || !canvasStatus.tile)
        {
            const std::string status = strings::formatCanvasNoTile(canvasStatus.canvasId, canvasStatus.zoom);
            ImGui::TextUnformatted(status.c_str());
        }
        else
        {
            const uomul::BlockCoord block = tileToBlock(*canvasStatus.tile);
            const std::string status = strings::formatCanvasTile(
                canvasStatus.canvasId, canvasStatus.tile->x, canvasStatus.tile->y, block.x, block.y, canvasStatus.zoom);
            ImGui::TextUnformatted(status.c_str());
        }
    }
    if (hasCanvasStatus)
    {
        ImGui::SameLine();
    }
    const std::string profileStatus = strings::formatProfileStatus(state.activeProfile.name);
    const std::string mapStatus = strings::formatMapStatus(!state.mapDocuments.active().changes().reader.has_value()
                                                               ? std::filesystem::path{}
                                                               : state.mapDocuments.active().changes().mapPath);
    const std::string stateStatus = strings::formatStateStatus(state.mapDocuments.active().changes().dirty());
    const std::string readinessStatus = strings::formatDataStatus(state.session.profileState());
    const std::string radarcolStatus = strings::formatRadarcolStatus(state.providers.radarColors().status().source);
    const float rightContentWidth = ImGui::CalcTextSize(profileStatus.c_str()).x + ImGui::GetStyle().ItemSpacing.x +
                                    ImGui::CalcTextSize(mapStatus.c_str()).x + ImGui::GetStyle().ItemSpacing.x +
                                    ImGui::CalcTextSize(stateStatus.c_str()).x + ImGui::GetStyle().ItemSpacing.x +
                                    ImGui::CalcTextSize(readinessStatus.c_str()).x + ImGui::GetStyle().ItemSpacing.x +
                                    ImGui::CalcTextSize(radarcolStatus.c_str()).x;
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - rightContentWidth));
    ImGui::TextUnformatted(profileStatus.c_str());
    ImGui::SameLine();
    ImGui::TextUnformatted(mapStatus.c_str());
    ImGui::SameLine();
    ImGui::TextUnformatted(stateStatus.c_str());
    ImGui::SameLine();
    ImGui::TextUnformatted(readinessStatus.c_str());
    ImGui::SameLine();
    ImGui::TextUnformatted(radarcolStatus.c_str());
    ImGui::End();
}

void drawStartupScreen(WorkbenchState & state)
{
    const ImGuiViewport * viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);
    if (!ImGui::Begin(
            strings::configurePreferencesWindow(), nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::End();
        return;
    }
    const std::string configureLabel = iconLabel(IconId::preferences, strings::configurePreferencesLabel());
    ImGui::TextUnformatted(configureLabel.c_str());
    ImGui::Separator();
    ImGui::TextUnformatted(strings::mapsWithoutProfile());
    const std::string profileReadiness(strings::profileReadinessName(state.session.profileState()));
    const std::string profileStatus = strings::formatProfileReadiness(profileReadiness);
    ImGui::TextUnformatted(profileStatus.c_str());
    const std::string startupRadarcolSource(strings::providerSourceName(state.providers.radarColors().status().source));
    const std::string radarcolStatus = strings::formatRadarcolSource(startupRadarcolSource);
    ImGui::TextUnformatted(radarcolStatus.c_str());
    const std::string preferencesLabel = iconLabel(IconId::preferences, strings::openPreferences());
    if (ImGui::Button(preferencesLabel.c_str()))
    {
        requestPreferencesDialog(state.menuBarState.preferencesDialog, state.activeProfile, state.preferences);
    }
    ImGui::End();
}

ImGuiID documentDockNodeId()
{
    const ImGuiID dockSpaceId = ImGui::GetID("BackgroundDockSpace");
    const ImGuiDockNode * centralNode = ImGui::DockBuilderGetCentralNode(dockSpaceId);
    return centralNode == nullptr ? 0U : centralNode->ID;
}

void requestDocumentClose(WorkbenchState & state, DocumentId id);

void activateDocumentOrNotify(WorkbenchState & state, DocumentId id)
{
    const auto activated = state.session.activateDocument(id, state.mapDocuments);
    if (!activated)
    {
        state.notifications.add(NotificationSeverity::error, activated.error().message);
    }
}

void requestDocumentActivation(WorkbenchState & state, DocumentId id)
{
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
        state.documentLifecycle.requestActivation(id);
    }
}

void dockDocumentWindow(const WorkbenchState & state, DocumentId id)
{
    if (const ImGuiID centralNode = documentDockNodeId(); centralNode != 0U)
    {
        const ImGuiCond condition = state.pendingDocumentFocus == id ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
        ImGui::SetNextWindowDockID(centralNode, condition);
        if (state.pendingDocumentFocus == id)
        {
            ImGui::SetNextWindowFocus();
        }
    }
}

void finishDocumentWindowFocus(WorkbenchState & state, DocumentId id, const std::string & windowName)
{
    if (state.pendingDocumentFocus == id)
    {
        if (ImGuiWindow * window = ImGui::FindWindowByName(windowName.c_str()); window != nullptr)
        {
            // SetWindowFocus() updates navigation focus, but Dear ImGui does not
            // select the corresponding tab for an already-docked window. Update
            // the dock tab state explicitly so reopening a document promotes it
            // in a stacked center node.
            ImGui::FocusWindow(window);
            if (ImGuiDockNode * dockNode = window->DockNode; dockNode != nullptr)
            {
                dockNode->SelectedTabId = window->TabId;
                if (dockNode->TabBar != nullptr)
                {
                    dockNode->TabBar->SelectedTabId = window->TabId;
                    dockNode->TabBar->NextSelectedTabId = window->TabId;
                }
            }
        }
        state.pendingDocumentFocus.reset();
    }
}

void reportOperation(WorkbenchState & state, std::string message, bool success)
{
    state.operationStatus = std::move(message);
    state.operationStatusSeverity = success ? NotificationSeverity::success : NotificationSeverity::error;
}

void executeMapEdit(WorkbenchState & state, uomul::TileCoord origin)
{
    if (state.toolState.activeTool() == ToolId::fill)
    {
        const auto result =
            state.mapDocuments.fillMasked(state.toolState.activeTile().value_or(uomul::MapTile{}), state.toolState.fillMode());
        reportOperation(state,
                        result ? "Filled " + std::to_string(result->changedTiles) + " tiles" : result.error().message,
                        result.has_value());
        return;
    }
    if (state.toolState.activeTool() == ToolId::replace)
    {
        const auto result = state.mapDocuments.replaceMasked(
            state.toolState.replaceMode(), state.toolState.replaceSource(), state.toolState.replaceTarget());
        reportOperation(state,
                        result ? "Replaced " + std::to_string(result->changedTiles) + " tiles" : result.error().message,
                        result.has_value());
        return;
    }
    if (state.toolState.activeTool() == ToolId::fuzzySelect)
    {
        const FuzzyTiledataNameLookup nameLookup = [&state](std::uint16_t id) -> std::optional<std::string>
        {
            const auto tile = state.providers.tiledata().mapTile(id);
            return tile ? std::optional<std::string>{tile->record.name} : std::nullopt;
        };
        const auto result = state.mapDocuments.fuzzySelect(state.toolState.fuzzyOptions(), origin, nameLookup);
        reportOperation(
            state, result ? "Selected " + std::to_string(*result) + " tiles" : result.error().message, result.has_value());
    }
}

[[nodiscard]] std::function<void(uomul::TileCoord)> mapEditCallback(WorkbenchState & state)
{
    return [&state](uomul::TileCoord origin) { executeMapEdit(state, origin); };
}

[[nodiscard]] std::function<std::optional<std::string>(std::uint16_t)> tiledataNameCallback(WorkbenchState & state)
{
    return [&state](std::uint16_t id) -> std::optional<std::string>
    {
        const auto tile = state.providers.tiledata().mapTile(id);
        return tile ? std::optional<std::string>{tile->record.name} : std::nullopt;
    };
}

void ensureConfiguredMetadataViewDocument(WorkbenchState & state, DocumentKind kind)
{
    const auto source = configuredDocumentSourceIdentity(state.activeProfile, kind);
    if (!source)
    {
        return;
    }
    const auto * path = source->path(kind == DocumentKind::texmaps ? SourceRole::index : SourceRole::primary);
    if (path == nullptr || path->empty())
    {
        return;
    }
    for (const auto & document : state.session.documents())
    {
        if (document.summary.identity.kind == kind && document.summary.identity.source == *source)
        {
            return;
        }
    }
    static_cast<void>(openConfiguredMetadataDocument(state, kind));
}

void ensureConfiguredMetadataWindows(WorkbenchState & state)
{
    if (metadataViewVisible(state, DocumentKind::radarcol))
    {
        ensureConfiguredMetadataViewDocument(state, DocumentKind::radarcol);
    }
    if (metadataViewVisible(state, DocumentKind::texmaps))
    {
        ensureConfiguredMetadataViewDocument(state, DocumentKind::texmaps);
    }
    if (metadataViewVisible(state, DocumentKind::tiledata))
    {
        ensureConfiguredMetadataViewDocument(state, DocumentKind::tiledata);
    }
}

void focusConfiguredMetadataWindow(WorkbenchState & state, DocumentKind kind)
{
    ensureConfiguredMetadataViewDocument(state, kind);
    for (const auto & document : state.session.documents())
    {
        if (document.summary.identity.kind == kind && document.summary.sourceKind == DocumentSourceKind::configuredMetadata)
        {
            ImGui::SetWindowFocus(documentWindowName(document.summary).c_str());
            return;
        }
    }
}

[[nodiscard]] WindowId metadataWindowId(DocumentKind kind) noexcept
{
    if (kind == DocumentKind::texmaps)
    {
        return WindowId::texmaps;
    }
    if (kind == DocumentKind::tiledata)
    {
        return WindowId::tiledata;
    }
    return WindowId::radarcol;
}

[[nodiscard]] std::string_view metadataContextId(DocumentKind kind) noexcept
{
    switch (kind)
    {
        case DocumentKind::radarcol :
            return "window.radarcol";
        case DocumentKind::texmaps :
            return "window.texmaps";
        case DocumentKind::tiledata :
            return "window.tiledata";
        case DocumentKind::map :
        case DocumentKind::statics :
        case DocumentKind::combined :
        case DocumentKind::unavailable :
            return {};
    }
    return {};
}

[[nodiscard]] bool metadataViewVisible(const WorkbenchState & state, DocumentKind kind) noexcept
{
    const auto * descriptor = state.windowRegistry.find(metadataWindowId(kind));
    return descriptor != nullptr && descriptor->visible;
}

void hideConfiguredMetadataView(WorkbenchState & state, const DocumentSummary & summary)
{
    if (summary.sourceKind != DocumentSourceKind::configuredMetadata)
    {
        return;
    }
    auto * descriptor = state.windowRegistry.find(metadataWindowId(summary.identity.kind));
    if (descriptor != nullptr)
    {
        descriptor->visible = false;
    }
}

[[nodiscard]] bool shouldDrawDocumentWindow(const WorkbenchState & state, const DocumentSummary & summary) noexcept
{
    return summary.sourceKind != DocumentSourceKind::configuredMetadata || metadataViewVisible(state, summary.identity.kind);
}

void forwardMetadataAssetRequest(WorkbenchState & state,
                                 DocumentKind kind,
                                 assets::AssetWindowState & sourceState,
                                 std::unordered_map<DocumentId, assets::AssetWindowState> & documentStates)
{
    if (!sourceState.requestedItem && !sourceState.requestedItemIndex)
    {
        return;
    }
    for (const auto & document : state.session.documents())
    {
        if (document.summary.identity.kind != kind || document.summary.sourceKind != DocumentSourceKind::configuredMetadata)
        {
            continue;
        }
        auto & targetState = documentStates[document.summary.identity.id];
        if (kind == DocumentKind::tiledata)
        {
            targetState.focusResult = [&state](bool visible)
            {
                if (!visible)
                {
                    state.notifications.add(NotificationSeverity::info, strings::tileNotVisibleWithCurrentFilter());
                }
            };
        }
        else if (kind == DocumentKind::texmaps)
        {
            targetState.focusResult = [&state](bool visible)
            {
                if (!visible)
                {
                    state.notifications.add(NotificationSeverity::info, strings::textureNotVisibleWithCurrentFilter());
                }
            };
        }
        targetState.requestedItem = std::move(sourceState.requestedItem);
        targetState.requestedItemIndex = sourceState.requestedItemIndex;
        return;
    }
}

void drawDocumentWindow(WorkbenchState & state, DocumentRecord & document)
{
    const std::string windowName = documentWindowName(document.summary);
    if (!shouldDrawDocumentWindow(state, document.summary))
    {
        return;
    }
    const DocumentId id = document.summary.identity.id;
    if (document.summary.identity.kind == DocumentKind::map && document.map)
    {
        bool visible = true;
        dockDocumentWindow(state, id);
        drawMapDocumentWindow(
            state.mapDocuments,
            state.toolState,
            state.coordinateDialog,
            state.providers.textures(),
            state.providers.radarColors(),
            state.preferences.mapViewTileSource,
            windowName,
            [&state, id = document.summary.identity.id] { requestDocumentActivation(state, id); },
            [&state]
            { state.deferWorkflowAction([&state] { static_cast<void>(state.commands.execute(CommandId::fileOpen, state)); }); },
            [&state]
            { state.deferWorkflowAction([&state] { static_cast<void>(state.commands.execute(CommandId::fileNew, state)); }); },
            mapEditCallback(state),
            tiledataNameCallback(state),
            [&state](uomul::MapTile tile)
            {
                return acceptRadarcolColorSource(state.radarcolState,
                                                 state.providers.radarColors().rawColor(tile.id).value_or(0));
            },
            [&state](const MapCanvasStatus & status) { state.lastHoveredCanvasStatus = status; },
            [&state] { state.activeAssetPanelContext.clear(); },
            visible);
        if (!visible)
        {
            requestDocumentClose(state, document.summary.identity.id);
        }
        finishDocumentWindowFocus(state, id, windowName);
        return;
    }

    if (document.summary.identity.kind == DocumentKind::radarcol && document.radarcol)
    {
        auto [assetView, inserted] = state.radarcolState.documentAssetViews.try_emplace(document.summary.identity.id);
        if (inserted)
        {
            restoreConfiguredMetadataDocumentAssetState(state, document, assetView->second);
        }
        bool visible = true;
        dockDocumentWindow(state, id);
        drawRadarcolDocumentWindow(state.radarcolState,
                                   *document.radarcol,
                                   visible,
                                   state.preferences.idFormat,
                                   windowName,
                                   [&state, id = document.summary.identity.id] { requestDocumentActivation(state, id); });
        if (!visible)
        {
            hideConfiguredMetadataView(state, document.summary);
            requestDocumentClose(state, document.summary.identity.id);
        }
        finishDocumentWindowFocus(state, id, windowName);
        return;
    }

    if (document.summary.identity.kind == DocumentKind::tiledata && document.tiledata)
    {
        auto [assetView, inserted] = state.tiledataState.documentAssetViews.try_emplace(document.summary.identity.id);
        if (inserted)
        {
            restoreConfiguredMetadataDocumentAssetState(state, document, assetView->second);
        }
        bool visible = true;
        dockDocumentWindow(state, id);
        drawTiledataDocumentWindow(state.tiledataState,
                                   *document.tiledata,
                                   visible,
                                   state.preferences.idFormat,
                                   state.providers.textures(),
                                   state.providers.radarColors(),
                                   windowName);
        if (!visible)
        {
            hideConfiguredMetadataView(state, document.summary);
            requestDocumentClose(state, document.summary.identity.id);
        }
        finishDocumentWindowFocus(state, id, windowName);
        return;
    }

    if (document.summary.identity.kind == DocumentKind::texmaps && document.texmaps)
    {
        auto [assetView, inserted] = state.texmapsState.documentAssetViews.try_emplace(document.summary.identity.id);
        if (inserted)
        {
            assetView->second.hideEmpty = true;
            restoreConfiguredMetadataDocumentAssetState(state, document, assetView->second);
        }
        bool visible = true;
        dockDocumentWindow(state, id);
        drawTexmapsDocumentWindow(state.texmapsState,
                                  *document.texmaps,
                                  visible,
                                  state.preferences.idFormat,
                                  windowName,
                                  [&state, id = document.summary.identity.id] { requestDocumentActivation(state, id); });
        if (!visible)
        {
            hideConfiguredMetadataView(state, document.summary);
            requestDocumentClose(state, document.summary.identity.id);
        }
        finishDocumentWindowFocus(state, id, windowName);
        return;
    }

    bool visible = true;
    dockDocumentWindow(state, id);
    if (ImGui::Begin(windowName.c_str(), &visible))
    {
        requestDocumentActivation(state, document.summary.identity.id);
        const std::string kindName(documentKindName(document.summary.identity.kind));
        const std::string noView = strings::formatDocumentNoView(kindName);
        ImGui::TextUnformatted(noView.c_str());
        ImGui::TextUnformatted(strings::documentPlaceholder());
    }
    ImGui::End();
    if (!visible)
    {
        requestDocumentClose(state, document.summary.identity.id);
    }
    finishDocumentWindowFocus(state, id, windowName);
}

void drawDocumentWindows(WorkbenchState & state)
{
    forwardMetadataAssetRequest(
        state, DocumentKind::radarcol, state.radarcolState.assetView, state.radarcolState.documentAssetViews);
    forwardMetadataAssetRequest(
        state, DocumentKind::texmaps, state.texmapsState.assetView, state.texmapsState.documentAssetViews);
    forwardMetadataAssetRequest(
        state, DocumentKind::tiledata, state.tiledataState.assetView, state.tiledataState.documentAssetViews);
    for (auto & document : state.session.documentsMutable())
    {
        drawDocumentWindow(state, document);
    }
}

void drawDocumentSurface(WorkbenchState & state)
{
    drawDocumentWindows(state);
}

void continueQuit(WorkbenchState & state)
{
    if (!state.quitRequested)
    {
        return;
    }
    if (state.session.documents().empty())
    {
        state.quitRequested = false;
        state.shouldClose = true;
        return;
    }

    state.documentLifecycle.requestClose(
        {.id = state.session.documents().front().summary.identity.id, .dirtyPolicy = DocumentCloseDirtyPolicy::prompt});
}

void closePendingDocument(WorkbenchState & state, DocumentId id);

void finishPendingDocumentClose(WorkbenchState & state, DocumentId id, PendingChangesAction action)
{
    if (action == PendingChangesAction::save)
    {
        const auto transition = state.operationController.completeSave(true);
        if (transition.kind == OperationTransitionKind::execute)
        {
            state.deferWorkflowAction([&state, id] { closePendingDocument(state, id); });
        }
    }
    else if (action == PendingChangesAction::discard)
    {
        static_cast<void>(state.operationController.cancel());
        state.deferWorkflowAction([&state, id] { closePendingDocument(state, id); });
    }
}

void rearmPendingDocumentClose(WorkbenchState & state, DocumentId id);

void processDocumentIntent(WorkbenchState & state, DocumentRecord & document)
{
    if (!document.radarcol && !document.texmaps)
    {
        return;
    }
    const auto result =
        document.radarcol ? document.radarcol->processPendingIntent() : document.texmaps->processPendingIntent();
    if (!result)
    {
        state.notifications.add(NotificationSeverity::error, result.error().message);
        if (state.pendingDocumentCloseDecision && state.pendingDocumentCloseAction &&
            *state.pendingDocumentCloseDecision == document.summary.identity.id)
        {
            rearmPendingDocumentClose(state, document.summary.identity.id);
        }
        return;
    }
    if (state.pendingDocumentCloseDecision && state.pendingDocumentCloseAction &&
        *state.pendingDocumentCloseDecision == document.summary.identity.id)
    {
        const DocumentId id = *state.pendingDocumentCloseDecision;
        const PendingChangesAction action = *state.pendingDocumentCloseAction;
        state.pendingDocumentCloseDecision.reset();
        state.pendingDocumentCloseAction.reset();
        finishPendingDocumentClose(state, id, action);
    }
}

void syncProviders(WorkbenchState & state);

void processDocumentIntents(WorkbenchState & state)
{
    for (auto & document : state.session.documentsMutable())
    {
        processDocumentIntent(state, document);
    }
    state.session.syncAssetDocuments();
    syncProviders(state);
}

void applyDeferredWorkflowActions(WorkbenchState & state)
{
    while (!state.deferredWorkflowActions.empty())
    {
        const auto actions = std::move(state.deferredWorkflowActions);
        for (const auto & action : actions)
        {
            action();
        }
    }
}

[[nodiscard]] DocumentRecord * findMutableDocument(WorkbenchState & state, DocumentId id)
{
    for (auto & candidate : state.session.documentsMutable())
    {
        if (candidate.summary.identity.id == id)
        {
            return &candidate;
        }
    }
    return nullptr;
}

[[nodiscard]] std::string pendingDocumentFilename(const DocumentRecord * record)
{
    if (record != nullptr && record->radarcol)
    {
        const std::string filename = record->radarcol->sourcePath().filename().string();
        return filename.empty() ? record->summary.displayName : filename;
    }
    if (record != nullptr && record->texmaps)
    {
        const std::string filename = record->texmaps->sourceDataPath().filename().string();
        return filename.empty() ? record->summary.displayName : filename;
    }
    if (record != nullptr && record->map)
    {
        const std::string filename = record->map->sourcePath.filename().string();
        return filename.empty() ? record->summary.displayName : filename;
    }
    return "document";
}

void closePendingDocument(WorkbenchState & state, DocumentId id)
{
    const auto closed = state.session.closeDocument(id, state.mapDocuments);
    if (!closed)
    {
        state.notifications.add(NotificationSeverity::error, closed.error().message);
    }
    else
    {
        evictClosedDocumentState(state, id);
        continueQuit(state);
    }
}

void rearmPendingDocumentClose(WorkbenchState & state, DocumentId id)
{
    state.pendingDocumentCloseAction.reset();
    static_cast<void>(state.operationController.cancel());
    OperationRequest request;
    request.kind = OperationKind::close;
    request.documentId = id;
    const auto transition = state.operationController.begin(request, true, false, OutputCollisionPolicy::confirm);
    if (transition.kind != OperationTransitionKind::showDirtyDecision)
    {
        state.pendingDocumentCloseDecision.reset();
    }
}

[[nodiscard]] bool resolvePendingDocumentOperation(WorkbenchState & state, PendingChangesAction action)
{
    const auto transition = state.operationController.resolveDirty(action);
    if (transition.kind == OperationTransitionKind::rejected)
    {
        state.notifications.add(NotificationSeverity::error, transition.error);
        return false;
    }
    return true;
}

void requestMapDocumentResolution(WorkbenchState & state, const DocumentRecord & record, PendingChangesAction action)
{
    if (!resolvePendingDocumentOperation(state, action))
    {
        return;
    }

    if (action == PendingChangesAction::save)
    {
        const auto saved = state.mapDocuments.save(state.mapDocuments.active().changes().originalPath, false);
        if (!saved)
        {
            static_cast<void>(state.operationController.completeSave(false));
            state.notifications.add(NotificationSeverity::error, saved.error().message);
            rearmPendingDocumentClose(state, record.summary.identity.id);
            return;
        }
        const auto completed = state.operationController.completeSave(true);
        if (completed.kind != OperationTransitionKind::execute)
        {
            state.notifications.add(NotificationSeverity::error, completed.error);
            rearmPendingDocumentClose(state, record.summary.identity.id);
            return;
        }
    }
    else
    {
        const auto discarded = state.mapDocuments.discard();
        if (!discarded)
        {
            state.notifications.add(NotificationSeverity::error, discarded.error().message);
            rearmPendingDocumentClose(state, record.summary.identity.id);
            return;
        }
    }

    const DocumentId id = record.summary.identity.id;
    state.pendingDocumentCloseDecision.reset();
    state.pendingDocumentCloseAction.reset();
    closePendingDocument(state, id);
}

void requestRadarcolDocumentResolution(WorkbenchState & state, RadarcolDocument & document, PendingChangesAction action)
{
    if (!resolvePendingDocumentOperation(state, action))
    {
        return;
    }
    document.requestIntent({
        .kind =
            action == PendingChangesAction::discard ? RadarcolDocumentIntentKind::discard : RadarcolDocumentIntentKind::save,
        .colorEdits = {},
        .outputPath = {},
        .overwriteOutput = false,
    });
    state.pendingDocumentCloseAction = action;
}

void requestTexmapsDocumentResolution(WorkbenchState & state, TexmapsDocument & document, PendingChangesAction action)
{
    if (!resolvePendingDocumentOperation(state, action))
    {
        return;
    }
    document.requestIntent({
        .kind = action == PendingChangesAction::discard ? TexmapsDocumentIntentKind::discard : TexmapsDocumentIntentKind::save,
        .outputIndexPath = {},
        .outputDataPath = {},
        .overwriteOutput = false,
        .textureId = 0,
        .texture = uomul::TextureImage(),
        .textureEdits = {},
        .textureIds = {},
    });
    state.pendingDocumentCloseAction = action;
}

void requestPendingDocumentResolution(WorkbenchState & state, DocumentRecord * record, PendingChangesAction action)
{
    if (record == nullptr)
    {
        return;
    }
    if (record->map)
    {
        requestMapDocumentResolution(state, *record, action);
        return;
    }
    if (record->radarcol)
    {
        requestRadarcolDocumentResolution(state, record->radarcol.value(), action);
        return;
    }
    if (record->texmaps)
    {
        requestTexmapsDocumentResolution(state, record->texmaps.value(), action);
    }
}

void drawPendingDocumentCloseDialog(WorkbenchState & state)
{
    if (!state.pendingDocumentCloseDecision)
    {
        return;
    }
    const auto id = *state.pendingDocumentCloseDecision;
    const DocumentRecord * record = findMutableDocument(state, id);
    DocumentCloseDialogState dialog{
        .visible = true,
        .filename = pendingDocumentFilename(record),
        .actionRequested = [&state, id](PendingChangesAction action)
        { requestPendingDocumentResolution(state, findMutableDocument(state, id), action); },
        .cancelRequested =
            [&state]
        {
            state.pendingDocumentCloseDecision.reset();
            state.pendingDocumentCloseAction.reset();
            state.quitRequested = false;
            static_cast<void>(state.operationController.cancel());
        },
    };
    drawDocumentCloseDialog(dialog);
}

void requestDocumentClose(WorkbenchState & state, DocumentId id)
{
    state.documentLifecycle.requestClose({.id = id, .dirtyPolicy = DocumentCloseDirtyPolicy::prompt});
}

void applyPendingDocumentAction(WorkbenchState & state)
{
    const auto pendingClose = state.documentLifecycle.takeClose();
    if (pendingClose)
    {
        // A visible map queues activation while it is rendered. A close request
        // supersedes that stale activation request and must be applied first.
        static_cast<void>(state.documentLifecycle.takeActivation());
    }
    else if (const auto id = state.documentLifecycle.takeActivation())
    {
        activateDocumentOrNotify(state, *id);
        return;
    }
    else
    {
        return;
    }

    const DocumentCloseRequest request = *pendingClose;
    const DocumentId id = request.id;

    const DocumentRecord * requested = nullptr;
    for (const auto & candidate : state.session.documents())
    {
        if (candidate.summary.identity.id == id)
        {
            requested = &candidate;
            break;
        }
    }
    if (requested == nullptr)
    {
        return;
    }

    if (!state.quitRequested)
    {
        hideConfiguredMetadataView(state, requested->summary);
    }

    if (requested->summary.dirty)
    {
        const auto activated = state.session.activateDocument(id, state.mapDocuments);
        if (!activated)
        {
            state.notifications.add(NotificationSeverity::error, activated.error().message);
            return;
        }
        OperationRequest operationRequest;
        operationRequest.kind = OperationKind::close;
        operationRequest.documentId = id;
        const auto transition = state.operationController.begin(operationRequest, true, false, OutputCollisionPolicy::confirm);
        if (transition.kind == OperationTransitionKind::showDirtyDecision)
        {
            state.pendingDocumentCloseDecision = id;
        }
        return;
    }

    const auto closed = state.session.closeDocument(id, state.mapDocuments);
    if (!closed)
    {
        state.notifications.add(NotificationSeverity::error, closed.error().message);
    }
    else
    {
        evictClosedDocumentState(state, id);
        continueQuit(state);
    }
}

[[nodiscard]] bool dispatchToolShortcuts(WorkbenchState & state)
{
    for (const ToolShortcut & shortcut : toolShortcuts)
    {
        if (ImGui::GetIO().KeyShift != shortcut.shift || ImGui::GetIO().KeyCtrl || !ImGui::IsKeyPressed(shortcut.key, false))
        {
            continue;
        }
        return state.commands.executeShortcut(shortcut.shortcut, state);
    }
    return false;
}

[[nodiscard]] bool dispatchSelectionShortcuts(WorkbenchState & state)
{
    if (!ImGui::GetIO().KeyCtrl || !state.hoveredPanelContext.empty())
    {
        return false;
    }
    if (ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_A, false))
    {
        return state.commands.executeShortcut("Ctrl+Shift+A", state);
    }
    if (!ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_A, false))
    {
        return state.commands.executeShortcut("Ctrl+A", state);
    }
    return ImGui::IsKeyPressed(ImGuiKey_I, false) && state.commands.executeShortcut("Ctrl+I", state);
}

[[nodiscard]] bool dispatchGeneralShortcut(WorkbenchState & state, bool allowFullscreen)
{
    const std::array<std::pair<ImGuiKey, std::string_view>, shortcutCount> shortcuts{
        std::pair{ImGuiKey_GraveAccent, "`"},
        std::pair{ImGuiKey_S, "Ctrl+Shift+S"},
        std::pair{ImGuiKey_S, "Ctrl+S"},
        std::pair{ImGuiKey_O, "Ctrl+O"},
        std::pair{ImGuiKey_N, "Ctrl+N"},
        std::pair{ImGuiKey_Q, "Ctrl+Q"},
        std::pair{ImGuiKey_G, "Ctrl+G"},
        std::pair{ImGuiKey_F11, "F11"},
        std::pair{ImGuiKey_E, "Ctrl+E"},
        std::pair{ImGuiKey_C, "Ctrl+C"},
    };
    const auto execute = [&state, &shortcuts](std::size_t index)
    { return state.commands.executeShortcut(shortcuts.at(index).second, state); };
    if (ImGui::IsKeyPressed(shortcuts.at(graveShortcutIndex).first, false))
        return execute(graveShortcutIndex);
    if (ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyShift &&
        ImGui::IsKeyPressed(shortcuts.at(saveAsShortcutIndex).first, false))
        return execute(saveAsShortcutIndex);
    if (ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(shortcuts.at(saveShortcutIndex).first, false))
        return execute(saveShortcutIndex);
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(shortcuts.at(openShortcutIndex).first, false))
        return execute(openShortcutIndex);
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(shortcuts.at(newShortcutIndex).first, false))
        return execute(newShortcutIndex);
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(shortcuts.at(quitShortcutIndex).first, false))
        return execute(quitShortcutIndex);
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(shortcuts.at(goToShortcutIndex).first, false))
        return execute(goToShortcutIndex);
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(shortcuts.at(assetEditShortcutIndex).first, false))
        return execute(assetEditShortcutIndex);
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(shortcuts.at(assetCopyShortcutIndex).first, false))
        return execute(assetCopyShortcutIndex);
    return allowFullscreen && ImGui::IsKeyPressed(shortcuts.at(fullscreenShortcutIndex).first, false) &&
           execute(fullscreenShortcutIndex);
}

void dispatchShortcuts(WorkbenchState & state, bool allowFullscreen)
{
    if (ImGui::GetIO().WantTextInput || ImGui::GetIO().WantCaptureKeyboard)
    {
        return;
    }
    if (dispatchToolShortcuts(state) || dispatchSelectionShortcuts(state))
        return;
    static_cast<void>(dispatchGeneralShortcut(state, allowFullscreen));
}

void beginGuiFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

[[nodiscard]] GLFWcursor * transparentCursor()
{
    static GLFWcursor * cursor = []
    {
        static std::array<unsigned char, 4> pixels{0U, 0U, 0U, 0U};
        const GLFWimage image{.width = 1, .height = 1, .pixels = pixels.data()};
        return glfwCreateCursor(&image, 0, 0);
    }();
    return cursor;
}

[[nodiscard]] std::optional<IconId> toolCursorIcon(const WorkbenchState & state)
{
    if (state.toolState.tilePickRequested())
    {
        return IconId::eyedropper;
    }
    const MapCanvasViewState & canvasState = state.mapDocuments.canvasState();
    if (!canvasState.cursorHovered)
    {
        return std::nullopt;
    }
    if (canvasState.panning || state.toolState.activeTool() == ToolId::move)
    {
        return IconId::pan;
    }
    switch (state.toolState.activeTool())
    {
        case ToolId::rectangleSelect :
            return IconId::selection;
        case ToolId::lassoSelect :
            return IconId::lasso;
        case ToolId::fuzzySelect :
            return IconId::autoFix;
        case ToolId::fill :
            return IconId::formatColorFill;
        case ToolId::replace :
            return IconId::replace;
        case ToolId::move :
            return IconId::pan;
    }
    return std::nullopt;
}

void drawToolCursor(GLFWwindow * window, const WorkbenchState & state)
{
    static bool customCursorActive = false;
    const std::optional<IconId> icon = toolCursorIcon(state);
    if (!icon.has_value())
    {
        if (customCursorActive)
        {
            glfwSetCursor(window, nullptr);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            customCursorActive = false;
        }
        return;
    }
    if (!iconFontAvailable())
    {
        if (customCursorActive)
        {
            glfwSetCursor(window, nullptr);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            customCursorActive = false;
        }
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        return;
    }

    if (GLFWcursor * cursor = transparentCursor(); cursor != nullptr)
    {
        glfwSetCursor(window, cursor);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        customCursorActive = true;
    }
    else
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        customCursorActive = true;
    }
    ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    const std::string glyph = iconGlyph(*icon);
    const ImVec2 mousePosition = ImGui::GetIO().MousePos;
    ImGui::GetForegroundDrawList()->AddText(ImGui::GetFont(),
                                            ImGui::GetFontSize(),
                                            ImVec2(mousePosition.x + 1.0F, mousePosition.y + 1.0F),
                                            ImGui::GetColorU32(ImGuiCol_Text),
                                            glyph.c_str());
}

void applyPendingClipboard(GLFWwindow * window, WorkbenchState & state)
{
    if (state.pendingClipboardText.empty())
    {
        return;
    }

    glfwSetClipboardString(window, state.pendingClipboardText.c_str());
    state.pendingClipboardText.clear();
}

void applyProfileChange(WorkbenchState & state)
{
    state.preferences.activeDataProfile = state.activeProfile.name;
    state.session.updateProfile(state.activeProfile);
    state.assetStateRestored = false;
    if (state.session.mapReady() && !state.preferences.showWelcomeOnStartup)
    {
        state.session.setStartupState(StartupState::workbench);
    }
    if (!state.mapDocuments.active().changes().reader.has_value())
    {
        state.welcomeGreetingSelected = false;
        state.welcomeGreeting.clear();
    }
}

void syncProviders(WorkbenchState & state)
{
    state.providers.sync(state.activeProfile);
    state.providers.syncDocuments(state.session.documents());
    const ProviderRevisions revisions = state.providers.revisions();
    if (revisions == state.providerRevisions)
    {
        return;
    }

    state.providerRevisions = revisions;
    state.mapDocuments.invalidateProviderCaches();
    assets::resetContext(state.radarcolState.assetView);
    assets::resetContext(state.texmapsState.assetView);
    assets::resetContext(state.tiledataState.assetView);
    for (auto & [id, assetView] : state.radarcolState.documentAssetViews)
    {
        static_cast<void>(id);
        assets::resetContext(assetView);
    }
    for (auto & [id, assetView] : state.texmapsState.documentAssetViews)
    {
        static_cast<void>(id);
        assets::resetContext(assetView);
    }
    for (auto & [id, assetView] : state.tiledataState.documentAssetViews)
    {
        static_cast<void>(id);
        assets::resetContext(assetView);
    }
}

[[nodiscard]] assets::AssetSourceContext
metadataContext(DocumentKind kind, const DocumentSourceIdentity & source, std::string contextId)
{
    return {
        .kind = kind,
        .contextKind = assets::AssetSourceContext::Kind::metadataView,
        .source = source,
        .contextId = std::move(contextId),
    };
}

void restoreMetadataAssetState(WorkbenchState & state)
{
    if (state.assetStateRestored)
    {
        return;
    }
    const auto radarContext =
        metadataContext(DocumentKind::radarcol, state.providers.radarColors().status().sourceIdentity, "window.radarcol");
    const auto texmapContext =
        metadataContext(DocumentKind::texmaps, state.providers.textures().status().sourceIdentity, "window.texmaps");
    const auto tiledataContext =
        metadataContext(DocumentKind::tiledata, state.providers.tiledata().status().sourceIdentity, "window.tiledata");
    if (const auto * record =
            assets::findAssetState(state.assetStateRecords, state.activeProfile.name, "default", radarContext);
        record != nullptr)
    {
        assets::restoreAssetState(*record, state.radarcolState.assetView);
    }
    if (const auto * record =
            assets::findAssetState(state.assetStateRecords, state.activeProfile.name, "default", texmapContext);
        record != nullptr)
    {
        assets::restoreAssetState(*record, state.texmapsState.assetView);
    }
    if (const auto * record =
            assets::findAssetState(state.assetStateRecords, state.activeProfile.name, "default", tiledataContext);
        record != nullptr)
    {
        assets::restoreAssetState(*record, state.tiledataState.assetView);
    }
    state.assetStateRestored = true;
}

void restoreConfiguredMetadataDocumentAssetState(const WorkbenchState & state,
                                                 const DocumentRecord & document,
                                                 assets::AssetWindowState & assetView)
{
    if (document.summary.sourceKind != DocumentSourceKind::configuredMetadata)
    {
        return;
    }

    const auto context = metadataContext(document.summary.identity.kind,
                                         document.summary.identity.source,
                                         std::string(metadataContextId(document.summary.identity.kind)));
    if (const auto * record = assets::findAssetState(state.assetStateRecords, state.activeProfile.name, "default", context);
        record != nullptr)
    {
        assets::restoreAssetState(*record, assetView);
    }
}

void updateAssetStateRecord(std::vector<assets::AssetStateRecord> & records, assets::AssetStateRecord record)
{
    const auto existing = std::ranges::find_if(records,
                                               [&record](const assets::AssetStateRecord & candidate)
                                               {
                                                   return candidate.profileIdentity == record.profileIdentity &&
                                                          candidate.assetWindowIdentity == record.assetWindowIdentity &&
                                                          candidate.context == record.context;
                                               });
    if (existing == records.end())
    {
        records.push_back(std::move(record));
    }
    else
    {
        *existing = std::move(record);
    }
}

template <typename DocumentStates>
void captureConfiguredMetadataAssetState(WorkbenchState & state, DocumentKind kind, const DocumentStates & documentStates)
{
    const std::string contextId(metadataContextId(kind));
    for (const auto & document : state.session.documents())
    {
        if (document.summary.identity.kind != kind || document.summary.sourceKind != DocumentSourceKind::configuredMetadata)
        {
            continue;
        }
        const auto assetView = documentStates.find(document.summary.identity.id);
        if (assetView == documentStates.end())
        {
            continue;
        }
        updateAssetStateRecord(state.assetStateRecords,
                               assets::captureAssetState(state.activeProfile.name,
                                                         "default",
                                                         metadataContext(kind, document.summary.identity.source, contextId),
                                                         assetView->second));
    }
}

void evictClosedDocumentState(WorkbenchState & state, DocumentId id)
{
    state.radarcolState.documentAssetViews.erase(id);
    state.radarcolState.documentBrowserKinds.erase(id);
    state.texmapsState.documentAssetViews.erase(id);
    state.texmapsState.documentTexturePreviews.erase(id);
    state.texmapsState.documentTexturePreviewRevisions.erase(id);
    state.tiledataState.documentAssetViews.erase(id);
}

void persistPreferenceChanges(WorkbenchState & state)
{
    try
    {
        if (!state.activeProfile.name.empty())
        {
            DataProfile profile = state.activeProfile;
            profile.preferences = state.preferences;
            saveProfileLayout(profile.layout, state.panelRegistry, state.windowRegistry, state.layout);
            state.profileStore.save(profile);
            state.activeProfile = std::move(profile);
        }

        Preferences globalPreferences;
        globalPreferences.activeDataProfile = state.activeProfile.name;
        savePreferences(globalPreferences);
        state.persistedPreferences = state.preferences;
    }
    catch (const std::exception & error)
    {
        state.notifications.add(NotificationSeverity::error, "Preferences save failed: " + std::string(error.what()));
    }
}

void applyPendingPersistence(WorkbenchState & state)
{
    // Tool state is edited directly through the palette and is persisted in the
    // regular preferences record. Keep that record synchronized before the
    // change check below so tool changes are saved during the session too.
    state.toolState.savePreferences(state.preferences.tools);

    if (state.menuBarState.preferencesDialog.profileDeleteRequested)
    {
        const std::string profileName = *state.menuBarState.preferencesDialog.profileDeleteRequested;
        try
        {
            state.profileStore.remove(profileName);
            state.menuBarState.preferencesDialog.profileDeleteRequested.reset();
            state.menuBarState.preferencesDialog.profilePersistenceOperation = ProfilePersistenceOperation::none;
            state.menuBarState.preferencesDialog.profilePersistenceError.reset();
            if (state.activeProfile.name == profileName)
            {
                state.activeProfile = {};
                state.preferences = {};
                state.toolState.loadPreferences(state.preferences.tools);
                state.panelRegistry.resetToDefaults();
                state.windowRegistry.resetToDefaults();
                state.layout = {};
                state.workbenchLayoutApplied = false;
                applyProfileChange(state);
            }
            requestPreferencesDialog(state.menuBarState.preferencesDialog, state.activeProfile, state.preferences);
        }
        catch (const std::exception & error)
        {
            state.menuBarState.preferencesDialog.profilePersistenceOperation = ProfilePersistenceOperation::remove;
            state.menuBarState.preferencesDialog.profilePersistenceError =
                "Profile deletion failed: " + std::string(error.what());
            state.menuBarState.preferencesDialog.visible = true;
            activatePreferencesDialog(state.menuBarState.preferencesDialog);
        }
    }
    if (state.menuBarState.preferencesDialog.profileSaveRequested)
    {
        try
        {
            DataProfile profile = state.menuBarState.preferencesDialog.workingProfile;
            if (profile.name == state.activeProfile.name)
            {
                profile.preferences = state.preferences;
                saveProfileLayout(profile.layout, state.panelRegistry, state.windowRegistry, state.layout);
            }
            state.profileStore.save(profile);
            if (profile.name == state.activeProfile.name)
            {
                state.activeProfile = std::move(profile);
            }
            state.menuBarState.preferencesDialog.profileSaveRequested = false;
            state.menuBarState.preferencesDialog.profilePersistenceOperation = ProfilePersistenceOperation::none;
            state.menuBarState.preferencesDialog.profilePersistenceError.reset();
        }
        catch (const std::exception & error)
        {
            state.menuBarState.preferencesDialog.profileSaveRequested = false;
            state.menuBarState.preferencesDialog.profilePersistenceOperation = ProfilePersistenceOperation::save;
            state.menuBarState.preferencesDialog.profilePersistenceError = "Profile save failed: " + std::string(error.what());
            state.menuBarState.preferencesDialog.visible = true;
            activatePreferencesDialog(state.menuBarState.preferencesDialog);
        }
    }
    if (state.preferences != state.persistedPreferences)
    {
        persistPreferenceChanges(state);
    }
    if (state.assetStateResetRequested)
    {
        assets::resetAssetState(state.assetStateRecords);
        assets::resetContext(state.radarcolState.assetView);
        assets::resetContext(state.texmapsState.assetView);
        assets::resetContext(state.tiledataState.assetView);
        for (auto & [id, assetView] : state.radarcolState.documentAssetViews)
        {
            static_cast<void>(id);
            assets::resetContext(assetView);
        }
        for (auto & [id, assetView] : state.texmapsState.documentAssetViews)
        {
            static_cast<void>(id);
            assets::resetContext(assetView);
        }
        for (auto & [id, assetView] : state.tiledataState.documentAssetViews)
        {
            static_cast<void>(id);
            assets::resetContext(assetView);
        }
        state.assetStateResetRequested = false;
        state.assetStateRestored = true;
    }

    captureConfiguredMetadataAssetState(state, DocumentKind::radarcol, state.radarcolState.documentAssetViews);
    captureConfiguredMetadataAssetState(state, DocumentKind::texmaps, state.texmapsState.documentAssetViews);
    captureConfiguredMetadataAssetState(state, DocumentKind::tiledata, state.tiledataState.documentAssetViews);
}

[[nodiscard]] bool welcomeShell(const WorkbenchState & state) noexcept
{
    return state.session.startupState() == StartupState::mapWelcome &&
           !state.mapDocuments.active().changes().reader.has_value() && !state.session.hasDocuments();
}

void updateNativeWindowMode(GLFWwindow * window, WorkbenchState & state, bool welcome)
{
    if (state.nativeWelcomeWindowMode == welcome)
    {
        return;
    }

    const bool leavingWelcome = state.nativeWelcomeWindowMode && !welcome;
    if (welcome && state.workbenchLayoutApplied && glfwGetWindowMonitor(window) == nullptr)
    {
        state.preferences.maximized = glfwGetWindowAttrib(window, GLFW_MAXIMIZED) == GLFW_TRUE;
    }
    glfwSetWindowAttrib(window, GLFW_DECORATED, welcome ? GLFW_FALSE : GLFW_TRUE);
    glfwSetWindowAttrib(window, GLFW_RESIZABLE, welcome ? GLFW_FALSE : GLFW_TRUE);
    if (welcome)
    {
        if (glfwGetWindowMonitor(window) != nullptr)
        {
            glfwSetWindowMonitor(window, nullptr, 0, 0, welcomeWindowWidth, welcomeWindowHeight, 0);
        }
        else
        {
            glfwRestoreWindow(window);
            glfwSetWindowSize(window, welcomeWindowWidth, welcomeWindowHeight);
        }
    }
    if (leavingWelcome)
    {
        state.workbenchLayoutApplied = false;
        static_cast<void>(applyWorkbenchLayout(window, state));
    }
    state.nativeWelcomeWindowMode = welcome;
}

[[nodiscard]] bool drawWelcomeOrWorkbench(logger::GuiLogSink & logSink, WorkbenchState & state, bool welcome)
{
    bool profileChanged = false;
    const float bottomPanelHeight =
        welcome ? 0.0F
                : drawLogPanel(logSink, state.preferences, state.logEntries, state.followLogScroll, state.logPanelRequest);
    if (state.session.startupState() == StartupState::configureProfile)
    {
        drawStartupScreen(state);
    }
    else if (welcome)
    {
        state.recentMapRecordedPath.clear();
        drawWelcomeView(state);
        notifyRadarcolFallback(state);
        drawRadarcolFallbackWorkflow(state);
        profileChanged = drawMenuBar(state, false);
    }
    else
    {
        state.welcomeGreetingSelected = false;
        state.welcomeGreeting.clear();
        state.hoveredPanelContext.clear();
        const auto updateHoveredPanelContext = [&state](std::string_view context)
        {
            state.hoveredPanelContext.assign(context);
            state.activeAssetPanelContext.assign(context);
        };
        state.radarcolState.hoveredContext = updateHoveredPanelContext;
        state.texmapsState.hoveredContext = updateHoveredPanelContext;
        state.tiledataState.hoveredContext = updateHoveredPanelContext;
        if (!state.quitRequested)
        {
            ensureConfiguredMetadataWindows(state);
        }
        drawDockSpace(state, bottomPanelHeight);
        drawDocumentSurface(state);
        drawPendingDocumentCloseDialog(state);
        drawMapPanels(state, state.blockInspectorState);
        notifyRadarcolFallback(state);
        drawRadarcolFallbackWorkflow(state);
        if (state.mapDocuments.active().changes().reader.has_value() &&
            state.recentMapRecordedPath != state.mapDocuments.active().changes().mapPath)
        {
            state.session.recordRecentFile(state.mapDocuments.active().changes().mapPath,
                                           state.mapDocuments.active().changes().dimensions);
            state.recentMapRecordedPath = state.mapDocuments.active().changes().mapPath;
        }
        applyPendingDocumentAction(state);
        processDocumentIntents(state);
    }
    if (!welcome)
    {
        drawStatusBar(state, bottomPanelHeight);
    }
    return profileChanged;
}

} // namespace

void drawWorkbench(GLFWwindow * window, logger::GuiLogSink & logSink, WorkbenchState & state)
{
    beginGuiFrame();
    bool welcome = welcomeShell(state);
    const bool profileChanged = welcome ? false : drawMenuBar(state, true);
    applyPendingClipboard(window, state);
    state.session.syncDocument(state.mapDocuments);
    if (state.quitRequested && !state.mapDocuments.active().changes().reader &&
        state.menuBarState.fileDialog.mode == MapFileDialogMode::none && !state.pendingDocumentCloseDecision)
    {
        continueQuit(state);
    }

    const bool wasWelcome = welcome;
    welcome = welcomeShell(state);
    syncProviders(state);
    restoreMetadataAssetState(state);
    if (welcome)
    {
        state.session.refreshRecentFiles();
    }
    updateNativeWindowMode(window, state, welcome);
    const bool enteredWorkbench = wasWelcome && !welcome;
    if (!welcome)
    {
        if (enteredWorkbench || !state.workbenchLayoutApplied)
        {
            static_cast<void>(applyWorkbenchLayout(window, state));
        }
        toggleFullscreen(window, state);
    }
    dispatchShortcuts(state, !welcome);
    const bool welcomeProfileChanged = drawWelcomeOrWorkbench(logSink, state, welcome);
    applyDeferredWorkflowActions(state);
    applyLogPanelRequest(state, logSink);
    const bool preferencesProfileChanged = applyPendingPreferencesDialog(window, state);
    if (profileChanged || welcomeProfileChanged || preferencesProfileChanged)
    {
        applyProfileChange(state);
    }
    applyPendingPreferenceChanges(state);
    applyPendingPersistence(state);
    drawToolCursor(window, state);
}

} // namespace uo_mule::gui
