#include "gui/application/gui_resources.h"
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <system_error>
#include <utility>

namespace uo_mule::gui
{
namespace
{

[[nodiscard]] bool isUsableFile(const std::filesystem::path & path)
{
    std::error_code error;
    return std::filesystem::is_regular_file(path, error) && !error;
}

[[nodiscard]] const char * environmentVariable(GuiResourceId resource)
{
    switch (resource)
    {
        case GuiResourceId::applicationIcon :
            return "IM_CPP_ICON_PATH";
        case GuiResourceId::welcomeBanner :
            return "IM_CPP_WELCOME_BANNER_PATH";
        case GuiResourceId::materialDesignIconsFont :
            return "IM_CPP_FONT_PATH";
    }
    return nullptr;
}

} // namespace

GuiResourceLookup::GuiResourceLookup(std::filesystem::path executablePath,
                                     std::filesystem::path applicationIconInstallPath,
                                     std::filesystem::path applicationIconSourcePath,
                                     std::filesystem::path welcomeBannerInstallPath,
                                     std::filesystem::path welcomeBannerSourcePath,
                                     std::filesystem::path materialDesignIconsInstallPath,
                                     std::filesystem::path materialDesignIconsSourcePath) :
    executablePath_(std::move(executablePath)),
    applicationIconInstallPath_(std::move(applicationIconInstallPath)),
    applicationIconSourcePath_(std::move(applicationIconSourcePath)),
    welcomeBannerInstallPath_(std::move(welcomeBannerInstallPath)),
    welcomeBannerSourcePath_(std::move(welcomeBannerSourcePath)),
    materialDesignIconsInstallPath_(std::move(materialDesignIconsInstallPath)),
    materialDesignIconsSourcePath_(std::move(materialDesignIconsSourcePath))
{
}

std::optional<std::filesystem::path> GuiResourceLookup::environmentOverride(GuiResourceId resource)
{
    const char * variable = environmentVariable(resource);
    if (variable == nullptr)
    {
        return std::nullopt;
    }

    const char * value = std::getenv(variable);
    if (value == nullptr || *value == '\0')
    {
        return std::nullopt;
    }

    const std::filesystem::path path(value);
    return isUsableFile(path) ? std::optional<std::filesystem::path>(path) : std::nullopt;
}

std::filesystem::path GuiResourceLookup::installPath(GuiResourceId resource) const
{
    const std::filesystem::path * relativePath = &materialDesignIconsInstallPath_;
    if (resource == GuiResourceId::applicationIcon)
    {
        relativePath = &applicationIconInstallPath_;
    }
    else if (resource == GuiResourceId::welcomeBanner)
    {
        relativePath = &welcomeBannerInstallPath_;
    }
    return executablePath_.empty() ? std::filesystem::path{}
                                   : (executablePath_.parent_path() / *relativePath).lexically_normal();
}

const std::filesystem::path & GuiResourceLookup::sourcePath(GuiResourceId resource) const
{
    if (resource == GuiResourceId::applicationIcon)
    {
        return applicationIconSourcePath_;
    }
    if (resource == GuiResourceId::welcomeBanner)
    {
        return welcomeBannerSourcePath_;
    }
    return materialDesignIconsSourcePath_;
}

std::optional<std::filesystem::path> GuiResourceLookup::find(GuiResourceId resource) const
{
    if (const auto overridePath = environmentOverride(resource); overridePath.has_value())
    {
        return overridePath;
    }

    std::filesystem::path installedPath = installPath(resource);
    if (isUsableFile(installedPath))
    {
        return installedPath;
    }

    std::filesystem::path developmentPath = sourcePath(resource);
    if (isUsableFile(developmentPath))
    {
        return developmentPath;
    }

    return std::nullopt;
}

GuiResourceLookup defaultGuiResourceLookup()
{
    std::error_code error;
    const std::filesystem::path executablePath = std::filesystem::read_symlink("/proc/self/exe", error);
    return {error ? std::filesystem::path{} : executablePath,
            APPLICATION_ICON_INSTALL_RELATIVE_PATH,
            APPLICATION_ICON_SOURCE_PATH,
            WELCOME_BANNER_INSTALL_RELATIVE_PATH,
            WELCOME_BANNER_SOURCE_PATH,
            MATERIAL_DESIGN_ICONS_INSTALL_RELATIVE_PATH,
            MATERIAL_DESIGN_ICONS_SOURCE_PATH};
}

} // namespace uo_mule::gui
