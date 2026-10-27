#include "gui_model/dialogs/file_deduction.h"
#include "gui_model/dialogs/dialog_contracts.h"
#include "gui_model/dialogs/file_browser_model.h"
#include "uomul/facet_sizes.h"
#include "uomul/map/map.h"
#include "uomul/radarcol/radarcol.h"
#include "uomul/texmaps/texmaps.h"
#include "uomul/tiledata/tiledata.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace uo_mule::gui
{

namespace
{

constexpr int mapCandidateScore = 80;
constexpr int mapFilenameScore = 110;
constexpr int mapSetupScore = 5;
constexpr int radarcolNamedScore = 100;
constexpr int radarcolFilenameScore = 80;
constexpr int radarcolClassicSizeScore = 60;
constexpr int radarcolStructuralScore = 20;
constexpr int texmapsValidatedScore = 140;
constexpr int texmapsHintScore = 70;
constexpr int texmapsFallbackScore = 10;
constexpr int tiledataNamedScore = 100;
constexpr int tiledataSizeScore = 60;
constexpr int filenameHintScoreBonus = 10;
constexpr std::uintmax_t classicRadarcolByteCount = static_cast<std::uintmax_t>(uomul::radarColorCount) * 2U;

[[nodiscard]] std::string lowercase(std::string value)
{
    std::ranges::transform(
        value, value.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

[[nodiscard]] bool filenameContains(std::string_view filename, std::string_view token)
{
    return filename.contains(token);
}

[[nodiscard]] bool isCanonicalFilename(std::string_view filename, std::string_view prefix)
{
    if (!filename.ends_with(".mul"))
    {
        return false;
    }
    const auto stem = filename.substr(0, filename.size() - std::string_view{".mul"}.size());
    if (!stem.starts_with(prefix))
    {
        return false;
    }
    const auto suffix = stem.substr(prefix.size());
    return suffix.empty() || std::ranges::all_of(suffix, [](unsigned char character) { return std::isdigit(character) != 0; });
}

[[nodiscard]] std::filesystem::path texmapsCompanionPath(const std::filesystem::path & path, bool selectedIndex)
{
    const std::string stem = lowercase(path.stem().string());
    constexpr std::string_view indexPrefix = "texidx";
    constexpr std::string_view dataPrefix = "texmaps";
    const std::string_view selectedPrefix = selectedIndex ? indexPrefix : dataPrefix;
    const std::string suffix = stem.starts_with(selectedPrefix) ? stem.substr(selectedPrefix.size()) : "";
    const std::string companion = std::string(selectedIndex ? dataPrefix : indexPrefix) + suffix + path.extension().string();
    return path.parent_path() / companion;
}

void deduceMapCandidates(FileBrowserSummary & summary, const std::filesystem::path & path)
{
    const auto exactFacets = summary.size ? uomul::facetSizesForMapFileBytes(*summary.size) : std::vector<uomul::FacetSize>{};
    for (const auto & facet : exactFacets)
    {
        const auto dimensions = uomul::Dimensions{.widthTiles = facet.widthTiles, .heightTiles = facet.heightTiles};
        auto reader = uomul::MapReader::open(path, dimensions);
        if (!reader)
        {
            continue;
        }

        summary.mapCandidates.push_back(FileBrowserMapCandidate{
            .presetName = std::string(facet.name),
            .facet = std::string(facet.mapLabel),
            .dimensions = dimensions,
            .expectedBlocks = reader->completeBlocks(),
            .completeBlocks = reader->completeBlocks(),
            .missingBlocks = reader->missingBlocks(),
            .trailingBytes = reader->ignoredTrailingBytes(),
            .valid = true,
        });
        summary.assetCandidates.push_back(FileBrowserAssetCandidate{
            .kind = MulAssetKind::map,
            .path = path,
            .companionPath = {},
            .label = "Map",
            .facet = std::string(facet.mapLabel),
            .reason = "Valid map layout for " + std::to_string(dimensions.widthTiles) + " x " +
                      std::to_string(dimensions.heightTiles) + " tiles",
            .warning = isCanonicalFilename(lowercase(path.filename().string()), "map") ? std::string{} : "Unexpected filename",
            .score = isCanonicalFilename(lowercase(path.filename().string()), "map") ? mapFilenameScore : mapCandidateScore,
            .dimensions = dimensions,
            .validated = true,
            .openable = true,
            .filenameWarning = {},
        });
    }
}

void deduceRadarcolCandidates(FileBrowserSummary & summary, const std::filesystem::path & path, std::string_view filename)
{
    if (!summary.readable)
    {
        return;
    }

    const bool namedRadarcol = isCanonicalFilename(filename, "radarcol");
    const bool radarcolFilename = filenameContains(filename, "radarcol");
    const bool classicRadarcolSize = summary.size && *summary.size == classicRadarcolByteCount;
    const auto reader = uomul::RadarColReader::open(path);
    if (!reader)
    {
        return;
    }

    const std::string reason = std::to_string(reader->colorCount()) + " entries";
    int score = radarcolStructuralScore;
    std::string warning;
    if (namedRadarcol)
    {
        score = radarcolNamedScore;
    }
    else if (classicRadarcolSize)
    {
        score = radarcolClassicSizeScore;
    }
    else if (radarcolFilename)
    {
        score = radarcolFilenameScore;
    }
    else
    {
        warning = "Unexpected filename";
    }

    summary.assetCandidates.push_back(FileBrowserAssetCandidate{
        .kind = MulAssetKind::radarcol,
        .path = path,
        .companionPath = {},
        .label = "Radarcol",
        .facet = {},
        .reason = reason,
        .warning = std::move(warning),
        .score = score,
        .itemCount = reader->colorCount(),
        .dimensions = std::nullopt,
        .validated = true,
        .openable = true,
        .filenameWarning = {},
    });
}

void deduceTexmapsCandidates(FileBrowserSummary & summary, const std::filesystem::path & path, std::string_view filename)
{
    const bool selectedIsData = isCanonicalFilename(filename, "texmaps");
    const bool selectedIsIndex = !selectedIsData && isCanonicalFilename(filename, "texidx");
    const bool filenameHint =
        filenameContains(filename, "tex") || filenameContains(filename, "maps") || filenameContains(filename, "idx");

    if (!selectedIsIndex && !selectedIsData)
    {
        summary.assetCandidates.push_back(FileBrowserAssetCandidate{
            .kind = MulAssetKind::texmapsSet,
            .path = path,
            .companionPath = {},
            .label = "Texmaps",
            .facet = {},
            .reason = "texmaps or texidx",
            .warning = "Matching idx/maps file missing",
            .score = filenameHint ? texmapsHintScore + filenameHintScoreBonus : texmapsFallbackScore,
            .dimensions = std::nullopt,
            .validated = false,
            .openable = false,
            .filenameWarning = "Unexpected filename",
        });
        return;
    }

    const std::filesystem::path indexPath = selectedIsIndex ? path : texmapsCompanionPath(path, false);
    const std::filesystem::path dataPath = selectedIsData ? path : texmapsCompanionPath(path, true);
    auto set = inspectTexmapsSet(indexPath, dataPath);
    std::error_code indexError;
    const bool hasIndex = std::filesystem::is_regular_file(indexPath, indexError);
    indexError.clear();
    const bool hasData = std::filesystem::is_regular_file(dataPath, indexError);
    if (!selectedIsIndex && !selectedIsData && !hasIndex && !hasData)
    {
        return;
    }

    set.label = "Texmaps";
    set.reason = set.openable ? std::to_string(set.itemCount) + " entries" : set.reason;
    if (!hasIndex)
    {
        set.warning = "texidx.mul file is not set";
    }
    else if (!hasData)
    {
        set.warning = "texmaps.mul file is not set";
    }
    if (set.openable)
    {
        summary.assetCandidates.push_back(set);
        return;
    }

    summary.assetCandidates.push_back(FileBrowserAssetCandidate{
        .kind = MulAssetKind::texmapsSet,
        .path = path,
        .companionPath = {},
        .label = "Texmaps",
        .facet = {},
        .reason = "A matching Texmaps file set must be set",
        .warning = std::move(set.warning),
        .score = texmapsHintScore + filenameHintScoreBonus,
        .dimensions = std::nullopt,
        .validated = false,
        .openable = false,
        .filenameWarning = {},
    });
}

void deduceTiledataCandidates(FileBrowserSummary & summary, const std::filesystem::path & path, std::string_view filename)
{
    if (!summary.readable || !summary.size)
    {
        return;
    }
    const auto extended = uomul::TiledataReader::open(path, uomul::TiledataLayout::extended64);
    const auto legacy = uomul::TiledataReader::open(path, uomul::TiledataLayout::legacy32);
    if (!extended && !legacy)
    {
        return;
    }

    const auto layout = extended ? uomul::TiledataLayout::extended64 : uomul::TiledataLayout::legacy32;
    const bool named = isCanonicalFilename(filename, "tiledata");
    summary.assetCandidates.push_back(FileBrowserAssetCandidate{
        .kind = MulAssetKind::tiledata,
        .path = path,
        .companionPath = {},
        .label = "Tiledata",
        .facet = {},
        .reason = layout == uomul::TiledataLayout::extended64 ? "Extended64" : "Legacy32",
        .warning = named ? std::string{} : "Unexpected filename",
        .score = named ? tiledataNamedScore : tiledataSizeScore,
        .itemCount = uomul::tiledataMapTileCount + uomul::tiledataStaticTileCount,
        .dimensions = std::nullopt,
        .validated = true,
        .openable = true,
        .filenameWarning = {},
    });
}

void appendMapSetupCandidate(FileBrowserSummary & summary, const std::filesystem::path & path)
{
    if (!summary.readable)
    {
        return;
    }

    summary.assetCandidates.push_back(FileBrowserAssetCandidate{
        .kind = MulAssetKind::map,
        .path = path,
        .companionPath = {},
        .label = "Map (choose dimensions)",
        .facet = {},
        .reason = "Map dimensions must be chosen and validated before opening",
        .warning = "Choose map dimensions",
        .score = mapSetupScore,
        .dimensions = std::nullopt,
        .validated = false,
        .openable = false,
        .filenameWarning = {},
    });
}

} // namespace

void deduceFileCandidates(FileBrowserSummary & summary, const std::filesystem::path & path)
{
    const auto filename = lowercase(path.filename().string());
    deduceMapCandidates(summary, path);
    deduceRadarcolCandidates(summary, path, filename);
    deduceTexmapsCandidates(summary, path, filename);
    deduceTiledataCandidates(summary, path, filename);
    if (!std::ranges::any_of(summary.assetCandidates,
                             [](const auto & candidate) { return candidate.validated && candidate.openable; }))
    {
        appendMapSetupCandidate(summary, path);
    }
}

FileBrowserAssetCandidate inspectTexmapsSet(const std::filesystem::path & indexPath, const std::filesystem::path & dataPath)
{
    const auto reader = uomul::TexMapsReader::open(indexPath, dataPath);
    std::string reason;
    if (reader)
    {
        reason = std::to_string(reader->textureCount()) + " entries";
    }
    else if (reader.error().message.empty())
    {
        reason = "Invalid Texmaps file set";
    }
    else
    {
        reason = reader.error().message;
    }
    return FileBrowserAssetCandidate{
        .kind = MulAssetKind::texmapsSet,
        .path = indexPath,
        .companionPath = dataPath,
        .label = "Texmaps",
        .facet = {},
        .reason = std::move(reason),
        .warning = {},
        .score = texmapsValidatedScore,
        .itemCount = reader ? reader->textureCount() : 0U,
        .dimensions = std::nullopt,
        .validated = reader.has_value(),
        .openable = reader.has_value(),
        .filenameWarning = {},
    };
}

FileBrowserAssetCandidate inspectTexmapsCompanion(const std::filesystem::path & selectedPath,
                                                  const std::filesystem::path & companionPath)
{
    auto selectedAsIndex = inspectTexmapsSet(selectedPath, companionPath);
    if (selectedAsIndex.openable)
    {
        return selectedAsIndex;
    }
    return inspectTexmapsSet(companionPath, selectedPath);
}

std::optional<FileBrowserAssetCandidate>
selectConfidentFileBrowserCandidate(const std::vector<FileBrowserAssetCandidate> & candidates)
{
    const auto isCanonicalCandidate = [](const FileBrowserAssetCandidate & candidate)
    {
        const auto filename = lowercase(candidate.path.filename().string());
        switch (candidate.kind)
        {
            case MulAssetKind::map :
                return isCanonicalFilename(filename, "map");
            case MulAssetKind::radarcol :
                return isCanonicalFilename(filename, "radarcol");
            case MulAssetKind::texmapsSet :
                return isCanonicalFilename(filename, "texidx");
            case MulAssetKind::tiledata :
                return isCanonicalFilename(filename, "tiledata");
            case MulAssetKind::unknown :
                return false;
        }
        return false;
    };
    const auto isConfident = [&](const FileBrowserAssetCandidate & candidate)
    { return candidate.validated && candidate.openable && candidate.warning.empty() && isCanonicalCandidate(candidate); };
    const auto texmaps = std::ranges::find_if(candidates,
                                              [&](const auto & candidate)
                                              { return candidate.kind == MulAssetKind::texmapsSet && isConfident(candidate); });
    if (texmaps != candidates.end())
    {
        return *texmaps;
    }

    const auto confidentCount = std::ranges::count_if(candidates, isConfident);
    if (confidentCount != 1)
    {
        return std::nullopt;
    }
    return *std::ranges::find_if(candidates, isConfident);
}

void sortFileBrowserCandidates(std::vector<FileBrowserAssetCandidate> & candidates)
{
    std::ranges::stable_sort(candidates,
                             [](const auto & left, const auto & right)
                             {
                                 if (left.score != right.score)
                                 {
                                     return left.score > right.score;
                                 }
                                 return left.openable > right.openable;
                             });
}

} // namespace uo_mule::gui
