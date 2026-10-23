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
    selection.anchorIndex.reset();
}

void selectSingleAt(AssetSelection & selection, AssetItemId item, std::size_t index)
{
    selectSingle(selection, std::move(item));
    selection.anchorIndex = index;
}

void toggleAt(AssetSelection & selection, AssetItemId item, std::size_t index)
{
    const auto found = std::ranges::find(selection.selected, item);
    if (found == selection.selected.end())
    {
        selection.selected.push_back(item);
    }
    else
    {
        selection.selected.erase(found);
    }
    selection.anchor = item;
    selection.anchorIndex = index;
}

void clear(AssetSelection & selection) noexcept
{
    selection.selected.clear();
    selection.anchor.reset();
    selection.anchorIndex.reset();
}

} // namespace uo_mule::gui::assets
