#include "gui/panels/log_panel.h"
#include "gui/presentation/theme/theme.h"
#include "gui_model/persistence/preferences.h"
#include "logger/gui_log_sink.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <imgui.h>
#include <vector>

namespace uo_mule::gui
{

namespace
{

constexpr float minimumLogPanelHeight = 96.0F;
constexpr float logPanelHeightRatio = 0.33F;
constexpr float bottomScrollPosition = 1.0F;
constexpr std::size_t maxDisplayedEntries = logger::defaultGuiLogCapacity;

void appendEntries(std::vector<logger::GuiLogEntry> & entries, const std::vector<logger::GuiLogEntry> & newEntries)
{
    for (const auto & entry : newEntries)
    {
        if (entries.size() == maxDisplayedEntries)
        {
            entries.erase(entries.begin());
        }
        entries.push_back(entry);
    }
}

void drawLogEntries(const std::vector<logger::GuiLogEntry> & entries,
                    const Preferences & preferences,
                    bool newEntriesWereAdded,
                    bool & followScroll)
{
    ImGui::BeginChild("LogEntries", ImVec2(0.0F, 0.0F), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    const bool wasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();
    for (const auto & entry : entries)
    {
        const theme::Color color = theme::logLevelColor(entry.level);
        ImGui::PushStyleColor(ImGuiCol_Text, theme::imguiColor(color));
        ImGui::TextWrapped("%s", entry.formattedMessage.c_str());
        ImGui::PopStyleColor();
    }

    if (preferences.autoScroll && (newEntriesWereAdded || (followScroll && wasAtBottom)))
    {
        ImGui::SetScrollHereY(bottomScrollPosition);
    }

    followScroll = preferences.autoScroll;
    ImGui::EndChild();
}

void drawLogPanelContents(logger::GuiLogSink & logSink,
                          Preferences & preferences,
                          std::vector<logger::GuiLogEntry> & entries,
                          bool & followScroll)
{
    const auto newEntries = logSink.drain();
    appendEntries(entries, newEntries);
    const bool newEntriesWereAdded = !newEntries.empty();

    if (ImGui::Checkbox("Autoscroll", &preferences.autoScroll))
    {
    }

    ImGui::SameLine();

    if (ImGui::Button("Clear"))
    {
        entries.clear();
        logSink.clear();
    }

    ImGui::Separator();
    drawLogEntries(entries, preferences, newEntriesWereAdded, followScroll);
}

} // namespace

float drawLogPanel(logger::GuiLogSink & logSink,
                   Preferences & preferences,
                   std::vector<logger::GuiLogEntry> & entries,
                   bool & followScroll)
{
    if (!preferences.showLogPanel)
    {
        return 0.0F;
    }

    const ImGuiViewport * viewport = ImGui::GetMainViewport();
    const bool wasVisible = preferences.showLogPanel;
    const float maximumPanelHeight = std::max(minimumLogPanelHeight, viewport->WorkSize.y * 0.8F);

    if (preferences.logPanelHeight <= 0.0F)
    {
        preferences.logPanelHeight =
            std::clamp(viewport->WorkSize.y * logPanelHeightRatio, minimumLogPanelHeight, maximumPanelHeight);
    }

    preferences.logPanelHeight = std::clamp(preferences.logPanelHeight, minimumLogPanelHeight, maximumPanelHeight);
    const float collapsedPanelHeight = ImGui::GetFrameHeight();
    const float panelHeight = preferences.logPanelCollapsed ? collapsedPanelHeight : preferences.logPanelHeight;
    const float minimumPanelHeight = preferences.logPanelCollapsed ? collapsedPanelHeight : minimumLogPanelHeight;
    ImGui::SetNextWindowSizeConstraints(ImVec2(viewport->WorkSize.x, minimumPanelHeight),
                                        ImVec2(viewport->WorkSize.x, maximumPanelHeight));
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - panelHeight),
                            ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, preferences.logPanelHeight), ImGuiCond_Appearing);
    ImGui::SetNextWindowCollapsed(preferences.logPanelCollapsed, ImGuiCond_Appearing);
    constexpr auto panelFlags = static_cast<ImGuiWindowFlags>(static_cast<unsigned int>(ImGuiWindowFlags_NoMove) |
                                                              static_cast<unsigned int>(ImGuiWindowFlags_NoSavedSettings));
    const bool windowContentsVisible = ImGui::Begin("Logs###view.logs", &preferences.showLogPanel, panelFlags);
    const bool isCollapsed = ImGui::IsWindowCollapsed();

    if (isCollapsed != preferences.logPanelCollapsed)
    {
        preferences.logPanelCollapsed = isCollapsed;
    }

    if (windowContentsVisible && !isCollapsed)
    {
        drawLogPanelContents(logSink, preferences, entries, followScroll);
        const float updatedPanelHeight = ImGui::GetWindowSize().y;

        if (std::abs(updatedPanelHeight - preferences.logPanelHeight) > 1.0F)
        {
            preferences.logPanelHeight = updatedPanelHeight;
        }
    }

    const float occupiedHeight = std::max(0.0F, viewport->WorkPos.y + viewport->WorkSize.y - ImGui::GetWindowPos().y);
    ImGui::End();

    if (preferences.showLogPanel != wasVisible)
    {
    }

    return occupiedHeight;
}

} // namespace uo_mule::gui
