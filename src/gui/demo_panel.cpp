#include "core/app_info.h"
#include "gui/demo_panel.h"
#include "gui/preferences.h"
#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <string>

namespace im_cpp::gui
{
namespace
{

constexpr float initialDemoPanelWidth = 520.0F;
constexpr float initialDemoPanelHeight = 250.0F;
constexpr float minimumDemoPanelWidth = 240.0F;
constexpr float minimumDemoPanelHeight = 100.0F;
constexpr float positionTolerance = 0.5F;

}

void drawDemoPanel(spdlog::logger & logger,
                   Preferences & preferences,
                   bool & shouldClose,
                   ImGuiID dockSpaceId,
                   float bottomPanelHeight,
                   DemoPanelState & state)
{
    const ImGuiViewport * viewport = ImGui::GetMainViewport();
    if (state.width <= 0.0F)
    {
        state.width = initialDemoPanelWidth;
        state.height = initialDemoPanelHeight;
    }
    const ImVec2 workBottom(viewport->WorkPos.x + viewport->WorkSize.x,
                            viewport->WorkPos.y + viewport->WorkSize.y - bottomPanelHeight);
    const float availableWidth = std::max(0.0F, workBottom.x - viewport->WorkPos.x);
    const float availableHeight = std::max(0.0F, workBottom.y - viewport->WorkPos.y);
    bool resizeToFit = false;
    if (!state.hasWindowGeometry)
    {
        ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowDockID(dockSpaceId, ImGuiCond_FirstUseEver);
    }
    else if (!state.docked)
    {
        const float constrainedWidth = std::min(state.width, availableWidth);
        const float constrainedHeight = std::min(state.height, availableHeight);
        const float constrainedX =
            std::clamp(state.x, viewport->WorkPos.x, std::max(viewport->WorkPos.x, workBottom.x - constrainedWidth));
        const float constrainedY =
            std::clamp(state.y, viewport->WorkPos.y, std::max(viewport->WorkPos.y, workBottom.y - constrainedHeight));
        if (std::abs(constrainedX - state.x) > positionTolerance || std::abs(constrainedY - state.y) > positionTolerance)
        {
            ImGui::SetNextWindowPos(ImVec2(constrainedX, constrainedY), ImGuiCond_Always);
        }
        if (std::abs(constrainedWidth - state.width) > positionTolerance ||
            std::abs(constrainedHeight - state.height) > positionTolerance)
        {
            state.width = constrainedWidth;
            state.height = constrainedHeight;
            resizeToFit = true;
        }
    }
    ImGui::SetNextWindowSize(ImVec2(state.width, state.height), resizeToFit ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
    if (!state.docked)
    {
        ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(minimumDemoPanelWidth, availableWidth),
                                                   std::min(minimumDemoPanelHeight, availableHeight)),
                                            ImVec2(availableWidth, availableHeight));
    }
    const std::string panelName(core::applicationName());
    const bool windowContentsVisible = ImGui::Begin(panelName.c_str());
    state.docked = ImGui::IsWindowDocked();
    state.x = ImGui::GetWindowPos().x;
    state.y = ImGui::GetWindowPos().y;
    state.width = ImGui::GetWindowSize().x;
    state.height = ImGui::GetWindowSize().y;
    state.hasWindowGeometry = true;
    if (windowContentsVisible)
    {
        ImGui::Text("Hello I'm C++!");
        if (ImGui::Button(preferences.showLogPanel ? "Hide Logs (§)" : "Show Logs (§)"))
        {
            preferences.showLogPanel = !preferences.showLogPanel;
            savePreferences(preferences);
        }
        ImGui::SameLine();
        if (ImGui::Button("Generate Test Logs"))
        {
            logger.log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},
                       spdlog::level::trace,
                       "Generated a trace message");
            logger.log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},
                       spdlog::level::debug,
                       "Generated a debug message");
            logger.log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},
                       spdlog::level::info,
                       "Generated an information message");
            logger.log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},
                       spdlog::level::warn,
                       "Generated a warning message");
            logger.log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},
                       spdlog::level::err,
                       "Generated an error message");
        }
        ImGui::SameLine();
        if (ImGui::Button("Close"))
        {
            shouldClose = true;
        }
    }
    ImGui::End();
}

}
