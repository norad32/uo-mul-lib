#include "gui_model/persistence/preferences.h"
#include "core/app_info.h"
#include "gui_model/persistence/config_paths.h"
#include "gui_model/persistence/preferences_store.h"
#include "gui_model/tools/tool_contracts.h"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>

namespace uo_mule::gui
{
namespace
{

[[nodiscard]] std::string_view toolName(ToolId tool) noexcept
{
    switch (tool)
    {
        case ToolId::move :
            return "pan";
        case ToolId::rectangleSelect :
            return "rectangle";
        case ToolId::lassoSelect :
            return "free";
        case ToolId::fuzzySelect :
            return "fuzzy";
        case ToolId::fill :
            return "fill";
        case ToolId::replace :
            return "replace";
    }
    return "pan";
}

[[nodiscard]] ToolId parseTool(std::string_view value) noexcept
{
    if (value == "rectangle")
        return ToolId::rectangleSelect;
    if (value == "free")
        return ToolId::lassoSelect;
    if (value == "fuzzy")
        return ToolId::fuzzySelect;
    if (value == "fill")
        return ToolId::fill;
    if (value == "replace")
        return ToolId::replace;
    return ToolId::move;
}

[[nodiscard]] std::string_view granularityName(ToolSelectionGranularity granularity) noexcept
{
    return granularity == ToolSelectionGranularity::blocks ? "blocks" : "tiles";
}

[[nodiscard]] ToolSelectionGranularity parseGranularity(std::string_view value) noexcept
{
    return value == "blocks" ? ToolSelectionGranularity::blocks : ToolSelectionGranularity::tiles;
}

[[nodiscard]] std::string_view fuzzyScopeName(FuzzyScope scope) noexcept
{
    return scope == FuzzyScope::wholeMap ? "whole" : "contiguous";
}

[[nodiscard]] FuzzyScope parseFuzzyScope(std::string_view value) noexcept
{
    return value == "whole" ? FuzzyScope::wholeMap : FuzzyScope::contiguous;
}

[[nodiscard]] std::int32_t parseInteger(const std::string & value, std::int32_t fallback) noexcept
{
    try
    {
        return static_cast<std::int32_t>(std::stol(value));
    }
    catch (...)
    {
        return fallback;
    }
}

std::atomic<std::uint64_t> temporaryFileCounter = 0;
constexpr std::uint32_t maximumTemporaryFileAttempts = 128;
constexpr std::int32_t minimumZ = -128;
constexpr std::int32_t maximumZ = 127;
constexpr std::int32_t maximumTileId = 65535;

[[nodiscard]] int createTemporaryPreferencesFile(std::string & path)
{
    const std::string basePath = path;
    for (std::uint32_t attempt = 0; attempt < maximumTemporaryFileAttempts; attempt++)
    {
        path = basePath + ".tmp." + std::to_string(getpid()) + "." + std::to_string(temporaryFileCounter.fetch_add(1));
        const int descriptor = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
        if (descriptor != -1 || errno != EEXIST)
        {
            if (descriptor == -1)
            {
                throw std::system_error(errno, std::generic_category(), "Cannot create temporary preferences file");
            }

            return descriptor;
        }
    }

    throw std::system_error(EEXIST, std::generic_category(), "Cannot create temporary preferences file");
}

[[nodiscard]] std::optional<std::filesystem::path> preferencesPath()
{
    const auto config = configDirectory();
    if (config.empty())
    {
        return std::nullopt;
    }
    return config / std::string(core::applicationName()) / "preferences.conf";
}

class TemporaryPreferencesFile
{
public:
    explicit TemporaryPreferencesFile(const std::filesystem::path & path) :
        path_(path.string()), descriptor_(createTemporaryPreferencesFile(path_))
    {
    }

    ~TemporaryPreferencesFile()
    {
        if (descriptor_ != -1)
        {
            close(descriptor_);
        }

        if (!committed_)
        {
            unlink(path_.c_str());
        }
    }

    TemporaryPreferencesFile(const TemporaryPreferencesFile &) = delete;
    TemporaryPreferencesFile & operator=(const TemporaryPreferencesFile &) = delete;
    TemporaryPreferencesFile(TemporaryPreferencesFile &&) = delete;
    TemporaryPreferencesFile & operator=(TemporaryPreferencesFile &&) = delete;

    void write(std::string_view contents) const
    {
        while (!contents.empty())
        {
            const ssize_t bytesWritten = ::write(descriptor_, contents.data(), contents.size());
            if (bytesWritten == -1)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                throw std::system_error(errno, std::generic_category(), "Cannot write preferences file");
            }
            contents.remove_prefix(static_cast<std::size_t>(bytesWritten));
        }
    }

