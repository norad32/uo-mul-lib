#include "gui/panels/radarcol_panel.h"
#include "uomul/radarcol/radarcol.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <imgui.h>
#include <string>
#include <utility>

namespace uo_mule::gui
{

namespace
{

constexpr int radarcolGridColumns = 16;
constexpr int radarcolPageCount = static_cast<int>(uomul::radarMapColorCount / radarcolPageColorCount);
constexpr std::uint32_t radarcolColorsPerPage = static_cast<std::uint32_t>(radarcolPageColorCount);
[[nodiscard]] std::uint32_t pageCacheKey(const RadarcolPanelState & state) noexcept
{
    return state.page + (state.staticColors ? radarcolPageCount : 0U);
}

[[nodiscard]] std::uint32_t pageColorBase(const RadarcolPanelState & state) noexcept
{
    return state.staticColors ? uomul::radarStaticColorOffset + (state.page * radarcolColorsPerPage)
                              : state.page * radarcolColorsPerPage;
}

[[nodiscard]] std::uint32_t lastPageColorBase(const RadarcolPanelState & state) noexcept
{
    const std::uint32_t colorCount = state.reader ? state.reader->colorCount() : 0U;
    return colorCount > uomul::radarStaticColorOffset ? colorCount - uomul::radarStaticColorOffset : 0U;
}

void updatePageCount(RadarcolPanelState & state) noexcept
{
    if (state.staticColors && state.reader)
    {
        const std::uint32_t staticColors = lastPageColorBase(state);
        state.pageCount = staticColors == 0U ? 0U : (staticColors + radarcolColorsPerPage - 1U) / radarcolColorsPerPage;
        const int maximumPage = state.pageCount == 0U ? 0 : static_cast<int>(state.pageCount - 1U);
        state.page = static_cast<std::uint32_t>(std::clamp(static_cast<int>(state.page), 0, maximumPage));
    }
    else
    {
        state.pageCount = static_cast<std::uint32_t>(radarcolPageCount);
    }
}

constexpr float radarcolSwatchSize = 24.0F;
constexpr float radarcolChannelScale = 1.0F / 31.0F;
constexpr float radarcolByteScale = 255.0F;
constexpr std::uint32_t radarcolChannelMask = 0x1FU;
constexpr int radarcolPageStep = 1;
constexpr int radarcolPageFastStep = 8;
constexpr std::size_t radarcolRedChannelIndex = 0;
constexpr std::size_t radarcolGreenChannelIndex = 1;
constexpr std::size_t radarcolBlueChannelIndex = 2;

[[nodiscard]] ImVec4 colorToImVec4(uomul::RadarColor color) noexcept
{
    constexpr std::uint32_t redShift = 10;
    constexpr std::uint32_t greenShift = 5;
    const auto packed = static_cast<std::uint32_t>(color);
    return {static_cast<float>((packed >> redShift) & radarcolChannelMask) * radarcolChannelScale,
            static_cast<float>((packed >> greenShift) & radarcolChannelMask) * radarcolChannelScale,
            static_cast<float>(packed & radarcolChannelMask) * radarcolChannelScale,
            1.0F};
}

[[nodiscard]] uomul::RadarColor colorFromPicker(const std::array<float, 3> & color) noexcept
{
    const auto channel = [](float value)
    { return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * radarcolByteScale)); };
    return uomul::RadarColorConverter::fromRgb888(channel(color.at(radarcolRedChannelIndex)),
                                                  channel(color.at(radarcolGreenChannelIndex)),
                                                  channel(color.at(radarcolBlueChannelIndex)));
}

void setPath(std::array<char, radarcolPathBufferSize> & buffer, const std::filesystem::path & path)
{
    const std::string value = path.string();
    const std::size_t length = std::min(value.size(), buffer.size() - 1);
    std::memcpy(buffer.data(), value.data(), length);
    buffer.at(length) = '\0';
}

void loadPage(RadarcolPanelState & state)
{
    if (!state.reader)
    {
        return;
    }

    const std::uint32_t pageIndex = pageCacheKey(state);
    if (state.loadedPageIndex == pageIndex)
    {
        return;
    }

    const std::uint32_t firstColor = pageColorBase(state);
    for (std::size_t slot = 0; slot < radarcolPageColorCount; ++slot)
    {
        const std::uint32_t colorIndex = firstColor + static_cast<std::uint32_t>(slot);
        const auto color = state.reader->readRawColor(colorIndex);
        state.pageColorAvailable.at(slot) = color.has_value();
        state.pageColors.at(slot) = color.value_or(0);
    }

    state.loadedPageIndex = pageIndex;
}

