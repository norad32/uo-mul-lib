#pragma once

#include <filesystem>
#include <optional>

namespace uo_mule::gui
{

enum class GuiResourceId
{
    applicationIcon,
    materialDesignIconsFont
};

class GuiResourceLookup
{
public:
    GuiResourceLookup(std::filesystem::path executablePath,
                      std::filesystem::path applicationIconInstallPath,
                      std::filesystem::path applicationIconSourcePath,
                      std::filesystem::path materialDesignIconsInstallPath,
                      std::filesystem::path materialDesignIconsSourcePath);

    [[nodiscard]] std::optional<std::filesystem::path> find(GuiResourceId resource) const;

private:
    [[nodiscard]] std::optional<std::filesystem::path> environmentOverride(GuiResourceId resource) const;
    [[nodiscard]] std::filesystem::path installPath(GuiResourceId resource) const;
    [[nodiscard]] const std::filesystem::path & sourcePath(GuiResourceId resource) const;

    std::filesystem::path executablePath_;
    std::filesystem::path applicationIconInstallPath_;
    std::filesystem::path applicationIconSourcePath_;
    std::filesystem::path materialDesignIconsInstallPath_;
    std::filesystem::path materialDesignIconsSourcePath_;
};

[[nodiscard]] GuiResourceLookup defaultGuiResourceLookup();

} // namespace uo_mule::gui
