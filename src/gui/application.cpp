#include "gui/application.h"
#include "core/app_info.h"
#include "core/gui_log_sink.h"
#include "gui/data_profile.h"
#include "gui/data_profile_store.h"
#include "gui/command_registry.h"
#include "gui/dialogs/preferences_dialog.h"
#include "gui/log_panel.h"
#include "gui/map_workspace.h"
#include "gui/map_context.h"
#include "gui/menu_bar.h"
#include "gui/notification_state.h"
#include "gui/panels/block_inspector_panel.h"
#include "gui/panels/edit_tools_panel.h"
#include "gui/panels/map_canvas_panel.h"
#include "gui/panels/map_info_panel.h"
#include "gui/panels/radarcol_panel.h"
#include "gui/panels/selection_stats_panel.h"
#include "gui/panels/texmaps_panel.h"
#include "gui/panels/tile_inspector_panel.h"
#include "gui/preferences.h"
#include "gui/theme.h"
#include "gui/workbench_state.h"
#include "gui/workbench_session.h"
#include "gui/welcome_screen.h"
#include "gui/layout_store.h"
#include "gui/panel_registry.h"
#include "uomul/map.h"
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <memory>
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace uo_mule::gui
{
namespace
{

constexpr int initialApplicationWindowWidth = 1280;
constexpr int initialApplicationWindowHeight = 800;
constexpr std::size_t appIconWidth = 32;
constexpr std::size_t appIconHeight = 32;
constexpr int appIconWidthAsInt = 32;
constexpr int appIconHeightAsInt = 32;
constexpr std::size_t appIconPaletteSize = 2;
constexpr int appIconPaletteSizeAsInt = 2;
constexpr std::size_t appIconRgbChannels = 3;
constexpr std::size_t appIconAlphaChannel = appIconRgbChannels;
constexpr std::size_t appIconChannels = appIconRgbChannels + 1;
constexpr std::size_t appIconHexColorLength = 7;
constexpr int appIconHexBase = 16;
constexpr int appIconHexAlphaOffset = 10;
constexpr int appIconAlpha = 255;
constexpr float dockLeftWidthRatio = 0.20F;
constexpr float dockRightWidthRatio = 0.24F;
constexpr float statusBarHeight = 24.0F;
constexpr float notificationLeftMargin = 16.0F;
constexpr float notificationTopMargin = 48.0F;
constexpr float notificationMinimumWidth = 280.0F;
constexpr float notificationMaximumWidth = 480.0F;
constexpr float notificationMaximumHeightRatio = 0.5F;
constexpr char hexadecimalLowercaseStart = 'a';
constexpr char hexadecimalUppercaseStart = 'A';
constexpr char hexadecimalLastDigit = '9';
constexpr char hexadecimalLastLowercase = 'f';
constexpr char hexadecimalLastUppercase = 'F';
using AppIconColor = std::array<unsigned char, appIconChannels>;
using AppIconPalette = std::array<AppIconColor, appIconPaletteSize>;
using AppIconPixels = std::array<unsigned char, appIconWidth * appIconHeight * appIconChannels>;

[[nodiscard]] std::ifstream openAppIcon()
{
    if (const char * path = std::getenv("IM_CPP_ICON_PATH"); path != nullptr)
    {
        std::ifstream file(path);
        if (file)
        {
            return file;
        }
    }

    std::error_code error;
    const std::filesystem::path executablePath = std::filesystem::read_symlink("/proc/self/exe", error);

    if (!error)
    {
        std::ifstream file(executablePath.parent_path() / APPLICATION_ICON_INSTALL_RELATIVE_PATH);
        if (file)
        {
            return file;
        }
    }
    return std::ifstream(APPLICATION_ICON_SOURCE_PATH);
}

[[nodiscard]] unsigned char parseHexDigit(char value)
{
    if (value >= '0' && value <= hexadecimalLastDigit)
    {
        return static_cast<unsigned char>(value - '0');
    }

    if (value >= hexadecimalLowercaseStart && value <= hexadecimalLastLowercase)
    {
        return static_cast<unsigned char>(value - hexadecimalLowercaseStart + appIconHexAlphaOffset);
    }

    if (value >= hexadecimalUppercaseStart && value <= hexadecimalLastUppercase)
    {
        return static_cast<unsigned char>(value - hexadecimalUppercaseStart + appIconHexAlphaOffset);
    }
    throw std::runtime_error("Invalid application icon color");
}

[[nodiscard]] std::size_t appIconPaletteIndex(char symbol)
{
    if (symbol == '.')
    {
        return 0;
    }

    if (symbol == '+')
    {
        return 1;
    }

    throw std::runtime_error("Invalid application icon symbol");
}

[[nodiscard]] AppIconColor parseAppIconColor(const std::string & definition)
{
    std::istringstream colorLine(definition);
    char symbol = 0;
    std::string marker;
    std::string hexColor;
    colorLine >> symbol >> marker >> hexColor;
    if ((symbol != '.' && symbol != '+') || marker != "c" || hexColor.size() != appIconHexColorLength ||
        hexColor.front() != '#')
    {
        throw std::runtime_error("Invalid application icon color");
    }

    AppIconColor color{};
    for (std::size_t channel = 0; channel < appIconRgbChannels; channel++)
    {
        constexpr std::size_t hexPrefixLength = 1;
        constexpr std::size_t hexDigitsPerChannel = 2;
        const std::size_t digitOffset = hexPrefixLength + (channel * hexDigitsPerChannel);
        color.at(channel) = static_cast<unsigned char>((parseHexDigit(hexColor.at(digitOffset)) * appIconHexBase) +
                                                       parseHexDigit(hexColor.at(digitOffset + 1)));
    }

    color.at(appIconAlphaChannel) = appIconAlpha;
    return color;
}

[[nodiscard]] AppIconPalette loadAppIconPalette(std::ifstream & file)
{
    AppIconPalette colors{};
    for (std::size_t index = 0; index < appIconPaletteSize; index++)
    {
        std::string colorDefinition;
        if (!std::getline(file, colorDefinition))
        {
            throw std::runtime_error("Invalid application icon palette");
        }

        std::istringstream colorLine(colorDefinition);
        char symbol = 0;
        colorLine >> symbol;
        colors.at(appIconPaletteIndex(symbol)) = parseAppIconColor(colorDefinition);
    }

    return colors;
}

void loadAppIconPixels(std::ifstream & file, const AppIconPalette & colors, AppIconPixels & pixels)
{
    std::string row;
    for (std::size_t y = 0; y < appIconHeight; y++)
    {
        if (!std::getline(file, row) || row.size() != appIconWidth)
        {
            throw std::runtime_error("Invalid application icon row");
        }

        for (std::size_t x = 0; x < appIconWidth; x++)
        {
            const AppIconColor & color = colors.at(appIconPaletteIndex(row.at(x)));
            const std::size_t pixelOffset = ((y * appIconWidth) + x) * appIconChannels;
            for (std::size_t channel = 0; channel < appIconChannels; channel++)
            {
                pixels.at(pixelOffset + channel) = color.at(channel);
            }
        }
    }
}

[[nodiscard]] AppIconPixels loadAppIcon()
{
    std::ifstream file = openAppIcon();
    if (!file)
    {
        throw std::runtime_error("Failed to open application icon asset");
    }

    std::string line;
    int width = 0;
    int height = 0;
    int colorCount = 0;
    int charactersPerPixel = 0;

    if (!std::getline(file, line) || line != "! XPM2" || !std::getline(file, line))
    {
        throw std::runtime_error("Invalid application icon asset");
    }

    std::istringstream dimensions(line);
    dimensions >> width >> height >> colorCount >> charactersPerPixel;
    if (width != appIconWidthAsInt || height != appIconHeightAsInt || colorCount != appIconPaletteSizeAsInt ||
        charactersPerPixel != 1)
    {
        throw std::runtime_error("Unsupported application icon asset");
    }

    AppIconPixels pixels{};
    const AppIconPalette colors = loadAppIconPalette(file);
    loadAppIconPixels(file, colors, pixels);

    return pixels;
}

class GlfwSession
{
public:
    GlfwSession(const GlfwSession &) = delete;
    GlfwSession & operator=(const GlfwSession &) = delete;
    GlfwSession(GlfwSession &&) = delete;
    GlfwSession & operator=(GlfwSession &&) = delete;

    GlfwSession()
    {
        if (glfwInit() == GLFW_FALSE)
        {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    }

    ~GlfwSession() { glfwTerminate(); }
};

struct WindowDeleter
{
    void operator()(GLFWwindow * window) const noexcept
    {
        if (window != nullptr)
        {
            glfwDestroyWindow(window);
        }
    }
};

class ImGuiContext
{
public:
    ImGuiContext()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
    }

    ~ImGuiContext() { ImGui::DestroyContext(); }

    ImGuiContext(const ImGuiContext &) = delete;
    ImGuiContext & operator=(const ImGuiContext &) = delete;
    ImGuiContext(ImGuiContext &&) = delete;
    ImGuiContext & operator=(ImGuiContext &&) = delete;
};

class GlfwBackend
{
public:
    explicit GlfwBackend(GLFWwindow * window) : initialized_(ImGui_ImplGlfw_InitForOpenGL(window, true))
    {
        if (!initialized_)
        {
            throw std::runtime_error("Failed to initialize ImGui GLFW backend");
        }
    }

    ~GlfwBackend()
    {
        if (initialized_)
        {
            ImGui_ImplGlfw_Shutdown();
        }
    }

    GlfwBackend(const GlfwBackend &) = delete;
    GlfwBackend & operator=(const GlfwBackend &) = delete;
    GlfwBackend(GlfwBackend &&) = delete;
    GlfwBackend & operator=(GlfwBackend &&) = delete;

private:
    bool initialized_ = false;
};

class OpenGLBackend
{
public:
    OpenGLBackend() : initialized_(ImGui_ImplOpenGL3_Init("#version 330"))
    {
        if (!initialized_)
        {
            throw std::runtime_error("Failed to initialize ImGui OpenGL backend");
        }
    }

    ~OpenGLBackend()
    {
        if (initialized_)
        {
            ImGui_ImplOpenGL3_Shutdown();
        }
    }

    OpenGLBackend(const OpenGLBackend &) = delete;
    OpenGLBackend & operator=(const OpenGLBackend &) = delete;
    OpenGLBackend(OpenGLBackend &&) = delete;
    OpenGLBackend & operator=(OpenGLBackend &&) = delete;

private:
    bool initialized_ = false;
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
    constexpr ImGuiDockNodeFlags dockSpaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;
    const ImGuiID dockSpaceId = ImGui::GetID("BackgroundDockSpace");
    if (state.layoutResetRequested || ImGui::DockBuilderGetNode(dockSpaceId) == nullptr)
    {
        state.layoutResetRequested = false;
        ImGui::DockBuilderRemoveNode(dockSpaceId);
        ImGui::DockBuilderAddNode(dockSpaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockSpaceId, ImVec2(viewport->WorkSize.x, dockSpaceHeight));
        ImGuiID leftNode = 0;
        ImGuiID centerNode = 0;
        ImGuiID rightNode = 0;
        ImGui::DockBuilderSplitNode(dockSpaceId, ImGuiDir_Left, dockLeftWidthRatio, &leftNode, &centerNode);
        ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Right, dockRightWidthRatio, &rightNode, &centerNode);
        ImGui::DockBuilderDockWindow("Map Canvas###view.map-canvas", centerNode);

        // Keep all inspectors together on the left and tools together on the right.
        ImGui::DockBuilderDockWindow("Tile Inspector###view.tile-inspector", leftNode);
        ImGui::DockBuilderDockWindow("Block Inspector###view.block-inspector", leftNode);
        ImGui::DockBuilderDockWindow("Selection Statistics###view.selection-statistics", leftNode);
        ImGui::DockBuilderDockWindow("Map Information###view.map-information", leftNode);
        ImGui::DockBuilderDockWindow("Edit Tools###view.edit-tools", rightNode);
        ImGui::DockBuilderDockWindow("Texmaps###view.texmaps", rightNode);
        ImGui::DockBuilderDockWindow("Radarcol###view.radarcol", rightNode);

        ImGui::DockBuilderFinish(dockSpaceId);
    }
    ImGui::DockSpace(dockSpaceId, ImVec2(0.0F, 0.0F), dockSpaceFlags);
    ImGui::End();
}

void renderFrame(GLFWwindow * window)
{
    ImGui::Render();
    int displayWidth = 0;
    int displayHeight = 0;
    glfwGetFramebufferSize(window, &displayWidth, &displayHeight);
    glViewport(0, 0, displayWidth, displayHeight);
    glClearColor(theme::background.red, theme::background.green, theme::background.blue, theme::background.alpha);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
}

void drawMapPanels(WorkbenchState & state,
                   const DataProfile & activeProfile,
                   MapCanvasPanelState & canvasState,
                   BlockInspectorPanelState & blockInspectorState,
                   SelectionStatsPanelState & selectionStatsState,
                   EditToolsPanelState & editToolsState,
                   RadarcolPanelState & radarcolState,
                   TexmapsPanelState & texmapsState)
{
    MapWorkspace & workspace = state.workspace;
    state.providers.sync(activeProfile);
    syncRadarcolPanel(radarcolState, activeProfile);
    syncTexmapsPanel(texmapsState, activeProfile);

    if (state.panelRegistry.find(PanelId::mapCanvas)->visible)
    {
        drawMapCanvasPanel(workspace,
                           radarcolState,
                           canvasState,
                           [&state] { static_cast<void>(state.commands.execute(CommandId::fileOpen, state)); },
                           [&state] { static_cast<void>(state.commands.execute(CommandId::fileNew, state)); },
                           [&state](const MapCanvasStatus & status) { state.lastHoveredCanvasStatus = status; },
                           state.panelRegistry.find(PanelId::mapCanvas)->visible);
    }
    else
    {
        workspace.clearHover();
    }
    const MapContextSnapshot context = state.session.contextSnapshot(workspace);

    if (state.panelRegistry.find(PanelId::tileInspector)->visible)
    {
        drawTileInspectorPanel(workspace, context, state.providers, state.panelRegistry.find(PanelId::tileInspector)->visible);
    }

    if (state.panelRegistry.find(PanelId::blockInspector)->visible)
    {
        drawBlockInspectorPanel(workspace, context, state.providers, blockInspectorState, state.panelRegistry.find(PanelId::blockInspector)->visible);
    }

    if (state.panelRegistry.find(PanelId::selectionStatistics)->visible)
    {
        drawSelectionStatsPanel(workspace, context, selectionStatsState, state.panelRegistry.find(PanelId::selectionStatistics)->visible);
    }

    if (state.panelRegistry.find(PanelId::mapInformation)->visible)
    {
        drawMapInfoPanel(workspace, state.panelRegistry.find(PanelId::mapInformation)->visible);
    }

    if (state.panelRegistry.find(PanelId::editTools)->visible)
    {
        drawEditToolsPanel(workspace, editToolsState, state.panelRegistry.find(PanelId::editTools)->visible);
    }

    if (state.panelRegistry.find(PanelId::radarcol)->visible)
    {
    drawRadarcolPanel(radarcolState, workspace.editMode(), state.panelRegistry.find(PanelId::radarcol)->visible, state.fileBrowser);
    }

    if (state.panelRegistry.find(PanelId::texmaps)->visible)
    {
    drawTexmapsPanel(texmapsState, state.panelRegistry.find(PanelId::texmaps)->visible, state.fileBrowser);
    }
}

[[nodiscard]] const char * readinessLabel(ProfileReadinessState state) noexcept;

void drawStatusBar(const WorkbenchState & state, float bottomPanelHeight)
{
    const ImGuiViewport * viewport = ImGui::GetMainViewport();
    const float statusY = viewport->WorkPos.y + viewport->WorkSize.y - bottomPanelHeight - statusBarHeight;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, statusY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, statusBarHeight), ImGuiCond_Always);
    constexpr auto statusFlags = static_cast<ImGuiWindowFlags>(
        static_cast<unsigned int>(ImGuiWindowFlags_NoDecoration) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoDocking) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoMove) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoResize) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoSavedSettings) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoScrollbar));
    ImGui::Begin("Workbench Status###workbench.status", nullptr, statusFlags);
    bool hasCanvasStatus = false;
    if (state.lastHoveredCanvasStatus)
    {
        hasCanvasStatus = true;
        const MapCanvasStatus & canvasStatus = *state.lastHoveredCanvasStatus;
        if (!canvasStatus.hasDocument || !canvasStatus.tile)
        {
            ImGui::Text("%s: No tile under pointer  Zoom %.3gx", canvasStatus.canvasId.c_str(), canvasStatus.zoom);
        }
        else
        {
            const uomul::BlockCoord block = tileToBlock(*canvasStatus.tile);
            ImGui::Text("%s: Tile (%u, %u)  Block (%u, %u)  Zoom %.3gx",
                        canvasStatus.canvasId.c_str(),
                        canvasStatus.tile->x,
                        canvasStatus.tile->y,
                        block.x,
                        block.y,
                        canvasStatus.zoom);
        }
    }

    if (hasCanvasStatus)
    {
        ImGui::SameLine();
    }

    const std::string profileStatus = "Profile: " + (state.activeProfile.name.empty() ? std::string("None") : state.activeProfile.name);
    const std::string mapStatus =
        "Map: " + (state.workspace.reader() == nullptr ? std::string("None") : state.workspace.mapPath().string());
    const std::string stateStatus = "State: " + std::string(state.workspace.dirty() ? "Staged changes" : "Clean");
    const std::string readinessStatus = "Data: " + std::string(readinessLabel(state.session.profileState()));
    const float rightContentWidth = ImGui::CalcTextSize(profileStatus.c_str()).x + ImGui::GetStyle().ItemSpacing.x +
                                    ImGui::CalcTextSize(mapStatus.c_str()).x + ImGui::GetStyle().ItemSpacing.x +
                                    ImGui::CalcTextSize(stateStatus.c_str()).x + ImGui::GetStyle().ItemSpacing.x +
                                    ImGui::CalcTextSize(readinessStatus.c_str()).x;
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - rightContentWidth));
    ImGui::TextUnformatted(profileStatus.c_str());
    ImGui::SameLine();
    ImGui::TextUnformatted(mapStatus.c_str());
    ImGui::SameLine();
    ImGui::TextUnformatted(stateStatus.c_str());
    ImGui::SameLine();
    ImGui::TextUnformatted(readinessStatus.c_str());
    ImGui::End();
}

