#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace uo_mule::gui
{

enum class WindowId : std::uint8_t
{
    radarcol,
    texmaps,
    tiledata,
};

struct WindowDescriptor
{
    WindowId id;
    std::string stableId;
    std::string title;
    bool defaultVisible = true;
    bool visible = true;
};

class WindowRegistry
{
public:
    WindowRegistry();

    [[nodiscard]] WindowDescriptor * find(WindowId id) noexcept;
    [[nodiscard]] const WindowDescriptor * find(WindowId id) const noexcept;
    [[nodiscard]] std::vector<WindowDescriptor> & windows() noexcept;
    [[nodiscard]] const std::vector<WindowDescriptor> & windows() const noexcept;
    void resetToDefaults() noexcept;

private:
    std::vector<WindowDescriptor> windows_;
};

} // namespace uo_mule::gui
