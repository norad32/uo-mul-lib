#include "gui/panels/log_panel.h"
#include "gui/presentation/strings.h"
#include "gui/presentation/theme.h"
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
                    bool autoScroll,
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

    if (autoScroll && (newEntriesWereAdded || (followScroll && wasAtBottom)))
    {
        ImGui::SetScrollHereY(bottomScrollPosition);
    }

    followScroll = autoScroll;
    ImGui::EndChild();
}

void drawLogPanelContents(logger::GuiLogSink & logSink,
                          bool autoScroll,
                          std::vector<logger::GuiLogEntry> & entries,
                          bool & followScroll,
                          LogPanelRequest & request)
{
    const auto newEntries = logSink.drain();
    appendEntries(entries, newEntries);
    const bool newEntriesWereAdded = !newEntries.empty();

    if (ImGui::Checkbox(strings::autoScroll(), &autoScroll))
    {
        request.autoScroll = autoScroll;
    }

    ImGui::SameLine();

    if (ImGui::Button(strings::clear()))
    {
        request.clearEntries = true;
    }

    ImGui::Separator();
    drawLogEntries(entries, autoScroll, newEntriesWereAdded, followScroll);
}

} // namespace

float drawLogPanel(logger::GuiLogSink & logSink,
                   const Preferences & preferences,
                   std::vector<logger::GuiLogEntry> & entries,
                   bool & followScroll,
                   LogPanelRequest & request)
{
    request = {};
    if (!preferences.showLogPanel)
    {
        return 0.0F;
    }

    const ImGuiViewport * viewport = ImGui::GetMainViewport();
    const bool wasVisible = preferences.showLogPanel;
    const float maximumPanelHeight = std::max(minimumLogPanelHeight, viewport->WorkSize.y * 0.8F);

    float panelHeightPreference = preferences.logPanelHeight;
    bool panelCollapsed = preferences.logPanelCollapsed;
    bool showPanel = preferences.showLogPanel;
    bool autoScroll = preferences.autoScroll;
    if (panelHeightPreference <= 0.0F)
    {
        panelHeightPreference =
            std::clamp(viewport->WorkSize.y * logPanelHeightRatio, minimumLogPanelHeight, maximumPanelHeight);
    }

    panelHeightPreference = std::clamp(panelHeightPreference, minimumLogPanelHeight, maximumPanelHeight);
    const float collapsedPanelHeight = ImGui::GetFrameHeight();
    const float panelHeight = panelCollapsed ? collapsedPanelHeight : panelHeightPreference;
    const float minimumPanelHeight = panelCollapsed ? collapsedPanelHeight : minimumLogPanelHeight;
    ImGui::SetNextWindowSizeConstraints(ImVec2(viewport->WorkSize.x, minimumPanelHeight),
                                        ImVec2(viewport->WorkSize.x, maximumPanelHeight));
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - panelHeight),
                            ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, panelHeightPreference), ImGuiCond_Appearing);
    ImGui::SetNextWindowCollapsed(panelCollapsed, ImGuiCond_Appearing);
    constexpr auto panelFlags = static_cast<ImGuiWindowFlags>(static_cast<unsigned int>(ImGuiWindowFlags_NoMove) |
                                                              static_cast<unsigned int>(ImGuiWindowFlags_NoSavedSettings));
    const bool windowContentsVisible = ImGui::Begin("Logs###view.logs", &showPanel, panelFlags);
    const bool isCollapsed = ImGui::IsWindowCollapsed();

    if (isCollapsed != panelCollapsed)
    {
        panelCollapsed = isCollapsed;
        request.collapsed = panelCollapsed;
    }

    if (windowContentsVisible && !isCollapsed)
    {
        drawLogPanelContents(logSink, autoScroll, entries, followScroll, request);
        const float updatedPanelHeight = ImGui::GetWindowSize().y;

        if (std::abs(updatedPanelHeight - panelHeightPreference) > 1.0F)
        {
            panelHeightPreference = updatedPanelHeight;
            request.height = updatedPanelHeight;
        }
    }

    const float occupiedHeight = std::max(0.0F, viewport->WorkPos.y + viewport->WorkSize.y - ImGui::GetWindowPos().y);
    ImGui::End();

    if (showPanel != wasVisible)
    {
        request.visible = showPanel;
    }

    if (std::abs(panelHeightPreference - preferences.logPanelHeight) > 1.0F)
    {
        request.height = panelHeightPreference;
    }
    if (autoScroll != preferences.autoScroll)
    {
        request.autoScroll = autoScroll;
    }
    if (panelCollapsed != preferences.logPanelCollapsed)
    {
        request.collapsed = panelCollapsed;
    }

    return occupiedHeight;
}

} // namespace uo_mule::gui
