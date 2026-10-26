#include "gui/presentation/asset_browser_helpers.h"
#include "gui/presentation/strings.h"
#include "gui/workbench/notification_state.h"
#include "gui_model/model/assets/asset_window_contracts.h"
#include "gui_model/persistence/preferences.h"
#include <cstdint>
#include <utility>

namespace uo_mule::gui
{

void configureAssetAdapter(assets::AssetAdapterDescriptor & descriptor, AssetAdapterCommonOptions options)
{
    descriptor.viewId = std::move(options.viewId);
    descriptor.windowIdentity = std::move(options.windowIdentity);
    descriptor.title = std::move(options.title);
    descriptor.sourceTooltip = std::move(options.sourceTooltip);
    descriptor.cacheContext = std::move(options.cacheContext);
    descriptor.source = std::move(options.source);
    descriptor.itemPicked = std::move(options.itemPicked);
    descriptor.hoveredContext = std::move(options.hoveredContext);
}

std::string formatAssetItemId(const assets::AssetItemId & item, IdFormat idFormat)
{
    return assets::itemIndex(item)
        .transform([idFormat](std::uint32_t id) { return strings::formatId(id, idFormat); })
        .value_or(item.value);
}

void clearAssetNavigationStatus(NotificationState & notifications)
{
    std::erase_if(notifications.entries,
                  [](const Notification & notification)
                  {
                      return notification.message == "Tile is not visible with the current filter." ||
                             notification.message == "Texture is not visible with the current filter.";
                  });
}

} // namespace uo_mule::gui
