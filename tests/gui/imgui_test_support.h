#pragma once

#include <imgui.h>

namespace uo_mule::gui::test
{

inline constexpr float displayWidth = 1920.0F;
inline constexpr float displayHeight = 1080.0F;
inline constexpr float framesPerSecond = 60.0F;

class ImGuiFrame
{
public:
    ImGuiFrame() : context_(ImGui::CreateContext())
    {
        ImGui::SetCurrentContext(context_);
        ImGui::StyleColorsDark();
        auto & io = ImGui::GetIO();
        io.DisplaySize = ImVec2(displayWidth, displayHeight);
        io.DeltaTime = 1.0F / framesPerSecond;
        io.Fonts->AddFontDefault();
        io.Fonts->Build();
        ImGui::NewFrame();
    }

    ~ImGuiFrame()
    {
        ImGui::EndFrame();
        ImGui::DestroyContext(context_);
    }

    ImGuiFrame(const ImGuiFrame &) = delete;
    ImGuiFrame & operator=(const ImGuiFrame &) = delete;
    ImGuiFrame(ImGuiFrame &&) = delete;
    ImGuiFrame & operator=(ImGuiFrame &&) = delete;

private:
    ImGuiContext * context_ = nullptr;
};

} // namespace uo_mule::gui::test