[[nodiscard]] const char * readinessLabel(ProfileReadinessState state) noexcept
{
    switch (state)
    {
        case ProfileReadinessState::needsProfile :
            return "No profile configured";
        case ProfileReadinessState::profileInvalid :
            return "Profile invalid";
        case ProfileReadinessState::profileIncomplete :
            return "Profile incomplete: radarcol.mul required";
        case ProfileReadinessState::ready :
            return "Ready";
    }
    return "Unknown";
}

void drawNotifications(const NotificationState & notifications)
{
    if (notifications.entries.empty())
    {
        return;
    }

    const ImGuiViewport * viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + notificationLeftMargin,
                                  viewport->WorkPos.y + notificationTopMargin),
                            ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(notificationMinimumWidth, 0.0F),
                                        ImVec2(notificationMaximumWidth, viewport->WorkSize.y * notificationMaximumHeightRatio));
    if (ImGui::Begin("Notifications###workbench.notifications", nullptr,
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        for (const auto & notification : notifications.entries)
        {
            ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
            if (notification.severity == NotificationSeverity::error)
            {
                color = theme::imguiColor(theme::coral);
            }
            else if (notification.severity == NotificationSeverity::warning)
            {
                color = theme::imguiColor(theme::amber);
            }
            else if (notification.severity == NotificationSeverity::success)
            {
                color = theme::imguiColor(theme::sageGreen);
            }
            ImGui::TextColored(color, "%s", notification.message.c_str());
        }
    }
    ImGui::End();
}