void beginEditing(RadarcolPanelState & state, bool overwriteOutput);

void loadSource(RadarcolPanelState & state)
{
    auto reader = uomul::RadarColReader::open(state.sourcePath.data());
    if (!reader)
    {
        state.reader.reset();
        state.error =
            reader.error().path.empty() ? reader.error().message : reader.error().path.string() + ": " + reader.error().message;
        return;
    }

    state.reader.emplace(std::move(*reader));
    state.loadedPageIndex = unloadedRadarcolPage;
    state.error.clear();
}

void drawSourceControls(RadarcolPanelState & state)
{
    ImGui::BeginDisabled(state.editor.has_value());
    ImGui::InputText("Source", state.sourcePath.data(), state.sourcePath.size(), ImGuiInputTextFlags_ElideLeft);
    ImGui::SameLine();
    if (ImGui::Button("Load"))
    {
        loadSource(state);
    }
    ImGui::EndDisabled();
    if (state.reader && !state.editor)
    {
        ImGui::Text("Loaded: %s", state.sourcePath.data());
    }
}

void saveChanges(RadarcolPanelState & state);
void discardChanges(RadarcolPanelState & state);

void drawCategoryAndPageControls(RadarcolPanelState & state)
{
    ImGui::Separator();
    if (ImGui::RadioButton("Land", !state.staticColors))
    {
        state.staticColors = false;
        state.page = 0;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Static", state.staticColors))
    {
        state.staticColors = true;
        state.page = 0;
    }
    ImGui::SameLine();
    int page = static_cast<int>(state.page);
    ImGui::SetNextItemWidth(100.0F);
    const int maximumPage = state.staticColors ? static_cast<int>(state.pageCount) - 1 : radarcolPageCount - 1;
    if (ImGui::InputInt("Page", &page, radarcolPageStep, radarcolPageFastStep, ImGuiInputTextFlags_CharsDecimal))
    {
        state.page = static_cast<std::uint32_t>(std::clamp(page, 0, std::max(0, maximumPage)));
    }
    ImGui::SameLine();
    ImGui::Text("of %u", state.pageCount);
}

void drawEditControls(RadarcolPanelState & state, bool editMode)
{
    if (state.editor)
    {
        ImGui::Text("Editing: %s", state.outputPath.data());
    }

    if (state.reader && !state.editor && editMode)
    {
        ImGui::InputText("Output", state.outputPath.data(), state.outputPath.size(), ImGuiInputTextFlags_ElideLeft);
        ImGui::Checkbox("Overwrite output", &state.overwriteOutput);
        ImGui::SameLine();
        if (ImGui::Button("Begin editing"))
        {
            beginEditing(state, state.overwriteOutput);
        }
        return;
    }

    if (state.editor)
    {
        ImGui::TextUnformatted(editMode ? "Edit mode: ON" : "Edit mode is off; color changes are disabled.");
        ImGui::BeginDisabled(!editMode);
        if (ImGui::Button("Save changes"))
        {
            saveChanges(state);
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard changes"))
        {
            discardChanges(state);
        }
        ImGui::EndDisabled();
        return;
    }

    if (state.reader)
    {
        ImGui::TextUnformatted("Enable Edit mode to change colors.");
    }
}

void beginEditing(RadarcolPanelState & state, bool overwriteOutput)
{
    const std::filesystem::path source(state.sourcePath.data());
    const std::filesystem::path output(state.outputPath.data());
    auto editor = uomul::RadarColEditor::create(source, output, {.overwriteOutput = overwriteOutput});
    if (!editor)
    {
        state.error =
            editor.error().path.empty() ? editor.error().message : editor.error().path.string() + ": " + editor.error().message;
        return;
    }

    state.editor.emplace(std::move(*editor));
    state.error.clear();
}

void applyColor(RadarcolPanelState & state, std::uint32_t index, uomul::RadarColor color, std::size_t slot)
{
    if (!state.editor)
    {
        return;
    }

    auto edited = state.editor->editRawColor(index, color);
    if (!edited)
    {
        state.error =
            edited.error().path.empty() ? edited.error().message : edited.error().path.string() + ": " + edited.error().message;
        return;
    }

    state.pageColors.at(slot) = color;
    state.pageColorAvailable.at(slot) = true;
    state.error.clear();
}

void drawRadarcolColorItem(RadarcolPanelState & state, std::uint32_t index, std::size_t slot, bool editMode)
{
    ImGui::PushID(static_cast<int>(index));
    const ImVec4 color = colorToImVec4(state.pageColors.at(slot));
    if (ImGui::ColorButton("##RadarcolColor",
                           color,
                           ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
                           ImVec2(radarcolSwatchSize, radarcolSwatchSize)) &&
        editMode && state.editor)
    {
        state.pickerColor.at(radarcolRedChannelIndex) = color.x;
        state.pickerColor.at(radarcolGreenChannelIndex) = color.y;
        state.pickerColor.at(radarcolBlueChannelIndex) = color.z;
        ImGui::OpenPopup("RadarcolColorPicker");
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("ID %04X%s", index, state.pageColorAvailable.at(slot) ? "" : " (not in source file)");
    }

    if (editMode && state.editor && ImGui::BeginPopup("RadarcolColorPicker"))
    {
        ImGui::Text("Color %04X", index);
        if (ImGui::ColorPicker3("##RadarcolPicker", state.pickerColor.data()))
        {
            applyColor(state, index, colorFromPicker(state.pickerColor), slot);
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

void drawRadarcolColorTable(RadarcolPanelState & state, std::uint32_t firstColor, std::uint32_t availableColors, bool editMode)
{
    if (!ImGui::BeginTable("RadarcolColorGrid", radarcolGridColumns, ImGuiTableFlags_BordersInner))
    {
        return;
    }

    for (std::size_t slot = 0; slot < radarcolPageColorCount; ++slot)
    {
        const std::uint32_t index = firstColor + static_cast<std::uint32_t>(slot);
        ImGui::TableNextColumn();
        if (index < firstColor + availableColors)
        {
            drawRadarcolColorItem(state, index, slot, editMode);
        }
    }

    ImGui::EndTable();
}

void drawColorGrid(RadarcolPanelState & state, bool editMode)
{
    loadPage(state);
    if (!state.reader)
    {
        ImGui::TextUnformatted("Load a radarcol.mul file to view its colors.");
        return;
    }

    const std::uint32_t firstColor = pageColorBase(state);
    if (state.staticColors && state.reader->colorCount() <= uomul::radarStaticColorOffset)
    {
        ImGui::TextUnformatted("No static colors are available.");
        return;
    }
    const std::uint32_t availableColors = state.staticColors
                                              ? lastPageColorBase(state) - (state.page * radarcolColorsPerPage)
                                              : std::min(state.reader->colorCount(), uomul::radarMapColorCount) - firstColor;
    if (availableColors == 0U)
    {
        ImGui::TextUnformatted("No colors are available on this page.");
        return;
    }

    const std::uint32_t lastColor = firstColor + std::min(availableColors, radarcolColorsPerPage) - 1U;
    ImGui::Text("IDs %04X-%04X", firstColor, lastColor);
    drawRadarcolColorTable(state, firstColor, availableColors, editMode);
}

void saveChanges(RadarcolPanelState & state)
{
    if (!state.editor)
    {
        return;
    }

    const std::filesystem::path output(state.outputPath.data());
    auto committed = state.editor->commit();
    if (!committed)
    {
        state.error = committed.error().path.empty() ? committed.error().message
                                                     : committed.error().path.string() + ": " + committed.error().message;
        state.editor.reset();
        return;
    }

    state.editor.reset();
    setPath(state.sourcePath, output);
    auto reader = uomul::RadarColReader::open(output);
    if (!reader)
    {
        state.reader.reset();
        state.error =
            reader.error().path.empty() ? reader.error().message : reader.error().path.string() + ": " + reader.error().message;
        return;
    }

    state.reader.emplace(std::move(*reader));
    state.loadedPageIndex = unloadedRadarcolPage;
    state.error.clear();
}

void discardChanges(RadarcolPanelState & state)
{
    if (state.editor)
    {
        state.editor->abort();
        state.editor.reset();
    }
    state.error.clear();
}

} // namespace

void drawRadarcolPanel(RadarcolPanelState & state, bool editMode, bool & visible)
{
    if (!ImGui::Begin("Radarcol", &visible))
    {
        ImGui::End();
        return;
    }

    drawSourceControls(state);
    updatePageCount(state);
    if (state.reader)
    {
        ImGui::Text("Colors: %u", state.reader->colorCount());
        drawCategoryAndPageControls(state);
    }
    drawEditControls(state, editMode);

    if (!state.error.empty())
    {
        ImGui::TextWrapped("%s", state.error.c_str());
    }

    ImGui::Separator();
    drawColorGrid(state, editMode && state.editor.has_value());
    ImGui::End();
}

} // namespace uo_mule::gui
