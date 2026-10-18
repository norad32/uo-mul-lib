#include "gui/map_workspace.h"
#include "gui/data_profile.h"
#include "gui/panels/texmaps_panel.h"
#include "gui/theme.h"
#include "uomul/texmaps/texmaps.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <imgui.h>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace uo_mule::gui
{

namespace
{

constexpr std::uint32_t texmapsTexturesPerPage = static_cast<std::uint32_t>(texmapsPageTextureCount);
constexpr float texmapsThumbnailSize = 64.0F;
constexpr float texmapsBorderThickness = 2.0F;
constexpr int texmapsPageStep = 1;
constexpr float previousButtonWidth = 100.0F;
constexpr float nextButtonWidth = 100.0F;
constexpr float firstLastButtonWidth = 50.0F;

[[nodiscard]] std::uint32_t pageTextureBase(const TexmapsPanelState & state) noexcept
{
    return state.page * texmapsTexturesPerPage;
}

void updatePageCount(TexmapsPanelState & state) noexcept
{
    if (state.reader)
    {
        const std::uint32_t textureCount = state.reader->textureCount();
        state.pageCount = (textureCount + texmapsTexturesPerPage - 1U) / texmapsTexturesPerPage;
        const int maximumPage = state.pageCount == 0U ? 0 : static_cast<int>(state.pageCount - 1U);
        state.page = static_cast<std::uint32_t>(std::clamp(static_cast<int>(state.page), 0, maximumPage));
    }
    else
    {
        state.pageCount = 0;
    }
}

[[nodiscard]] MapColor convert16ToRgb(std::uint16_t pixel) noexcept
{
    // Texture format: XRRRRRGGGGGBBBBB (5-5-5 RGB with unused high bit, shifted left)
    constexpr std::uint32_t redShift = 10;
    constexpr std::uint32_t greenShift = 5;
    constexpr std::uint32_t channelMask = 0x1FU;
    constexpr std::uint32_t maxByteValue = 255U;
    constexpr std::uint32_t max5BitValue = 31U;
    constexpr std::uint32_t roundingValue = 15U;

    const std::uint32_t pixelValue = static_cast<std::uint32_t>(pixel);
    const std::uint32_t red = ((pixelValue >> redShift) & channelMask);
    const std::uint32_t green = ((pixelValue >> greenShift) & channelMask);
    const std::uint32_t blue = (pixelValue & channelMask);

    // Scale 5-bit values to 8-bit (same as radarcol)
    const auto scale5to8 = [](std::uint32_t value) -> std::uint8_t {
        return static_cast<std::uint8_t>(((value * maxByteValue) + roundingValue) / max5BitValue);
    };

    return {.red = scale5to8(red), .green = scale5to8(green), .blue = scale5to8(blue)};
}

[[nodiscard]] TexmapsTexturePreview convertTextureToPreview(const uomul::TextureImage & texture) noexcept
{
    TexmapsTexturePreview preview;
    preview.width = texture.dimension;
    preview.height = texture.dimension;
    preview.pixels.reserve(texture.pixels.size());

    for (const auto pixel : texture.pixels)
    {
        preview.pixels.push_back(convert16ToRgb(pixel));
    }

    return preview;
}

void loadPage(TexmapsPanelState & state)
{
    if (!state.reader)
    {
        return;
    }

    const std::uint32_t pageIndex = state.page;
    if (state.loadedPageIndex == pageIndex)
    {
        return;
    }

    state.pageTextures.clear();
    state.error.clear();

    const std::uint32_t baseId = pageTextureBase(state);
    const std::uint32_t endId = std::min(baseId + texmapsTexturesPerPage, state.reader->textureCount());

    for (std::uint32_t id = baseId; id < endId; id++)
    {
        const auto textureResult = state.reader->readTexture(id);
        if (textureResult)
        {
            state.pageTextures.push_back(convertTextureToPreview(*textureResult));
        }
        else
        {
            // Empty texture - create a placeholder
            TexmapsTexturePreview emptyPreview;
            emptyPreview.width = uomul::textureSmallDimension;
            emptyPreview.height = uomul::textureSmallDimension;
            state.pageTextures.push_back(emptyPreview);
        }
    }

    state.loadedPageIndex = pageIndex;
}

void loadSource(TexmapsPanelState & state)
{
    state.error.clear();
    state.reader.reset();
    state.pageTextures.clear();
    state.loadedPageIndex = unloadedTexmapsPage;

    const std::string indexPath(state.sourceIndexPath.data());
    const std::string dataPath(state.sourceDataPath.data());

    if (indexPath.empty() || dataPath.empty())
    {
        return;
    }

    auto readerResult = uomul::TexMapsReader::open(indexPath, dataPath);
    if (!readerResult)
    {
        state.error = readerResult.error().message;
        return;
    }

    state.reader = std::move(readerResult.value());
    updatePageCount(state);
    loadPage(state);
}

void drawTextureThumbnail(const TexmapsTexturePreview & texture, std::uint32_t textureId, bool selected)
{
    const ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    const ImVec2 cellMin = cursorPos;
    const ImVec2 cellMax(cellMin.x + texmapsThumbnailSize, cellMin.y + texmapsThumbnailSize);

    ImDrawList * drawList = ImGui::GetWindowDrawList();

    // Draw checkerboard background
    constexpr float checkerSize = 4.0F;
    constexpr ImU32 checkerColor1 = IM_COL32(64, 64, 64, 255);
    constexpr ImU32 checkerColor2 = IM_COL32(128, 128, 128, 255);

    for (int checkerY = 0; (static_cast<float>(checkerY) * checkerSize) < texmapsThumbnailSize; checkerY++)
    {
        for (int checkerX = 0; (static_cast<float>(checkerX) * checkerSize) < texmapsThumbnailSize; checkerX++)
        {
            const bool oddX = checkerX % 2 == 1;
            const bool oddY = checkerY % 2 == 1;
            const ImU32 color = (oddX != oddY) ? checkerColor1 : checkerColor2;

            const ImVec2 checkerMin(cellMin.x + (static_cast<float>(checkerX) * checkerSize),
                                     cellMin.y + (static_cast<float>(checkerY) * checkerSize));
            const ImVec2 checkerMax(checkerMin.x + checkerSize, checkerMin.y + checkerSize);
            drawList->AddRectFilled(checkerMin, checkerMax, color);
        }
    }

    // Draw texture
    if (!texture.pixels.empty() && texture.width > 0 && texture.height > 0)
    {
        const float scaleX = texmapsThumbnailSize / static_cast<float>(texture.width);
        const float scaleY = texmapsThumbnailSize / static_cast<float>(texture.height);

        for (std::uint32_t ty = 0; ty < texture.height; ty++)
        {
            for (std::uint32_t tx = 0; tx < texture.width; tx++)
            {
                const std::size_t pixelIndex = (ty * texture.width) + tx;
                if (pixelIndex < texture.pixels.size())
                {
                    const MapColor & color = texture.pixels.at(pixelIndex);
                    const ImU32 imColor = IM_COL32(color.red, color.green, color.blue, 255);

                    const ImVec2 pixelMin(cellMin.x + (static_cast<float>(tx) * scaleX),
                                           cellMin.y + (static_cast<float>(ty) * scaleY));
                    const ImVec2 pixelMax(cellMin.x + (static_cast<float>(tx + 1) * scaleX),
                                           cellMin.y + (static_cast<float>(ty + 1) * scaleY));

                    drawList->AddRectFilled(pixelMin, pixelMax, imColor);
                }
            }
        }
    }

    // Draw border
    const ImU32 borderColor =
        selected ? theme::packedColor(theme::brightBrass) : theme::packedColor(theme::stoneBorder);
    drawList->AddRect(cellMin, cellMax, borderColor, 0.0F, ImDrawFlags_None, texmapsBorderThickness);

    // Add text label with texture ID
    const std::string label = std::to_string(textureId);
    const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
    const ImVec2 textPos(cellMin.x + ((texmapsThumbnailSize - textSize.x) * 0.5F),
                         cellMin.y + ((texmapsThumbnailSize - textSize.y) * 0.5F));
    drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), label.c_str());

    ImGui::Dummy(ImVec2(texmapsThumbnailSize, texmapsThumbnailSize));
}