void drawStartupScreen(WorkbenchState & state)
{
    const ImGuiViewport * viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);
    if (!ImGui::Begin("Configure Preferences###startup.configure-profile", nullptr,
                      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::End();
        return;
    }
    ImGui::TextUnformatted("Configure Preferences");
    ImGui::Separator();
    ImGui::TextUnformatted("A validated radarcol.mul is required before map actions are enabled.");
    ImGui::Text("Profile status: %s", readinessLabel(state.session.profileState()));
    if (ImGui::Button("Open Preferences"))
    {
        requestPreferencesDialog(state.menuBarState.preferencesDialog, state.activeProfile, state.preferences);
    }
    ImGui::End();
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void drawFrame(GLFWwindow * window, core::GuiLogSink & logSink, WorkbenchState & state)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    bool welcomeShell = state.session.startupState() == StartupState::mapWelcome && state.workspace.reader() == nullptr;
    bool profileChanged = welcomeShell ? false : drawMenuBar(state, true);

    if (!state.pendingClipboardText.empty())
    {
        glfwSetClipboardString(window, state.pendingClipboardText.c_str());
        state.pendingClipboardText.clear();
    }

    if (profileChanged)
    {
        state.preferences.activeDataProfile = state.activeProfile.name;
        savePreferences(state.preferences);
        state.session.updateProfile(state.activeProfile);
        if (state.session.mapReady() && !state.preferences.showWelcomeOnStartup)
        {
            state.session.setStartupState(StartupState::workbench);
        }
        if (state.workspace.reader() == nullptr)
        {
            state.welcomeGreetingSelected = false;
            state.welcomeGreeting.clear();
        }
    }

    welcomeShell = state.session.startupState() == StartupState::mapWelcome && state.workspace.reader() == nullptr;
    if (state.nativeWelcomeWindowMode != welcomeShell)
    {
        glfwSetWindowAttrib(window, GLFW_DECORATED, welcomeShell ? GLFW_FALSE : GLFW_TRUE);
        glfwSetWindowAttrib(window, GLFW_RESIZABLE, welcomeShell ? GLFW_FALSE : GLFW_TRUE);
        state.nativeWelcomeWindowMode = welcomeShell;
    }

    if (!ImGui::GetIO().WantTextInput && !ImGui::GetIO().WantCaptureKeyboard)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false))
        {
            static_cast<void>(state.commands.executeShortcut("`", state));
        }
        else if (ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_S, false))
        {
            static_cast<void>(state.commands.executeShortcut("Ctrl+Shift+S", state));
        }
        else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false))
        {
            static_cast<void>(state.commands.executeShortcut("Ctrl+O", state));
        }
        else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N, false))
        {
            static_cast<void>(state.commands.executeShortcut("Ctrl+N", state));
        }
        else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Q, false))
        {
            static_cast<void>(state.commands.executeShortcut("Ctrl+Q", state));
        }
        else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_G, false))
        {
            static_cast<void>(state.commands.executeShortcut("Ctrl+G", state));
        }
    }

    const float bottomPanelHeight = welcomeShell ? 0.0F : drawLogPanel(logSink, state.preferences, state.logEntries, state.followLogScroll);
    const bool noMapLoaded = state.workspace.reader() == nullptr;
    state.session.syncDocument(state.workspace);
    if (state.session.startupState() == StartupState::configureProfile)
    {
        drawStartupScreen(state);
    }
    else if (welcomeShell)
    {
        state.recentMapRecordedPath.clear();
        drawWelcomeScreen(state);
        profileChanged = drawMenuBar(state, false);
    }
    else
    {
        state.welcomeGreetingSelected = false;
        state.welcomeGreeting.clear();
        drawDockSpace(state, bottomPanelHeight);
        drawMapPanels(state,
                      state.activeProfile,
                      state.mapCanvasState,
                      state.blockInspectorState,
                      state.selectionStatsState,
                      state.editToolsState,
                      state.radarcolState,
                      state.texmapsState);
        if (state.recentMapRecordedPath != state.workspace.mapPath())
        {
            state.session.recordRecentMap(state.workspace.mapPath(), state.workspace.dimensions());
            state.recentMapRecordedPath = state.workspace.mapPath();
        }
    }
    if (!noMapLoaded)
    {
        drawStatusBar(state, bottomPanelHeight);
    }
    drawNotifications(state.notifications);
    renderFrame(window);
}

} // namespace

