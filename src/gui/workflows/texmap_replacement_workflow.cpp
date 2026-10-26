#include "gui/workflows/texmap_replacement_workflow.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/documents/texmaps_document.h"
#include "gui_model/model/assets/asset_window_contracts.h"
#include "image_codec/image_codec.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/texmaps/texmaps.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
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

constexpr int decimalBase = 10;
constexpr int hexadecimalBase = 16;

[[nodiscard]] uomul::Error folderError(const std::filesystem::path & path, std::string message)
{
    return {.code = uomul::ErrorCode::io, .path = path, .message = std::move(message)};
}

[[nodiscard]] std::string lower(std::string value)
{
    std::ranges::transform(
        value, value.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

[[nodiscard]] std::optional<std::uint32_t> parseNumber(std::string_view value, int base) noexcept
{
    if (value.empty())
    {
        return std::nullopt;
    }
    std::uint32_t parsed = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed, base);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
    {
        return std::nullopt;
    }
    return parsed;
}

struct CandidatePaths
{
    std::vector<std::filesystem::path> bmp;
    std::vector<std::filesystem::path> png;
};

[[nodiscard]] std::optional<std::filesystem::path> chooseCandidate(const CandidatePaths & candidates, std::string & error)
{
    if (candidates.bmp.size() > 1U || (candidates.bmp.empty() && candidates.png.size() > 1U))
    {
        error = "multiple image files match this texture ID";
        return std::nullopt;
    }
    if (!candidates.bmp.empty())
    {
        return candidates.bmp.front();
    }
    if (!candidates.png.empty())
    {
        return candidates.png.front();
    }
    error = "no matching BMP or PNG image";
    return std::nullopt;
}

[[nodiscard]] std::optional<uomul::TextureImage> loadCandidate(const CandidatePaths & candidates,
                                                               std::uint32_t & width,
                                                               std::uint32_t & height,
                                                               std::filesystem::path & selectedPath,
                                                               std::string & error)
{
    const auto candidate = chooseCandidate(candidates, error);
    if (!candidate)
    {
        return std::nullopt;
    }

    const auto tryImage = [&](const std::filesystem::path & path) -> std::optional<uomul::TextureImage>
    {
        const auto decoded = image::decode(path);
        if (!decoded)
        {
            error = decoded.error().message;
            return std::nullopt;
        }
        width = decoded->width;
        height = decoded->height;
        const auto texture = image::toTexture(*decoded, path);
        if (!texture)
        {
            error = texture.error().message;
            return std::nullopt;
        }
        selectedPath = path;
        return *texture;
    };

    if (auto texture = tryImage(*candidate))
    {
        return texture;
    }
    if (!candidates.bmp.empty() && !candidates.png.empty() && *candidate == candidates.bmp.front())
    {
        if (candidates.png.size() > 1U)
        {
            error = "multiple image files match this texture ID";
            return std::nullopt;
        }
        if (auto texture = tryImage(candidates.png.front()))
        {
            return texture;
        }
    }
    return std::nullopt;
}

using CandidateMap = std::map<std::uint32_t, CandidatePaths>;

[[nodiscard]] std::expected<CandidateMap, uomul::Error> collectCandidates(const std::filesystem::path & folder)
{
    std::error_code error;
    if (!std::filesystem::is_directory(folder, error) || error)
    {
        return std::unexpected(folderError(folder, error ? error.message() : "replacement path is not a directory"));
    }

    CandidateMap candidates;
    for (const std::filesystem::directory_entry & entry : std::filesystem::directory_iterator(folder, error))
    {
        if (error)
        {
            return std::unexpected(folderError(folder, error.message()));
        }
        if (!entry.is_regular_file(error) || error)
        {
            error.clear();
            continue;
        }
        const auto extension = lower(entry.path().extension().string());
        if (extension != ".bmp" && extension != ".png")
        {
            continue;
        }
        const auto id = parseTexmapImageId(entry.path().stem().string());
        if (!id || *id >= uomul::textureCount)
        {
            continue;
        }
        auto & matching = candidates[*id];
        (extension == ".bmp" ? matching.bmp : matching.png).push_back(entry.path());
    }
    if (error)
    {
        return std::unexpected(folderError(folder, error.message()));
    }
    return candidates;
}

[[nodiscard]] TexmapReplacementRowState invalidState(std::string_view message) noexcept
{
    return message == "multiple image files match this texture ID" ? TexmapReplacementRowState::ambiguous
                                                                   : TexmapReplacementRowState::invalid;
}

[[nodiscard]] std::expected<TexmapReplacementRow, uomul::Error>
scanReplacementRow(std::uint32_t textureId, const CandidateMap & candidates, const TexmapsDocument & document)
{
    TexmapReplacementRow row;
    row.textureId = textureId;
    const auto current = document.effectiveTexture(textureId);
    if (!current)
    {
        return std::unexpected(current.error());
    }
    if (current->has_value())
    {
        row.currentDimension = current->value().dimension;
    }

    const auto candidate = candidates.find(textureId);
    if (candidate == candidates.end())
    {
        row.message = "No matching BMP or PNG image";
        return row;
    }

    std::string errorMessage;
    std::filesystem::path selectedPath;
    const auto replacement =
        loadCandidate(candidate->second, row.replacementWidth, row.replacementHeight, selectedPath, errorMessage);
    if (!replacement)
    {
        row.state = invalidState(errorMessage);
        row.message = std::move(errorMessage);
        return row;
    }

    row.state = TexmapReplacementRowState::valid;
    row.sourcePath = std::move(selectedPath);
    row.filename = row.sourcePath.filename().string();
    row.texture = *replacement;
    if (current->has_value() && current->value() == *row.texture)
    {
        row.noOp = true;
        row.message = "No change";
    }
    return row;
}

} // namespace

