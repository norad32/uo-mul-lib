#include "gui/application/gui_resources.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace
{

class TemporaryDirectory
{
public:
    TemporaryDirectory() : path_(std::filesystem::temp_directory_path() / "uo-mule-gui-resources")
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory &) = delete;
    TemporaryDirectory & operator=(const TemporaryDirectory &) = delete;

    [[nodiscard]] const std::filesystem::path & path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

class EnvironmentOverride
{
public:
    EnvironmentOverride(const char * name, const std::filesystem::path & value) : name_(name)
    {
        if (const char * previous = std::getenv(name); previous != nullptr)
        {
            previous_ = previous;
        }
        setenv(name, value.c_str(), 1);
    }

    ~EnvironmentOverride()
    {
        if (previous_.has_value())
        {
            setenv(name_.c_str(), previous_->c_str(), 1);
        }
        else
        {
            unsetenv(name_.c_str());
        }
    }

    EnvironmentOverride(const EnvironmentOverride &) = delete;
    EnvironmentOverride & operator=(const EnvironmentOverride &) = delete;

private:
    std::string name_;
    std::optional<std::string> previous_;
};

void clearOverrides()
{
    unsetenv("IM_CPP_ICON_PATH");
    unsetenv("IM_CPP_FONT_PATH");
}

} // namespace

TEST_CASE("guiResourceLookup_ShouldPreferEnvironmentOverride")
{
    clearOverrides();
    TemporaryDirectory directory;
    const auto source = directory.path() / "source.ttf";
    const auto installed = directory.path() / "installed.ttf";
    const auto overridePath = directory.path() / "override.ttf";
    std::ofstream(source).put('s');
    std::ofstream(installed).put('i');
    std::ofstream(overridePath).put('o');

    const uo_mule::gui::GuiResourceLookup lookup(
        directory.path() / "bin/application", installed.filename(), source, installed.filename(), source);
    const EnvironmentOverride override("IM_CPP_FONT_PATH", overridePath);

    REQUIRE(lookup.find(uo_mule::gui::GuiResourceId::materialDesignIconsFont) == overridePath);
}

TEST_CASE("guiResourceLookup_ShouldPreferInstalledPathBeforeBuildTreePath")
{
    clearOverrides();
    TemporaryDirectory directory;
    const auto executable = directory.path() / "bin/application";
    const auto installed = directory.path() / "installed.ttf";
    const auto source = directory.path() / "source.ttf";
    std::filesystem::create_directories(executable.parent_path());
    std::ofstream(installed).put('i');
    std::ofstream(source).put('s');

    const uo_mule::gui::GuiResourceLookup lookup(executable, "../installed.ttf", source, "../installed.ttf", source);

    REQUIRE(lookup.find(uo_mule::gui::GuiResourceId::materialDesignIconsFont) == installed);
}

TEST_CASE("guiResourceLookup_ShouldFindBuildTreePathWhenInstalledPathIsMissing")
{
    clearOverrides();
    TemporaryDirectory directory;
    const auto source = directory.path() / "source.ttf";
    std::ofstream(source).put('s');

    const uo_mule::gui::GuiResourceLookup lookup(
        directory.path() / "bin/application", "../missing.ttf", source, "../missing.ttf", source);

    REQUIRE(lookup.find(uo_mule::gui::GuiResourceId::materialDesignIconsFont) == source);
}

TEST_CASE("guiResourceLookup_ShouldReportMissingResource")
{
    clearOverrides();
    TemporaryDirectory directory;
    const uo_mule::gui::GuiResourceLookup lookup(directory.path() / "bin/application",
                                                 "../missing-icon.xpm",
                                                 directory.path() / "missing-icon.xpm",
                                                 "../missing.ttf",
                                                 directory.path() / "missing.ttf");

    REQUIRE_FALSE(lookup.find(uo_mule::gui::GuiResourceId::applicationIcon).has_value());
    REQUIRE_FALSE(lookup.find(uo_mule::gui::GuiResourceId::materialDesignIconsFont).has_value());
}
