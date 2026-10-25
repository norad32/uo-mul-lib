#pragma once

#include "gui_model/model/assets/asset_window_contracts.h"
#include "gui_model/persistence/preferences.h"
#include <functional>
#include <string>
#include <string_view>

namespace uo_mule::gui
{

struct NotificationState;

struct AssetAdapterCommonOptions
{
    std::string viewId;
    std::string windowIdentity;
    std::string title;
    std::string sourceTooltip;
    std::string cacheContext;
    assets::AssetSourceContext source;
    std::function<void(const assets::AssetItemId &)> itemPicked;
    std::function<void(std::string_view)> hoveredContext;
};

void configureAssetAdapter(assets::AssetAdapterDescriptor & descriptor, AssetAdapterCommonOptions options);
[[nodiscard]] std::string formatAssetItemId(const assets::AssetItemId & item, IdFormat idFormat);
void clearAssetNavigationStatus(NotificationState & notifications);

} // namespace uo_mule::gui
