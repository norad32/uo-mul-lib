#pragma once

#include "gui_model/documents/texmaps_document.h"
#include "image_codec/image_codec.h"
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace uo_mule::gui
{

enum class TexmapReplacementRowState : std::uint8_t
{
    missing,
    ambiguous,
    invalid,
    valid,
};

struct TexmapReplacementRow
{
    std::uint32_t textureId = 0;
    TexmapReplacementRowState state = TexmapReplacementRowState::missing;
    std::filesystem::path sourcePath;
    std::string filename;
    std::uint32_t currentDimension = 0;
    std::uint32_t replacementWidth = 0;
    std::uint32_t replacementHeight = 0;
    std::optional<uomul::TextureImage> texture;
    bool noOp = false;
    std::string message;
};

struct TexmapFolderReplacement
{
    std::filesystem::path folder;
    std::vector<TexmapReplacementRow> rows;
};

[[nodiscard]] std::optional<std::uint32_t> parseTexmapImageId(std::string_view stem) noexcept;

[[nodiscard]] std::expected<TexmapFolderReplacement, uomul::Error>
scanTexmapReplacementFolder(const std::filesystem::path & folder,
                            const std::vector<std::uint32_t> & textureIds,
                            const TexmapsDocument & document);

[[nodiscard]] bool texmapReplacementRowValid(const TexmapReplacementRow & row) noexcept;

} // namespace uo_mule::gui
