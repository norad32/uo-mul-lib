#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace uo_mule::gui
{

enum class PanelId : std::uint8_t
{
    tileInspector,
    blockInspector,
    selectionStatistics,
    mapInformation,
    editTools,
    radarcol,
    texmaps,
};

struct PanelDescriptor
{
    PanelId id;
    std::string stableId;
    std::string title;
    bool defaultVisible = true;
    bool visible = true;
};

class PanelRegistry
{
public:
    PanelRegistry();

    [[nodiscard]] PanelDescriptor * find(PanelId id) noexcept;
    [[nodiscard]] const PanelDescriptor * find(PanelId id) const noexcept;
    [[nodiscard]] std::vector<PanelDescriptor> & panels() noexcept;
    [[nodiscard]] const std::vector<PanelDescriptor> & panels() const noexcept;
    void resetToDefaults() noexcept;

private:
    std::vector<PanelDescriptor> panels_;
};

} // namespace uo_mule::gui
