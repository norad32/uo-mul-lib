#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace uo_mule::gui
{

struct WorkbenchState;

enum class CommandId : std::uint8_t
{
    fileOpen,
    fileNew,
    fileSaveAs,
    fileDiscard,
    fileClose,
    fileQuit,
    viewTileIds,
    viewZ,
    viewCombined,
    viewToggleLogPanel,
    panelMapCanvas,
    panelTileInspector,
    panelBlockInspector,
    panelSelectionStatistics,
    panelMapInformation,
    panelEditTools,
    panelRadarcol,
    panelTexmaps,
    selectionTiles,
    selectionBlocks,
    editToggle,
    dataPaths,
    resetLayout,
};

struct CommandAvailability
{
    bool enabled = false;
    std::string disabledReason;
};

struct Command
{
    CommandId id;
    std::string stableId;
    std::string label;
    std::string category;
    std::string description;
    std::string defaultShortcut;
    std::function<CommandAvailability(const WorkbenchState &)> availability;
    std::function<void(WorkbenchState &)> execute;
};

class CommandRegistry
{
public:
    void registerCommand(Command command);
    [[nodiscard]] const Command * find(CommandId id) const noexcept;
    [[nodiscard]] const Command * find(std::string_view stableId) const noexcept;
    [[nodiscard]] CommandAvailability availability(CommandId id, const WorkbenchState & state) const;
    [[nodiscard]] bool execute(CommandId id, WorkbenchState & state) const;
    [[nodiscard]] bool executeShortcut(std::string_view shortcut, WorkbenchState & state) const;
    [[nodiscard]] const std::vector<Command> & commands() const noexcept;

private:
    std::vector<Command> commands_;
};

void registerDefaultCommands(CommandRegistry & registry);

} // namespace uo_mule::gui
