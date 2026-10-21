#include "gui/assets/asset_window_contracts.h"
#include <algorithm>
#include <cstdint>
#include <utility>

namespace uo_mule::gui::assets
{

bool AssetCapabilitySet::supports(AssetPresentationCapability capability) const noexcept
{
    return (presentations & static_cast<std::uint8_t>(capability)) != 0U;
}

bool AssetCapabilitySet::supports(AssetQueryCapability capability) const noexcept
{
    return (queries & static_cast<std::uint8_t>(capability)) != 0U;
}

bool contains(const AssetSelection & selection, const AssetItemId & item)
{
    return std::ranges::find(selection.selected, item) != selection.selected.end();
}

void selectSingle(AssetSelection & selection, AssetItemId item)
{
    selection.selected.clear();
    selection.selected.push_back(std::move(item));
    selection.anchor = selection.selected.front();
}

void clear(AssetSelection & selection) noexcept
{
    selection.selected.clear();
    selection.anchor.reset();
}

} // namespace uo_mule::gui::assets
