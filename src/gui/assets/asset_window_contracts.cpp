#include "gui/assets/asset_window_contracts.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <regex>
#include <string_view>
#include <utility>

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

[[nodiscard]] std::vector<std::vector<FilterTerm>> parseFilter(std::string_view filter)
{
    std::vector<std::vector<FilterTerm>> groups(1U);
    std::size_t index = 0U;
    while (index < filter.size())
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
        if (index == filter.size())
        {
            break;
        }

        const char delimiter = filter.at(index) == '"' || filter.at(index) == '/' ? filter.at(index) : '\0';
        if (delimiter != '\0')
        {
            const bool regex = delimiter == '/';
            const std::size_t start = ++index;
            while (index < filter.size() && filter.at(index) != delimiter)
            {
                index++;
            }
            groups.back().push_back({.value = std::string(filter.substr(start, index - start)), .regex = regex});
            if (index < filter.size())
            {
                index++;
            }
            continue;
        }

        const std::size_t start = index;
        while (index < filter.size() && std::isspace(static_cast<unsigned char>(filter.at(index))) == 0 && filter.at(index) != ',')
        {
            index++;
        }
        groups.back().push_back({.value = std::string(filter.substr(start, index - start)), .regex = false});
    }
    if (groups.size() == 1U && groups.front().empty())
    {
        groups.clear();
    }
    return groups;
}

[[nodiscard]] bool containsInsensitive(std::string_view value, std::string_view term)
{
    return std::ranges::search(value, term, [](char first, char second) {
               return std::tolower(static_cast<unsigned char>(first)) == std::tolower(static_cast<unsigned char>(second));
           }).begin() != value.end();
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
                std::optional<std::regex> expression;
                if (term.regex)
                {
                    try
                    {
                        expression.emplace(term.value, std::regex_constants::icase);
                    }
                    catch (const std::regex_error &)
                    {
                    }
                }
                compiledGroup.push_back({.value = term.value, .regex = term.regex, .expression = std::move(expression)});
            }
        }
    }
    for (const auto & group : compiledGroups)
    {
        bool groupMatched = false;
        for (const auto & term : group)
        {
            for (const auto & value : values)
            {
                bool matched = false;
                if (term.regex)
                {
                    if (term.expression)
                    {
                        matched = std::regex_search(value, *term.expression);
                    }
                    else
                    {
                        matched = containsInsensitive(value, term.value);
                    }
                }
                else
                {
                    matched = containsInsensitive(value, term.value);
                }
                if (matched)
                {
                    groupMatched = true;
                    break;
                }
            }
            if (groupMatched)
            {
                break;
            }
        }
        if (!group.empty() && !groupMatched)
        {
            return false;
        }
    }
    return true;
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