void drawTextureGrid(TexmapsPanelState & state)
{
    if (state.pageTextures.empty())
    {
        ImGui::TextUnformatted("No textures to display");
        return;
    }

    const ImVec2 availableSize = ImGui::GetContentRegionAvail();
    const float itemSpacing = ImGui::GetStyle().ItemSpacing.x;
    const int columnsCount = static_cast<int>((availableSize.x / (texmapsThumbnailSize + itemSpacing)));

    if (ImGui::BeginTable("TextureGrid", std::max(1, columnsCount), ImGuiTableFlags_PadOuterX))
    {
        const std::uint32_t baseId = pageTextureBase(state);

        for (std::size_t i = 0; i < state.pageTextures.size(); i++)
        {
            ImGui::TableNextColumn();

            const std::uint32_t textureId = baseId + static_cast<std::uint32_t>(i);
            const bool selected = (textureId == state.selectedTextureId);

            drawTextureThumbnail(state.pageTextures.at(i), textureId, selected);

            if (ImGui::IsItemClicked())
            {
                state.selectedTextureId = textureId;
                state.previewVisible = true;
            }

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Texture ID: %u", textureId);
            }
        }

        ImGui::EndTable();
    }
}

} // namespace

std::optional<TexmapsTexturePreview> texmapsTexturePreview(TexmapsPanelState & state, std::uint32_t id)
{
    if (!state.reader)
    {
        return std::nullopt;
    }

    const auto textureResult = state.reader->readTexture(id);
    if (!textureResult)
    {
        return std::nullopt;
    }

    return convertTextureToPreview(*textureResult);
}

