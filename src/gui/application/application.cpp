#include "gui/application/application.h"
#include "core/app_info.h"
#include "gui/application/gui_resources.h"
#include "gui/commands/menu_bar.h"
#include "gui/workbench/workbench.h"
#include "gui/application/layout_store.h"
#include "gui/presentation/imgui_fonts.h"
#include "gui/presentation/theme.h"
#include "gui/workbench/workbench_state.h"
#include "gui_model/persistence/asset_state_store.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/session/workbench_session.h"
#include "logger/gui_log_sink.h"
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <array>
#include <cstddef>
#include <fstream>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <memory>
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace uo_mule::gui
{
namespace
{

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
    const auto path = defaultGuiResourceLookup().find(GuiResourceId::applicationIcon);
    return path.has_value() ? std::ifstream(*path) : std::ifstream{};
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

void renderFrame(GLFWwindow * window)
{
    ImGui::Render();
    int displayWidth = 0;
    int displayHeight = 0;
    glfwGetFramebufferSize(window, &displayWidth, &displayHeight);
    glViewport(0, 0, displayWidth, displayHeight);
    glClearColor(theme::canvasBackground.red,
                 theme::canvasBackground.green,
                 theme::canvasBackground.blue,
                 theme::canvasBackground.alpha);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
}

void drawFrame(GLFWwindow * window, logger::GuiLogSink & logSink, WorkbenchState & state)
{
    drawWorkbench(window, logSink, state);
    renderFrame(window);
}

void captureWindowLayout(GLFWwindow * window, AppLayout & layout, bool fullscreen)
{
    if (fullscreen)
    {
        return;
    }

    glfwGetWindowPos(window, &layout.window.x, &layout.window.y);
    glfwGetWindowSize(window, &layout.window.width, &layout.window.height);
    layout.window.geometryValid = layout.window.width > 0 && layout.window.height > 0;
}

} // namespace

Application::Application(std::shared_ptr<spdlog::logger> logger, std::shared_ptr<logger::GuiLogSink> logSink) :
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
    WorkbenchState state(loadPreferences());
    loadLayout(state.panelRegistry, state.layout);
    state.assetStateRecords = assets::loadAssetState(assets::assetStatePath());
    const std::unique_ptr<GLFWwindow, WindowDeleter> window{
        glfwCreateWindow(defaultWindowWidth, defaultWindowHeight, applicationName.c_str(), nullptr, nullptr)};

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
    const ImGuiFontState fontState =
        configureImGuiFonts(defaultGuiResourceLookup().find(GuiResourceId::materialDesignIconsFont));
    if (!fontState.iconFontAvailable)
    {
        SPDLOG_LOGGER_WARN(logger_, "{}", fontState.diagnostic);
    }
    ImGui::StyleColorsDark();
    theme::applyImGuiStyle();
    const GlfwBackend glfwBackend(window.get());
    const OpenGLBackend openGLBackend;

    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().ConfigFlags = static_cast<ImGuiConfigFlags>(static_cast<unsigned int>(ImGui::GetIO().ConfigFlags) |
                                                               static_cast<unsigned int>(ImGuiConfigFlags_DockingEnable));
    if (!state.layout.dockingIni.empty())
    {
        ImGui::LoadIniSettingsFromMemory(state.layout.dockingIni.c_str(), state.layout.dockingIni.size());
    }
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
    state.mapDocuments.invalidateProviderCaches();
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
                                   auto * callbackState = static_cast<WorkbenchState *>(glfwGetWindowUserPointer(glfwWindow));
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

    if (state.workbenchLayoutApplied)
    {
        captureWindowLayout(window.get(), state.layout, state.preferences.fullscreen);
        const char * dockingIni = ImGui::SaveIniSettingsToMemory();
        state.layout.dockingIni = dockingIni == nullptr ? std::string{} : std::string(dockingIni);
        saveLayout(state.panelRegistry, state.layout);
        if (state.assetStateResetRequested)
        {
            assets::resetAssetState(state.assetStateRecords);
        }
        assets::saveAssetState(assets::assetStatePath(), state.assetStateRecords);
    }
    state.toolState.savePreferences(state.preferences.tools);
    savePreferences(state.preferences);

    SPDLOG_LOGGER_INFO(logger_, "GUI closed");
    return 0;
}

} // namespace uo_mule::gui
