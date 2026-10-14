#include "core/app_info.h"
#include "gui/application.h"
#include "core/gui_log_sink.h"
#include "gui/demo_panel.h"
#include "gui/log_panel.h"
#include "gui/preferences.h"
#include "gui/theme.h"
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

namespace im_cpp::gui
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
    for (std::size_t channel = 0; channel < appIconRgbChannels; ++channel)
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
    for (std::size_t index = 0; index < appIconPaletteSize; ++index)
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
    for (std::size_t y = 0; y < appIconHeight; ++y)
    {
        if (!std::getline(file, row) || row.size() != appIconWidth)
        {
            throw std::runtime_error("Invalid application icon row");
        }
        for (std::size_t x = 0; x < appIconWidth; ++x)
        {
            const AppIconColor & color = colors.at(appIconPaletteIndex(row.at(x)));
            const std::size_t pixelOffset = ((y * appIconWidth) + x) * appIconChannels;
            for (std::size_t channel = 0; channel < appIconChannels; ++channel)
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

ImGuiID drawDockSpace(float bottomPanelHeight)
{
    const ImGuiViewport * viewport = ImGui::GetMainViewport();
    const float dockSpaceHeight = std::max(0.0F, viewport->WorkSize.y - bottomPanelHeight);
    ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, dockSpaceHeight), ImGuiCond_Always);
    constexpr auto dockSpaceWindowFlags = static_cast<ImGuiWindowFlags>(
        static_cast<unsigned int>(ImGuiWindowFlags_NoDocking) | static_cast<unsigned int>(ImGuiWindowFlags_NoTitleBar) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoCollapse) | static_cast<unsigned int>(ImGuiWindowFlags_NoResize) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoMove) | static_cast<unsigned int>(ImGuiWindowFlags_NoBringToFrontOnFocus) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoNavFocus) | static_cast<unsigned int>(ImGuiWindowFlags_NoBackground) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoSavedSettings) | static_cast<unsigned int>(ImGuiWindowFlags_NoScrollbar) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoScrollWithMouse));
    ImGui::Begin("BackgroundDockSpace", nullptr, dockSpaceWindowFlags);
    constexpr ImGuiDockNodeFlags dockSpaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;
    const ImGuiID dockSpaceId = ImGui::GetID("BackgroundDockSpace");
    ImGui::DockSpace(dockSpaceId, ImVec2(0.0F, 0.0F), dockSpaceFlags);
    ImGui::End();
    return dockSpaceId;
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

void drawFrame(GLFWwindow * window,
               spdlog::logger & logger,
               core::GuiLogSink & logSink,
               Preferences & preferences,
               bool & shouldClose,
               std::vector<core::GuiLogEntry> & entries,
               bool & followScroll,
               DemoPanelState & demoPanelState)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    if (ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false) && !ImGui::GetIO().WantTextInput &&
        !ImGui::GetIO().WantCaptureKeyboard)
    {
        preferences.showLogPanel = !preferences.showLogPanel;
        savePreferences(preferences);
    }
    const float bottomPanelHeight = drawLogPanel(logSink, preferences, entries, followScroll);
    const ImGuiID dockSpaceId = drawDockSpace(bottomPanelHeight);
    drawDemoPanel(logger, preferences, shouldClose, dockSpaceId, bottomPanelHeight, demoPanelState);
    renderFrame(window);
}

} // namespace

Application::Application(std::shared_ptr<spdlog::logger> logger, std::shared_ptr<core::GuiLogSink> logSink) :
    logger_(std::move(logger)), logSink_(std::move(logSink)), preferences_(loadPreferences())
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
    const std::unique_ptr<GLFWwindow, WindowDeleter> window{
        glfwCreateWindow(initialApplicationWindowWidth,
                         initialApplicationWindowHeight,
                         applicationName.c_str(),
                         nullptr,
                         nullptr)};
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
    ImGui::GetIO().ConfigFlags = static_cast<ImGuiConfigFlags>(
        static_cast<unsigned int>(ImGui::GetIO().ConfigFlags) | static_cast<unsigned int>(ImGuiConfigFlags_DockingEnable));
    std::vector<core::GuiLogEntry> entries;
    bool followScroll = true;
    DemoPanelState demoPanelState;
    while (glfwWindowShouldClose(window.get()) == GLFW_FALSE && !shouldClose_)
    {
        glfwPollEvents();
        drawFrame(window.get(), *logger_, *logSink_, preferences_, shouldClose_, entries, followScroll, demoPanelState);
    }

    SPDLOG_LOGGER_INFO(logger_, "GUI closed");
    return 0;
}

} // namespace im_cpp::gui
