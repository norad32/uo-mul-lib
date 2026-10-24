#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace uo_mule::gui
{

enum class ToolId : std::uint8_t
{
    move,
    rectangleSelect,
    lassoSelect,
    fuzzySelect,
    fill,
    replace,
};

enum class ToolCategory : std::uint8_t
{
    navigation,
    selection,
    editing,
};

enum class ToolSelectionGranularity : std::uint8_t
{
    tiles,
    blocks,
};

enum class FillMode : std::uint8_t
{
    id,
    z,
    idAndZ,
};

enum class ReplaceMode : std::uint8_t
{
    id,
    z,
    idAndZ,
};

enum class TilePickTarget : std::uint8_t
{
    activeTile,
    replaceSource,
    replaceTarget,
    fuzzyId,
    fuzzyZ,
    fuzzyName,
};

enum class FuzzyScope : std::uint8_t
{
    contiguous,
    wholeMap,
};

struct FuzzySelectionOptions
{
    FuzzyScope scope = FuzzyScope::contiguous;
    bool idEnabled = false;
    std::uint16_t id = 0;
    bool zEnabled = false;
    std::int8_t z = 0;
    bool nameEnabled = false;
    std::string name;
};

struct ToolContext
{
    bool mapDocumentOpen = false;
    bool mapEditable = false;
    ToolSelectionGranularity selectionGranularity = ToolSelectionGranularity::tiles;
};

struct ToolAvailability
{
    bool enabled = false;
    std::string disabledReason;
};

struct ToolStateChange
{
    bool accepted = false;
    bool changed = false;
    std::string reason;
};

struct ToolDescriptor
{
    ToolId id;
    std::string stableId;
    std::string label;
    ToolCategory category;
    std::string description;
    std::string defaultShortcut;
    [[nodiscard]] ToolAvailability availability(const ToolContext & context) const;
};

class ToolRegistry
{
public:
    ToolRegistry();

    void registerTool(ToolDescriptor descriptor);
    [[nodiscard]] const ToolDescriptor * find(ToolId id) const noexcept;
    [[nodiscard]] const ToolDescriptor * find(std::string_view stableId) const noexcept;
    [[nodiscard]] ToolAvailability availability(ToolId id, const ToolContext & context) const;
    [[nodiscard]] const std::vector<ToolDescriptor> & tools() const noexcept;

private:
    std::vector<ToolDescriptor> tools_;
};

void registerDefaultTools(ToolRegistry & registry);

} // namespace uo_mule::gui