Application::Application(std::shared_ptr<spdlog::logger> logger, std::shared_ptr<core::GuiLogSink> logSink) :
    logger_(std::move(logger)), logSink_(std::move(logSink))
{
}

int Application::run()
{
    SPDLOG_LOGGER_DEBUG(logger_, "GUI starting");
    const GlfwSession glfwSession;
#if GLFW_VERSION_MAJOR > 3 || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 4)
    const std::string applicationId(core::applicationId());
    glfwWindowHintString(GLFW_WAYLAND_APP_ID, applicationId.c_str());
#endif
    const std::string applicationName(core::applicationName());
    const std::unique_ptr<GLFWwindow, WindowDeleter> window{glfwCreateWindow(
        initialApplicationWindowWidth, initialApplicationWindowHeight, applicationName.c_str(), nullptr, nullptr)};

    if (!window)
    {
        throw std::runtime_error("Failed to create GUI window");
    }

    AppIconPixels appIconPixels = loadAppIcon();
    const GLFWimage icon{.width = 32, .height = 32, .pixels = appIconPixels.data()};
    glfwSetWindowIcon(window.get(), 1, &icon);

    glfwMakeContextCurrent(window.get());
    glfwSwapInterval(1);
    const ImGuiContext guiContext;
    ImGui::StyleColorsDark();
    theme::applyImGuiStyle();
    const GlfwBackend glfwBackend(window.get());
    const OpenGLBackend openGLBackend;

    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().ConfigFlags = static_cast<ImGuiConfigFlags>(static_cast<unsigned int>(ImGui::GetIO().ConfigFlags) |
                                                               static_cast<unsigned int>(ImGuiConfigFlags_DockingEnable));
    WorkbenchState state(loadPreferences());
    loadLayout(state.panelRegistry, state.dockingIni);
    state.activeProfile.name = state.preferences.activeDataProfile;
    bool invalidProfileLoad = false;
    if (!state.activeProfile.name.empty())
    {
        const auto loaded = state.profileStore.load(state.activeProfile.name);
        if (loaded.diagnostics.empty())
        {
            state.activeProfile = loaded.profile;
        }
        else
        {
            invalidProfileLoad = true;
        }
    }
    state.session.updateProfile(state.activeProfile);
    if (state.session.mapReady() && !state.preferences.showWelcomeOnStartup)
    {
        state.session.setStartupState(StartupState::workbench);
    }
    if (invalidProfileLoad)
    {
        state.session.markProfileInvalid();
    }
    state.startupLoaded = true;
    glfwSetWindowUserPointer(window.get(), &state);
    glfwSetWindowCloseCallback(window.get(),
                               [](GLFWwindow * glfwWindow)
                               {
                                    auto * callbackState =
                                        static_cast<WorkbenchState *>(glfwGetWindowUserPointer(glfwWindow));
                                    if (callbackState != nullptr)
                                    {
                                        glfwSetWindowShouldClose(glfwWindow, GLFW_FALSE);
                                        requestWindowClose(*callbackState);
                                    }
                               });
    while (glfwWindowShouldClose(window.get()) == GLFW_FALSE && !state.shouldClose)
    {
        glfwPollEvents();
        drawFrame(window.get(), *logSink_, state);
    }

    saveLayout(state.panelRegistry, state.dockingIni);

    SPDLOG_LOGGER_INFO(logger_, "GUI closed");
    return 0;
}

} // namespace uo_mule::gui