void syncTexmapsPanel(TexmapsPanelState & state, const DataProfile & profile)
{
    const auto configuredIndex = resolvedDataFilePath(profile, DataFileRole::texidx);
    const auto configuredData = resolvedDataFilePath(profile, DataFileRole::texmaps);
    const std::string indexValue = configuredIndex.string();
    const std::string dataValue = configuredData.string();
    if (indexValue == state.sourceIndexPath.data() && dataValue == state.sourceDataPath.data())
    {
        return;
    }

    std::ranges::fill(state.sourceIndexPath, '\0');
    std::ranges::fill(state.sourceDataPath, '\0');
    const auto indexLength = std::min(indexValue.size(), state.sourceIndexPath.size() - 1U);
    const auto dataLength = std::min(dataValue.size(), state.sourceDataPath.size() - 1U);
    std::copy_n(indexValue.begin(), indexLength, state.sourceIndexPath.begin());
    std::copy_n(dataValue.begin(), dataLength, state.sourceDataPath.begin());
    loadSource(state);
}

void drawTexmapsPanel(TexmapsPanelState & state, bool & visible)
{
    const bool contentsVisible = ImGui::Begin("Texmaps", &visible);

    if (!contentsVisible)
    {
        ImGui::End();
        return;
    }

    // Source file selection
    ImGui::TextUnformatted("Source Index File:");
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputText("##TexmapsIndexPath", state.sourceIndexPath.data(), state.sourceIndexPath.size());

    ImGui::TextUnformatted("Source Data File:");
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputText("##TexmapsDataPath", state.sourceDataPath.data(), state.sourceDataPath.size());

    if (ImGui::Button("Load Textures"))
    {
        loadSource(state);
    }

    if (!state.error.empty())
    {
        ImGui::TextColored(ImVec4(1.0F, 0.0F, 0.0F, 1.0F), "Error: %s", state.error.c_str());
    }

    ImGui::Separator();

    if (state.reader)
    {
        ImGui::Text("Total Textures: %u", state.reader->textureCount());

        // Pagination controls
        if (ImGui::Button("< Previous", ImVec2(previousButtonWidth, 0)))
        {
            const int newPage = static_cast<int>(state.page) - texmapsPageStep;
            state.page = static_cast<std::uint32_t>(std::max(0, newPage));
            state.loadedPageIndex = unloadedTexmapsPage;
        }

        ImGui::SameLine();

        ImGui::Text("Page %u/%u", state.page + 1, state.pageCount);

        ImGui::SameLine();

        if (ImGui::Button("Next >", ImVec2(nextButtonWidth, 0)))
        {
            const int newPage = static_cast<int>(state.page) + texmapsPageStep;
            state.page = static_cast<std::uint32_t>(
                std::min(newPage, static_cast<int>(state.pageCount) - 1));
            state.loadedPageIndex = unloadedTexmapsPage;
        }

        ImGui::SameLine();

        if (ImGui::Button("First", ImVec2(firstLastButtonWidth, 0)))
        {
            state.page = 0;
            state.loadedPageIndex = unloadedTexmapsPage;
        }

        ImGui::SameLine();

        if (ImGui::Button("Last", ImVec2(firstLastButtonWidth, 0)))
        {
            state.page = state.pageCount > 0 ? state.pageCount - 1 : 0;
            state.loadedPageIndex = unloadedTexmapsPage;
        }

        ImGui::Separator();

        loadPage(state);
        drawTextureGrid(state);
    }
    else
    {
        ImGui::TextUnformatted("Load a texture pair to begin");
    }

    ImGui::End();
}

} // namespace uo_mule::gui