    void commit(const std::filesystem::path & destination)
    {
        if (fsync(descriptor_) == -1)
        {
            throw std::system_error(errno, std::generic_category(), "Cannot sync preferences file");
        }

        if (close(descriptor_) == -1)
        {
            descriptor_ = -1;
            throw std::system_error(errno, std::generic_category(), "Cannot close preferences file");
        }

        descriptor_ = -1;

        if (rename(path_.c_str(), destination.c_str()) == -1)
        {
            throw std::system_error(errno, std::generic_category(), "Cannot replace preferences file");
        }

        committed_ = true;
    }

private:
    std::string path_;
    int descriptor_ = -1;
    bool committed_ = false;
};

void applyGeneralPreference(Preferences & preferences, std::string_view key, std::string_view value)
{
    if (key == "logs_panel_visible")
    {
        preferences.showLogPanel = value == "1";
    }
    else if (key == "logs_autoscroll")
    {
        preferences.autoScroll = value == "1";
    }
    else if (key == "logs_panel_height")
    {
        std::istringstream valueStream{std::string(value)};
        if (valueStream >> preferences.logPanelHeight)
        {
            preferences.logPanelHeight = std::max(0.0F, preferences.logPanelHeight);
        }
    }
    else if (key == "logs_panel_collapsed")
    {
        preferences.logPanelCollapsed = value == "1";
    }
    else if (key == "show_welcome_on_startup")
    {
        preferences.showWelcomeOnStartup = value == "1";
    }
    else if (key == "id_format")
    {
        preferences.idFormat = value == "decimal" ? IdFormat::decimal : IdFormat::hexadecimal;
    }
    else if (key == "texmaps_hide_unused")
    {
        preferences.texmapsHideUnused = value == "1";
    }
    else if (key == "texmaps_table_presentation")
    {
        preferences.texmapsTablePresentation = value == "1";
    }
    else if (key == "workspace_fullscreen")
    {
        preferences.workspaceFullscreen = value == "1";
    }
    else if (key == "map_view_tile_source")
    {
        preferences.mapViewTileSource = value == "radarcol" ? MapViewTileSource::radarcol : MapViewTileSource::textures;
    }
}

void applyToolPreference(Preferences & preferences, std::string_view key, std::string_view value)
{
    if (key == "tool_active")
    {
        preferences.tools.activeTool = parseTool(value);
    }
    else if (key == "tool_region")
    {
        preferences.tools.selectionGranularity = parseGranularity(value);
    }
    else if (key == "tool_active_tile_set")
    {
        preferences.tools.activeTileSet = value == "1";
    }
    else if (key == "tool_active_tile_id")
    {
        preferences.tools.activeTile.id =
            static_cast<std::uint16_t>(std::clamp(parseInteger(std::string(value), 0), 0, maximumTileId));
    }
    else if (key == "tool_active_tile_z")
    {
        preferences.tools.activeTile.z =
            static_cast<std::int8_t>(std::clamp(parseInteger(std::string(value), 0), minimumZ, maximumZ));
    }
    else if (key == "tool_fill_id")
    {
        preferences.tools.fillIdEnabled = value == "1";
    }
    else if (key == "tool_fill_z")
    {
        preferences.tools.fillZEnabled = value == "1";
    }
    else if (key == "tool_replace_id")
    {
        preferences.tools.replaceIdEnabled = value == "1";
    }
    else if (key == "tool_replace_z")
    {
        preferences.tools.replaceZEnabled = value == "1";
    }
    else if (key == "tool_replace_source_id")
    {
        preferences.tools.replaceSource.id =
            static_cast<std::uint16_t>(std::clamp(parseInteger(std::string(value), 0), 0, maximumTileId));
    }
    else if (key == "tool_replace_source_z")
    {
        preferences.tools.replaceSource.z =
            static_cast<std::int8_t>(std::clamp(parseInteger(std::string(value), 0), minimumZ, maximumZ));
    }
    else if (key == "tool_replace_target_id")
    {
        preferences.tools.replaceTarget.id =
            static_cast<std::uint16_t>(std::clamp(parseInteger(std::string(value), 0), 0, maximumTileId));
    }
    else if (key == "tool_replace_target_z")
    {
        preferences.tools.replaceTarget.z =
            static_cast<std::int8_t>(std::clamp(parseInteger(std::string(value), 0), minimumZ, maximumZ));
    }
    else if (key == "tool_fuzzy_scope")
    {
        preferences.tools.fuzzy.scope = parseFuzzyScope(value);
    }
    else if (key == "tool_fuzzy_id_enabled")
    {
        preferences.tools.fuzzy.idEnabled = value == "1";
    }
    else if (key == "tool_fuzzy_id")
    {
        preferences.tools.fuzzy.id =
            static_cast<std::uint16_t>(std::clamp(parseInteger(std::string(value), 0), 0, maximumTileId));
    }
    else if (key == "tool_fuzzy_z_enabled")
    {
        preferences.tools.fuzzy.zEnabled = value == "1";
    }
    else if (key == "tool_fuzzy_z")
    {
        preferences.tools.fuzzy.z =
            static_cast<std::int8_t>(std::clamp(parseInteger(std::string(value), 0), minimumZ, maximumZ));
    }
    else if (key == "tool_fuzzy_name_enabled")
    {
        preferences.tools.fuzzy.nameEnabled = value == "1";
    }
    else if (key == "tool_fuzzy_name")
    {
        preferences.tools.fuzzy.name = value;
    }
}

void applyPreference(Preferences & preferences, std::string_view key, std::string_view value)
{
    if (key.starts_with("tool_"))
    {
        applyToolPreference(preferences, key, value);
        return;
    }
    applyGeneralPreference(preferences, key, value);
}

} // namespace

