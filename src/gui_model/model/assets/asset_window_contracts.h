#pragma once

#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace uo_mule::gui::assets
{

inline constexpr std::string_view assetValueDragPayloadType = "uo_mule.asset-value";
inline constexpr std::string_view texmapValueDragPayloadType = "uo_mule.texmap-value";

enum class AssetDragPayloadSource : std::uint8_t
{
    provider,
    document,
};

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
    enum class Kind : std::uint8_t
    {
        metadataView,
        document,
    };

    DocumentKind kind = DocumentKind::map;
    Kind contextKind = Kind::document;
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

struct AssetSearchItem
{
    AssetItem item;
    std::vector<std::string> values;
};

struct AssetDetails
{
    AssetItemId item;
    std::string title;
    std::vector<std::string> fields;
    std::vector<std::string> fieldTooltips;
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

struct AssetDragPayloadValue
{
    std::uint32_t item = 0;
    std::uint32_t value = 0;
    std::uint64_t sourceDocumentId = 0;
    AssetDragPayloadSource source = AssetDragPayloadSource::provider;
};

struct AssetDragPayload
{
    std::string type;
    std::string label;
    AssetSourceContext source;
    std::vector<AssetItemId> items;
    std::vector<AssetDragPayloadValue> values;
};

struct AssetWindowState
{
    AssetPresentation presentation = AssetPresentation::grid;
    std::string presentationContext;
    std::map<std::string, AssetPresentation> presentations;
    AssetQuery query;
    std::string queryCacheContext;
    std::string queryCacheFilter;
    std::vector<AssetSearchItem> searchItems;
    bool searchItemsValid = false;
    std::vector<AssetItem> filteredItems;
    bool queryCacheValid = false;
    std::map<std::string, std::optional<AssetDetails>> detailsCache;
    AssetSelection selection;
    AssetSelection selectionDragBase;
    float scrollPosition = 0.0F;
    bool scrollPositionRestorePending = true;
    float selectionDragStartX = 0.0F;
    float selectionDragStartY = 0.0F;
    bool selectionDragActive = false;
    bool selectionDragStartedOnItem = false;
    bool detailsCollapsed = false;
    std::function<void()> selectAll;
    std::function<void()> selectNone;
    std::function<void()> invertSelection;
    std::optional<AssetItemId> requestedItem;
    std::optional<std::size_t> requestedItemIndex;
    std::optional<AssetItemId> highlightedItem;
    double highlightUntil = 0.0;
    std::function<void(bool)> focusResult;
};

void resetContext(AssetWindowState & state) noexcept;

struct AssetAdapterDescriptor
{
    std::string viewId;
    std::string windowIdentity;
    std::string title;
    std::string sourceTooltip;
    std::string cacheContext;
    AssetPresentation defaultPresentation = AssetPresentation::grid;
    std::function<void(AssetPresentation)> presentationChanged;
    bool queryWhenEmpty = false;
    std::function<std::string(const AssetItemId &)> formatSelectionId;
    std::function<void(const AssetItemId &, float)> renderSelectionPreview;
    std::function<void(const AssetItemId &)> itemPicked;
    std::function<void(const AssetItemId &)> itemDoubleClicked;
    std::function<bool(const AssetItemId &)> itemSourcePicked;
    std::function<void(const AssetItemId &)> itemContextMenu;
    std::string acceptedDragPayloadType;
    std::function<void(const AssetItemId &, const AssetDragPayloadValue &)> itemValueDropped;
    std::function<bool(const AssetItemId &)> itemStaged;
    std::function<void(const AssetSelection &)> itemRevert;
    float selectionPreviewSize = 0.0F;
    std::function<void()> inlinePresentationControl;
    AssetSourceContext source;
    AssetCapabilitySet capabilities;
    std::function<std::size_t(const AssetQuery &)> queryItemCount;
    std::size_t itemCount = 0;
    std::size_t fixedCellWidth = 0;
    std::size_t fixedCellHeight = 0;
    std::optional<float> gridItemSpacing;
    std::vector<AssetAction> actions;
    std::vector<AssetExtensionSlot> extensionSlots;
    std::function<void()> presentationTrailingControl;
    std::function<void(std::string_view)> hoveredContext;
};

using AssetRangeLoader = std::function<std::vector<AssetItem>(const AssetRangeRequest &, const AssetQuery &)>;
using AssetSearchItemLoader = std::function<std::vector<AssetSearchItem>()>;
using AssetDetailsLoader = std::function<std::optional<AssetDetails>(const AssetItemId &)>;
using AssetDragPayloadBuilder = std::function<std::optional<AssetDragPayload>(const AssetSelection &)>;
using AssetItemRenderer = std::function<void(const AssetItem &, bool)>;

struct AssetAdapter
{
    AssetAdapterDescriptor descriptor;
    AssetRangeLoader loadRange;
    AssetSearchItemLoader loadSearchItems;
    AssetDetailsLoader loadDetails;
    AssetDragPayloadBuilder makeDragPayload;
    AssetItemRenderer renderItem;
};

[[nodiscard]] bool contains(const AssetSelection & selection, const AssetItemId & item);
[[nodiscard]] bool hasStagedSelection(const AssetSelection & selection,
                                      const std::function<bool(const AssetItemId &)> & itemStaged);
[[nodiscard]] std::optional<std::uint32_t> itemIndex(const AssetItemId & item) noexcept;
[[nodiscard]] bool filterMatches(const AssetQuery & query, std::span<const std::string> values);
void selectSingle(AssetSelection & selection, AssetItemId item);
void selectSingleAt(AssetSelection & selection, AssetItemId item, std::size_t index);
void toggleAt(AssetSelection & selection, const AssetItemId & item, std::size_t index);
void clear(AssetSelection & selection) noexcept;

} // namespace uo_mule::gui::assets
