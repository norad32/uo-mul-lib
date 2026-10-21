#pragma once

#include "gui/assets/asset_window_contracts.h"
#include <cstddef>
#include <functional>

namespace uo_mule::gui::assets
{

struct AssetGridLayout
{
    std::size_t columns = 1;
    std::size_t rows = 0;
};

[[nodiscard]] AssetGridLayout gridLayout(std::size_t itemCount,
                                          float availableWidth,
                                          float cellWidth,
                                          float spacing) noexcept;

void drawAssetWindow(const AssetAdapter & adapter,
                     AssetWindowState & state,
                     bool & visible,
                     const std::function<void()> & becameVisible = {});

} // namespace uo_mule::gui::assets