Preferences loadPreferences()
{
    const auto path = preferencesPath();
    return path ? loadPreferences(*path) : Preferences{};
}

Preferences loadPreferences(const std::filesystem::path & path)
{
    Preferences preferences;
    std::ifstream file(path);
    std::string key;
    std::string value;
    while (file >> key)
    {
        if (key == "active_data_profile")
        {
            file >> std::quoted(preferences.activeDataProfile);
            continue;
        }

        if (key == "tool_fuzzy_name")
        {
            file >> std::quoted(preferences.tools.fuzzy.name);
            continue;
        }

        if (!(file >> value))
        {
            break;
        }
        applyPreference(preferences, key, value);
    }
    return preferences;
}

void savePreferences(const Preferences & preferences)
{
    const auto path = preferencesPath();
    if (path)
    {
        savePreferences(preferences, *path);
    }
}

void savePreferences(const Preferences & preferences, const std::filesystem::path & path)
{
    std::filesystem::create_directories(path.parent_path());
    std::ostringstream contents;
    contents << "logs_panel_visible " << static_cast<int>(preferences.showLogPanel) << '\n';
    contents << "logs_autoscroll " << static_cast<int>(preferences.autoScroll) << '\n';
    contents << "logs_panel_height " << preferences.logPanelHeight << '\n';
    contents << "logs_panel_collapsed " << static_cast<int>(preferences.logPanelCollapsed) << '\n';
    contents << "active_data_profile " << std::quoted(preferences.activeDataProfile) << '\n';
    contents << "show_welcome_on_startup " << static_cast<int>(preferences.showWelcomeOnStartup) << '\n';
    contents << "id_format " << (preferences.idFormat == IdFormat::decimal ? "decimal" : "hexadecimal") << '\n';
    contents << "texmaps_hide_unused " << static_cast<int>(preferences.texmapsHideUnused) << '\n';
    contents << "texmaps_table_presentation " << static_cast<int>(preferences.texmapsTablePresentation) << '\n';
    contents << "workspace_fullscreen " << static_cast<int>(preferences.workspaceFullscreen) << '\n';
    contents << "map_view_tile_source "
             << (preferences.mapViewTileSource == MapViewTileSource::radarcol ? "radarcol" : "textures") << '\n';
    contents << "tool_active " << toolName(preferences.tools.activeTool) << '\n';
    contents << "tool_region " << granularityName(preferences.tools.selectionGranularity) << '\n';
    contents << "tool_active_tile_set " << static_cast<int>(preferences.tools.activeTileSet) << '\n';
    contents << "tool_active_tile_id " << preferences.tools.activeTile.id << '\n';
    contents << "tool_active_tile_z " << static_cast<int>(preferences.tools.activeTile.z) << '\n';
    contents << "tool_fill_id " << static_cast<int>(preferences.tools.fillIdEnabled) << '\n';
    contents << "tool_fill_z " << static_cast<int>(preferences.tools.fillZEnabled) << '\n';
    contents << "tool_replace_id " << static_cast<int>(preferences.tools.replaceIdEnabled) << '\n';
    contents << "tool_replace_z " << static_cast<int>(preferences.tools.replaceZEnabled) << '\n';
    contents << "tool_replace_source_id " << preferences.tools.replaceSource.id << '\n';
    contents << "tool_replace_source_z " << static_cast<int>(preferences.tools.replaceSource.z) << '\n';
    contents << "tool_replace_target_id " << preferences.tools.replaceTarget.id << '\n';
    contents << "tool_replace_target_z " << static_cast<int>(preferences.tools.replaceTarget.z) << '\n';
    contents << "tool_fuzzy_scope " << fuzzyScopeName(preferences.tools.fuzzy.scope) << '\n';
    contents << "tool_fuzzy_id_enabled " << static_cast<int>(preferences.tools.fuzzy.idEnabled) << '\n';
    contents << "tool_fuzzy_id " << preferences.tools.fuzzy.id << '\n';
    contents << "tool_fuzzy_z_enabled " << static_cast<int>(preferences.tools.fuzzy.zEnabled) << '\n';
    contents << "tool_fuzzy_z " << static_cast<int>(preferences.tools.fuzzy.z) << '\n';
    contents << "tool_fuzzy_name_enabled " << static_cast<int>(preferences.tools.fuzzy.nameEnabled) << '\n';
    contents << "tool_fuzzy_name " << std::quoted(preferences.tools.fuzzy.name) << '\n';
    TemporaryPreferencesFile temporaryFile(path);
    temporaryFile.write(contents.str());
    temporaryFile.commit(path);
}

} // namespace uo_mule::gui
