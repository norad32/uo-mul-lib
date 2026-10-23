#pragma once

#include "gui/documents/document_identity.h"
#include "gui/metadata/document_contracts.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace uo_mule::gui::assets
{

enum class AssetPresentation : std::uint8_t
{
    grid,
    table,
};

enum class AssetPresentationCapability : std::uint8_t
{
    grid = 1U << 0U,
    table = 1U << 1U,
};

enum class AssetQueryCapability : std::uint8_t
{
    filter = 1U << 0U,
    sort = 1U << 1U,
};

enum class AssetItemState : std::uint8_t
{
    available,
    loading,
    empty,
    invalid,
    unavailable,
};

enum class AssetSelectionMode : std::uint8_t
{
    single,
    multiple,
};

struct AssetCapabilitySet
{
    std::uint8_t presentations = static_cast<std::uint8_t>(AssetPresentationCapability::grid);
    std::uint8_t queries = 0;
    AssetSelectionMode selection = AssetSelectionMode::single;

    [[nodiscard]] bool supports(AssetPresentationCapability capability) const noexcept;
    [[nodiscard]] bool supports(AssetQueryCapability capability) const noexcept;
};

struct AssetSourceContext
{
    DocumentKind kind = DocumentKind::map;
    DocumentSourceIdentity source;
    std::string contextId;

    bool operator==(const AssetSourceContext &) const = default;
};

struct AssetQuery
{
    std::string filter;
    std::string sortKey;
    bool sortAscending = true;

    bool operator==(const AssetQuery &) const = default;
};

struct AssetRangeRequest
{
    std::size_t first = 0;
    std::size_t count = 0;
    AssetPresentation presentation = AssetPresentation::grid;
};

struct AssetItemId
{
    std::string value;

    bool operator==(const AssetItemId &) const = default;
};

struct AssetItem
{
    AssetItemId id;
    std::string label;
    AssetItemState state = AssetItemState::unavailable;
    std::string stateMessage;
};

struct AssetDetails
{
    AssetItemId item;
    std::string title;
    std::vector<std::string> fields;
};

struct AssetSelection
{
    std::vector<AssetItemId> selected;
    std::optional<AssetItemId> anchor;
    std::optional<std::size_t> anchorIndex;
};

struct AssetAction
{
    std::string id;
    std::string label;
    bool enabled = true;
    std::string disabledReason;
    std::function<void()> invoke;
};

struct AssetExtensionSlot
{
    std::string id;
    std::string label;
    std::function<void()> draw;
};

struct AssetDragPayload
{
    std::string type;
    AssetSourceContext source;
    std::vector<AssetItemId> items;
};

struct AssetWindowState
{
    AssetPresentation presentation = AssetPresentation::grid;
    std::string presentationContext;
    std::map<std::string, AssetPresentation> presentations;
    AssetQuery query;
    AssetSelection selection;
    AssetSelection selectionDragBase;
    float scrollPosition = 0.0F;
    float selectionDragStartX = 0.0F;
    float selectionDragStartY = 0.0F;
    bool selectionDragActive = false;
    bool detailsCollapsed = false;
};

struct AssetAdapterDescriptor
{
    std::string viewId;
    std::string title;
    std::string sourceTooltip;
    std::function<std::string(const AssetItemId &)> formatSelectionId;
    std::function<void(const AssetItemId &, float)> renderSelectionPreview;
    float selectionPreviewSize = 0.0F;
    AssetSourceContext source;
    AssetCapabilitySet capabilities;
    std::size_t itemCount = 0;
    std::size_t fixedCellWidth = 0;
    std::size_t fixedCellHeight = 0;
    std::vector<AssetAction> actions;
    std::vector<AssetExtensionSlot> extensionSlots;
};

using AssetRangeLoader = std::function<std::vector<AssetItem>(const AssetRangeRequest &, const AssetQuery &)>;
using AssetDetailsLoader = std::function<std::optional<AssetDetails>(const AssetItemId &)>;
using AssetDragPayloadBuilder = std::function<std::optional<AssetDragPayload>(const AssetSelection &)>;
using AssetItemRenderer = std::function<void(const AssetItem &, bool)>;

struct AssetAdapter
{
    AssetAdapterDescriptor descriptor;
    AssetRangeLoader loadRange;
    AssetDetailsLoader loadDetails;
    AssetDragPayloadBuilder makeDragPayload;
    AssetItemRenderer renderItem;
};

[[nodiscard]] bool contains(const AssetSelection & selection, const AssetItemId & item);
void selectSingle(AssetSelection & selection, AssetItemId item);
void selectSingleAt(AssetSelection & selection, AssetItemId item, std::size_t index);
void toggleAt(AssetSelection & selection, AssetItemId item, std::size_t index);
void clear(AssetSelection & selection) noexcept;

} // namespace uo_mule::gui::assets