std::optional<std::uint32_t> parseTexmapImageId(std::string_view stem) noexcept
{
    if (stem.starts_with("0x") || stem.starts_with("0X"))
    {
        return parseNumber(stem.substr(2U), hexadecimalBase);
    }

    const bool decimal = std::ranges::all_of(stem, [](unsigned char character) { return std::isdigit(character) != 0; });
    return parseNumber(stem, decimal ? decimalBase : hexadecimalBase);
}

std::expected<TexmapFolderReplacement, uomul::Error> scanTexmapReplacementFolder(const std::filesystem::path & folder,
                                                                                 const std::vector<std::uint32_t> & textureIds,
                                                                                 const TexmapsDocument & document)
{
    const auto candidates = collectCandidates(folder);
    if (!candidates)
    {
        return std::unexpected(candidates.error());
    }

    TexmapFolderReplacement result{.folder = folder, .rows = {}};
    result.rows.reserve(textureIds.size());
    for (const auto textureId : textureIds)
    {
        const auto row = scanReplacementRow(textureId, *candidates, document);
        if (!row)
        {
            return std::unexpected(row.error());
        }
        result.rows.push_back(*row);
    }
    return result;
}

bool texmapReplacementRowValid(const TexmapReplacementRow & row) noexcept
{
    return row.state == TexmapReplacementRowState::valid && row.texture.has_value();
}

std::optional<assets::AssetDragPayload> makeTexmapDragPayload(const assets::AssetSelection & selection,
                                                              bool sourceAvailable,
                                                              assets::AssetDragPayloadSource source,
                                                              DocumentId sourceDocumentId)
{
    if (!sourceAvailable || selection.selected.size() != 1U)
    {
        return std::nullopt;
    }
    const auto textureId = assets::itemIndex(selection.selected.front());
    if (!textureId)
    {
        return std::nullopt;
    }
    assets::AssetDragPayload payload;
    payload.type = std::string(assets::texmapValueDragPayloadType);
    payload.label = "Texmap texture";
    payload.values.push_back({
        .item = *textureId,
        .value = *textureId,
        .sourceDocumentId = sourceDocumentId,
        .source = source,
    });
    return payload;
}

} // namespace uo_mule::gui
