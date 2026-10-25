#include "gui_model/model/assets/asset_window_contracts.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <regex>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace uo_mule::gui::assets
{
namespace
{

struct FilterTerm
{
    std::string value;
    bool regex = false;
};

struct CompiledFilterTerm
{
    std::string value;
    bool regex = false;
    std::optional<std::regex> expression;
};

void skipSeparators(std::string_view filter, std::size_t & index, std::vector<std::vector<FilterTerm>> & groups)
{
    while (index < filter.size() &&
           (std::isspace(static_cast<unsigned char>(filter.at(index))) != 0 || filter.at(index) == ','))
    {
        if (filter.at(index) == ',' && !groups.back().empty())
        {
            groups.emplace_back();
        }
        index++;
    }
}

void appendDelimitedTerm(std::string_view filter, std::size_t & index, std::vector<FilterTerm> & group)
{
    const char delimiter = filter.at(index);
    const bool regex = delimiter == '/';
    index++;
    const std::size_t start = index;
    while (index < filter.size() && filter.at(index) != delimiter)
    {
        index++;
    }
    group.push_back({.value = std::string(filter.substr(start, index - start)), .regex = regex});
    if (index < filter.size())
    {
        index++;
    }
}

void appendPlainTerm(std::string_view filter, std::size_t & index, std::vector<FilterTerm> & group)
{
    const std::size_t start = index;
    while (index < filter.size() && std::isspace(static_cast<unsigned char>(filter.at(index))) == 0 && filter.at(index) != ',')
    {
        index++;
    }
    group.push_back({.value = std::string(filter.substr(start, index - start)), .regex = false});
}

[[nodiscard]] std::vector<std::vector<FilterTerm>> parseFilter(std::string_view filter)
{
    std::vector<std::vector<FilterTerm>> groups(1U);
    std::size_t index = 0U;
    while (index < filter.size())
    {
        skipSeparators(filter, index, groups);
        if (index == filter.size())
        {
            break;
        }

        const char current = filter.at(index);
        if (current == '"' || current == '/')
        {
            appendDelimitedTerm(filter, index, groups.back());
        }
        else
        {
            appendPlainTerm(filter, index, groups.back());
        }
    }
    if (groups.size() == 1U && groups.front().empty())
    {
        groups.clear();
    }
    return groups;
}

[[nodiscard]] std::optional<std::regex> compileExpression(const FilterTerm & term)
{
    if (!term.regex)
    {
        return std::nullopt;
    }
    try
    {
        return std::regex(term.value, std::regex_constants::icase);
    }
    catch (const std::regex_error &)
    {
        return std::nullopt;
    }
}

[[nodiscard]] CompiledFilterTerm compileTerm(const FilterTerm & term)
{
    return {.value = term.value, .regex = term.regex, .expression = compileExpression(term)};
}

[[nodiscard]] bool containsInsensitive(std::string_view value, std::string_view term)
{
    return std::ranges::search(
               value,
               term,
               [](char first, char second)
               { return std::tolower(static_cast<unsigned char>(first)) == std::tolower(static_cast<unsigned char>(second)); })
               .begin() != value.end();
}

[[nodiscard]] bool termMatches(const CompiledFilterTerm & term, std::string_view value)
{
    if (term.regex && term.expression)
    {
        return std::regex_search(value.begin(), value.end(), *term.expression);
    }
    return containsInsensitive(value, term.value);
}

[[nodiscard]] bool groupMatches(const std::vector<CompiledFilterTerm> & group, std::span<const std::string> values)
{
    return group.empty() || std::ranges::any_of(group,
                                                [&values](const CompiledFilterTerm & term)
                                                {
                                                    return std::ranges::any_of(values,
                                                                               [&term](const std::string & value)
                                                                               { return termMatches(term, value); });
                                                });
}

} // namespace

bool AssetCapabilitySet::supports(AssetPresentationCapability capability) const noexcept
{
    return (presentations & static_cast<std::uint8_t>(capability)) != 0U;
}

bool AssetCapabilitySet::supports(AssetQueryCapability capability) const noexcept
{
    return (queries & static_cast<std::uint8_t>(capability)) != 0U;
}

bool filterMatches(const AssetQuery & query, std::span<const std::string> values)
{
    static thread_local std::string compiledFilterText;
    static thread_local std::vector<std::vector<CompiledFilterTerm>> compiledGroups;
    if (compiledFilterText != query.filter)
    {
        compiledFilterText = query.filter;
        compiledGroups.clear();
        for (const auto & group : parseFilter(query.filter))
        {
            auto & compiledGroup = compiledGroups.emplace_back();
            for (const auto & term : group)
            {
                compiledGroup.push_back(compileTerm(term));
            }
        }
    }
    return std::ranges::all_of(compiledGroups, [&values](const auto & group) { return groupMatches(group, values); });
}

bool contains(const AssetSelection & selection, const AssetItemId & item)
{
    return std::ranges::find(selection.selected, item) != selection.selected.end();
}

bool hasStagedSelection(const AssetSelection & selection, const std::function<bool(const AssetItemId &)> & itemStaged)
{
    if (!itemStaged)
    {
        return false;
    }
    return std::ranges::any_of(selection.selected, itemStaged);
}

std::optional<std::uint32_t> itemIndex(const AssetItemId & item) noexcept
{
    std::uint32_t value = 0;
    const auto result = std::from_chars(item.value.data(), item.value.data() + item.value.size(), value);
    if (result.ec != std::errc{} || result.ptr != item.value.data() + item.value.size())
    {
        return std::nullopt;
    }
    return value;
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

void toggleAt(AssetSelection & selection, const AssetItemId & item, std::size_t index)
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

void resetContext(AssetWindowState & state) noexcept
{
    state.query = {};
    state.queryCacheContext.clear();
    state.queryCacheFilter.clear();
    state.searchItems.clear();
    state.searchItemsValid = false;
    state.filteredItems.clear();
    state.queryCacheValid = false;
    state.detailsCache.clear();
    clear(state.selection);
    clear(state.selectionDragBase);
    state.selectionDragStartX = 0.0F;
    state.selectionDragStartY = 0.0F;
    state.selectionDragActive = false;
    state.selectionDragStartedOnItem = false;
    state.scrollPosition = 0.0F;
    state.scrollPositionRestorePending = true;
    state.requestedItem.reset();
    state.requestedItemIndex.reset();
    state.highlightedItem.reset();
    state.highlightUntil = 0.0;
}

} // namespace uo_mule::gui::assets
