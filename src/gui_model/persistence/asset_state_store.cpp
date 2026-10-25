#include "gui_model/persistence/asset_state_store.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/model/assets/asset_window_contracts.h"
#include "gui_model/persistence/config_paths.h"
#include "gui_model/source/source_identity.h"
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <istream>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace uo_mule::gui::assets
{
namespace
{

void writeSource(std::ostream & output, const AssetSourceContext & context)
{
    output << "context " << static_cast<int>(context.contextKind) << ' ' << static_cast<int>(context.kind) << ' '
           << std::quoted(context.contextId) << ' ' << context.source.roles.size() << '\n';
    for (const auto & [role, path] : context.source.roles)
    {
        output << "role " << static_cast<int>(role) << ' ' << std::quoted(path.generic_string()) << '\n';
    }
}

bool readRecord(std::istream & file, AssetStateRecord & record)
{
    file >> std::quoted(record.profileIdentity) >> std::quoted(record.assetWindowIdentity);
    std::string marker;
    file >> marker;
    if (marker != "context")
    {
        std::getline(file, marker);
        return false;
    }

    int contextKind = 0;
    int documentKind = 0;
    std::size_t roleCount = 0;
    file >> contextKind >> documentKind >> std::quoted(record.context.contextId) >> roleCount;
    record.context.contextKind = static_cast<AssetSourceContext::Kind>(contextKind);
    record.context.kind = static_cast<DocumentKind>(documentKind);
    for (std::size_t index = 0; index < roleCount; index++)
    {
        file >> marker;
        int role = 0;
        std::string pathValue;
        file >> role >> std::quoted(pathValue);
        record.context.source.roles[static_cast<SourceRole>(role)] = pathValue;
    }

    file >> marker;
    if (marker == "presentation")
    {
        int presentation = 0;
        file >> presentation;
        record.state.presentation = static_cast<AssetPresentation>(presentation);
    }
    file >> marker;
    if (marker == "query")
    {
        file >> std::quoted(record.state.query.filter) >> std::quoted(record.state.query.sortKey) >>
            record.state.query.sortAscending;
    }
    file >> marker;
    if (marker == "details")
    {
        file >> record.state.detailsCollapsed;
    }
    file >> marker;
    if (marker == "scroll")
    {
        file >> record.state.scrollPosition;
    }
    file >> marker;
    if (marker != "selection")
    {
        return true;
    }

    std::size_t selectedCount = 0;
    file >> selectedCount;
    for (std::size_t selected = 0; selected < selectedCount; selected++)
    {
        std::string value;
        file >> std::quoted(value);
        record.state.selection.selected.push_back({.value = std::move(value)});
    }
    bool hasAnchor = false;
    file >> hasAnchor;
    if (hasAnchor)
    {
        std::string value;
        file >> std::quoted(value);
        record.state.selection.anchor = AssetItemId{.value = std::move(value)};
    }
    bool hasAnchorIndex = false;
    file >> hasAnchorIndex;
    if (hasAnchorIndex)
    {
        std::size_t index = 0;
        file >> index;
        record.state.selection.anchorIndex = index;
    }
    return true;
}

} // namespace

std::filesystem::path assetStatePath()
{
    const auto config = configDirectory();
    return config.empty() ? std::filesystem::path{} : config / "uo-mule" / "asset_state.ini";
}

std::vector<AssetStateRecord> loadAssetState(const std::filesystem::path & path)
{
    std::vector<AssetStateRecord> records;
    if (path.empty())
    {
        return records;
    }

    std::ifstream file(path);
    std::string key;
    while (file >> key)
    {
        if (key == "schema_version")
        {
            std::getline(file, key);
        }
        else if (key == "record")
        {
            AssetStateRecord record;
            if (readRecord(file, record))
            {
                records.push_back(std::move(record));
            }
        }
        else
        {
            std::string ignored;
            std::getline(file, ignored);
        }
    }
    return records;
}

void saveAssetState(const std::filesystem::path & path, const std::vector<AssetStateRecord> & records)
{
    if (path.empty())
    {
        return;
    }
    std::filesystem::create_directories(path.parent_path());
    const auto temporary = path.string() + ".tmp";
    std::ofstream file(temporary, std::ios::trunc);
    file << "schema_version " << currentAssetStateSchemaVersion << '\n';
    for (const auto & record : records)
    {
        file << "record " << std::quoted(record.profileIdentity) << ' ' << std::quoted(record.assetWindowIdentity) << ' ';
        writeSource(file, record.context);
        file << "presentation " << static_cast<int>(record.state.presentation) << "\nquery "
             << std::quoted(record.state.query.filter) << ' ' << std::quoted(record.state.query.sortKey) << ' '
             << record.state.query.sortAscending << "\ndetails " << record.state.detailsCollapsed << "\nscroll "
             << record.state.scrollPosition << "\nselection " << record.state.selection.selected.size();
        for (const auto & selected : record.state.selection.selected)
        {
            file << ' ' << std::quoted(selected.value);
        }
        file << ' ' << record.state.selection.anchor.has_value();
        if (const auto & anchor = record.state.selection.anchor; anchor.has_value())
        {
            file << ' ' << std::quoted(anchor->value);
        }
        file << ' ' << record.state.selection.anchorIndex.has_value();
        if (const auto & anchorIndex = record.state.selection.anchorIndex; anchorIndex.has_value())
        {
            file << ' ' << *anchorIndex;
        }
        file << '\n';
    }
    file.close();
    std::filesystem::rename(temporary, path);
}

void resetAssetState(std::vector<AssetStateRecord> & records) noexcept
{
    records.clear();
}

const AssetStateRecord * findAssetState(const std::vector<AssetStateRecord> & records,
                                        const std::string & profileIdentity,
                                        const std::string & assetWindowIdentity,
                                        const AssetSourceContext & context) noexcept
{
    for (const auto & record : records)
    {
        if (record.profileIdentity == profileIdentity && record.assetWindowIdentity == assetWindowIdentity &&
            record.context == context)
        {
            return &record;
        }
    }
    return nullptr;
}

void restoreAssetState(const AssetStateRecord & record, AssetWindowState & state) noexcept
{
    resetContext(state);
    state.presentation = record.state.presentation;
    state.query = record.state.query;
    state.detailsCollapsed = record.state.detailsCollapsed;
    state.scrollPosition = record.state.scrollPosition;
    state.scrollPositionRestorePending = true;
    state.selection = record.state.selection;
}

AssetStateRecord captureAssetState(const std::string & profileIdentity,
                                   const std::string & assetWindowIdentity,
                                   const AssetSourceContext & context,
                                   const AssetWindowState & state)
{
    return {
        .profileIdentity = profileIdentity,
        .assetWindowIdentity = assetWindowIdentity,
        .context = context,
        .state = state,
    };
}

} // namespace uo_mule::gui::assets
