#include "app/map_cli.h"
#include "uomul/map.h"

#include <CLI/CLI.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <ostream>
#include <string>

namespace im_cpp::app
{
namespace
{
[[nodiscard]] std::expected<uomul::Dimensions, std::string> dimensions(const MapCommandOptions & options)
{
    if (!options.facet.empty())
    {
        std::string name = options.facet;
        std::ranges::transform(
            name, name.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        auto preset = uomul::facetDimensions(name);
        if (!preset)
        {
            return std::unexpected("Unknown facet preset: " + options.facet);
        }
        if (options.width != 0 || options.height != 0)
        {
            return std::unexpected("Use either --facet or both --width and --height");
        }
        return *preset;
    }
    if (options.width == 0 || options.height == 0)
    {
        return std::unexpected("Specify --facet or both --width and --height");
    }
    return uomul::Dimensions{.widthTiles = options.width, .heightTiles = options.height};
}

[[nodiscard]] std::string errorText(const uomul::Error & error)
{
    std::string result = error.path.empty() ? std::string{} : error.path.string() + ": ";
    result += error.message;
    if (error.offset != 0)
    {
        result += " (byte offset " + std::to_string(error.offset) + ')';
    }
    return result;
}

[[nodiscard]] int fail(const uomul::Error & error, std::ostream & outputError)
{
    outputError << errorText(error) << '\n';
    return 1;
}

[[nodiscard]] CLI::App *
configureOperation(CLI::App & parent, const std::string & name, const std::string & description, MapCommandOptions & options)
{
    auto * command = parent.add_subcommand(name, description);
    command->add_option("--map", options.path, "Input mapN.mul")->required();
    command->add_option("--width", options.width, "Facet width in tiles");
    command->add_option("--height", options.height, "Facet height in tiles");
    command->add_option("--facet", options.facet, "Facet preset: felucca, trammel, ilshenar, malas, tokuno, ter-mur");
    command->add_flag("--recover", options.recover, "Read available complete blocks from a truncated map");
    return command;
}

} // namespace

CLI::App * configureMapCommand(CLI::App & application, MapCommandOptions & options)
{
    auto * map = application.add_subcommand("map", "Inspect and edit land map MUL files");
    auto * inspect = configureOperation(*map, "inspect", "Read a land tile or block", options);
    auto * inspectBlockX = inspect->add_option("--block-x", options.blockX, "Block X coordinate");
    auto * inspectBlockY = inspect->add_option("--block-y", options.blockY, "Block Y coordinate");
    auto * inspectTileX = inspect->add_option("--tile-x", options.tileX, "Global tile X coordinate");
    auto * inspectTileY = inspect->add_option("--tile-y", options.tileY, "Global tile Y coordinate");
    inspectBlockX->each([&options](const std::string &) { options.useBlock = true; });
    inspectTileX->each([&options](const std::string &) { options.useTile = true; });
    inspectBlockX->excludes(inspectTileY);
    inspectBlockX->needs(inspectBlockY);
    inspectBlockY->needs(inspectBlockX);
    inspectTileX->needs(inspectTileY);
    inspectTileY->needs(inspectTileX);
    inspectBlockX->excludes(inspectTileX);
    inspectBlockY->excludes(inspectTileY);
    auto * edit = configureOperation(*map, "edit", "Edit land tiles and regions", options);
    edit->add_option("--output", options.outputPath, "Output map MUL")->required();
    edit->add_flag("--overwrite", options.overwrite, "Replace an existing output file");
    auto * editBlockX = edit->add_option("--block-x", options.blockX, "Block X coordinate");
    auto * editBlockY = edit->add_option("--block-y", options.blockY, "Block Y coordinate");
    auto * editTileX = edit->add_option("--tile-x", options.tileX, "Global tile X coordinate");
    auto * editTileY = edit->add_option("--tile-y", options.tileY, "Global tile Y coordinate");
    editBlockX->each([&options](const std::string &) { options.useBlock = true; });
    editTileX->each([&options](const std::string &) { options.useTile = true; });
    editBlockX->needs(editBlockY);
    editBlockY->needs(editBlockX);
    editTileX->needs(editTileY);
    editTileY->needs(editTileX);
    editBlockX->excludes(editTileX);
    editBlockY->excludes(editTileY);
    auto * editX = edit->add_option("--x", options.x, "Region minimum X");
    auto * editY = edit->add_option("--y", options.y, "Region minimum Y");
    editX->needs(editY);
    editY->needs(editX);
    editX->excludes(editBlockX);
    editX->excludes(editTileX);
    auto * regionWidth = edit->add_option("--region-width", options.regionWidth, "Region width in tiles");
    auto * regionHeight = edit->add_option("--region-height", options.regionHeight, "Region height in tiles");
    regionWidth->needs(regionHeight);
    regionHeight->needs(regionWidth);
    auto * landId = edit->add_option("--id", options.landId, "Replacement land tile ID");
    auto * landZ = edit->add_option("--z", options.z, "Replacement elevation");
    landId->needs(landZ);
    landZ->needs(landId);
    landId->excludes(editBlockX);
    landId->excludes(editTileX);
    auto * fromId = edit->add_option("--from-id", options.fromId, "Land ID to replace");
    auto * toId = edit->add_option("--to-id", options.toId, "Replacement land ID");
    fromId->needs(toId);
    toId->needs(fromId);
    fromId->excludes(landId);
    fromId->excludes(editBlockX);
    fromId->excludes(editTileX);
    fromId->each([&options](const std::string &) { options.replaceIdsSelected = true; });
    auto * fromZ = edit->add_option("--from-z", options.fromZ, "Elevation to replace");
    auto * toZ = edit->add_option("--to-z", options.toZ, "Replacement elevation");
    landId->excludes(fromId);
    landId->excludes(fromZ);
    fromId->excludes(fromZ);
    fromId->excludes(toZ);
    fromZ->needs(toZ);
    toZ->needs(fromZ);
    fromZ->excludes(landId);
    fromZ->excludes(editBlockX);
    fromZ->excludes(editTileX);
    fromZ->each([&options](const std::string &) { options.replaceZSelected = true; });
    auto * fillFlag = edit->add_flag("--fill", options.fillRegion, "Fill the selected region with --id and --z");
    fillFlag->needs(landId);
    landId->excludes(fromZ);
    landId->excludes(fromId);
    fromId->excludes(fromZ);
    fillFlag->needs(editX);
    auto * diff = map->add_subcommand("diff", "Generate, apply, or report land-only map diffs");
    auto * generate = diff->add_subcommand("generate", "Generate mapdifN.mul and mapdiflN.mul");
    generate->add_option("--base", options.path, "Base map MUL")->required();
    generate->add_option("--target", options.otherPath, "Target map MUL")->required();
    generate->add_option("--directory", options.diffDirectory, "Diff output directory")->required();
    generate->add_option("--map-index", options.mapIndex, "Facet index")->required();
    generate->add_option("--width", options.width, "Facet width in tiles");
    generate->add_option("--height", options.height, "Facet height in tiles");
    generate->add_option("--facet", options.facet, "Facet preset");
    generate->add_flag("--overwrite", options.overwrite, "Replace existing diff files");
    auto * apply = diff->add_subcommand("apply", "Apply a land diff to a base map");
    apply->add_option("--base", options.path, "Base map MUL")->required();
    apply->add_option("--directory", options.diffDirectory, "Directory containing the diff pair")->required();
    apply->add_option("--output", options.outputPath, "Output map MUL")->required();
    apply->add_option("--map-index", options.mapIndex, "Facet index")->required();
    apply->add_option("--width", options.width, "Facet width in tiles");
    apply->add_option("--height", options.height, "Facet height in tiles");
    apply->add_option("--facet", options.facet, "Facet preset");
    apply->add_flag("--overwrite", options.overwrite, "Replace an existing output file");
    auto * report = diff->add_subcommand("report", "Write a CSV land-map diff report");
    report->add_option("--source", options.path, "Source map MUL")->required();
    report->add_option("--target", options.otherPath, "Target map MUL")->required();
    report->add_option("--output", options.reportPath, "CSV report path")->required();
    report->add_option("--width", options.width, "Facet width in tiles");
    report->add_option("--height", options.height, "Facet height in tiles");
    report->add_option("--facet", options.facet, "Facet preset");
    report->add_flag("--overwrite", options.overwrite, "Replace an existing report");
    inspect->callback([&options] { options.operation = "inspect"; });
    edit->callback([&options] { options.operation = "edit"; });
    generate->callback([&options] { options.operation = "generate"; });
    apply->callback([&options] { options.operation = "apply"; });
    report->callback([&options] { options.operation = "report"; });
    map->require_subcommand(1);
    diff->require_subcommand(1);
    return map;
}

int runMapCommand(const MapCommandOptions & options, std::ostream & output, std::ostream & error)
{
    const auto mapDimensions = dimensions(options);
    if (!mapDimensions)
    {
        error << mapDimensions.error() << '\n';
        return 2;
    }
    const uomul::Dimensions dimensionsValue = *mapDimensions;
    constexpr std::int32_t minimumElevationValue = std::numeric_limits<std::int8_t>::min();
    constexpr std::int32_t maximumElevationValue = std::numeric_limits<std::int8_t>::max();
    constexpr std::uint32_t maximumLandId = std::numeric_limits<std::uint16_t>::max();
    if (options.operation == "edit" &&
        ((options.landId > maximumLandId) || (options.fromId > maximumLandId) || (options.toId > maximumLandId) ||
         options.z < minimumElevationValue || options.z > maximumElevationValue || options.fromZ < minimumElevationValue ||
         options.fromZ > maximumElevationValue || options.toZ < minimumElevationValue || options.toZ > maximumElevationValue))
    {
        error << "land IDs must be in [0, 65535] and elevations in [-128, 127]\n";
        return 2;
    }
    if (options.operation == "inspect")
    {
        auto reader =
            uomul::MapReader::open(options.path, dimensionsValue, uomul::OpenOptions{.recoverTruncatedMap = options.recover});
        if (!reader)
        {
            return fail(reader.error(), error);
        }
        if (reader->ignoredTrailingBytes() != 0 || reader->missingBlocks() != 0)
        {
            error << "Warning: recovery ignored trailing_bytes=" << reader->ignoredTrailingBytes()
                  << " and missingBlocks=" << reader->missingBlocks() << '\n';
        }
        if (options.useBlock)
        {
            auto block = reader->readLandBlock({.x = options.blockX, .y = options.blockY});
            if (!block)
            {
                return fail(block.error(), error);
            }
            output << "block " << options.blockX << ',' << options.blockY << '\n';
            std::size_t index = 0;
            for (const auto & tile : *block)
            {
                output << index << ' ' << tile.id << ' ' << static_cast<int>(tile.z) << '\n';
                ++index;
            }
            return 0;
        }
        if (options.useTile)
        {
            auto tile = reader->readLandTile({.x = options.tileX, .y = options.tileY});
            if (!tile)
            {
                return fail(tile.error(), error);
            }
            output << "tile " << options.tileX << ',' << options.tileY << " id=" << tile->id
                   << " z=" << static_cast<int>(tile->z) << '\n';
            return 0;
        }
        output << "dimensions=" << dimensionsValue.widthTiles << 'x' << dimensionsValue.heightTiles
               << " blocks=" << reader->completeBlocks() << " bytes_ignored=" << reader->ignoredTrailingBytes() << '\n';
        return 0;
    }
    if (options.operation == "edit")
    {
        auto editor = uomul::MapEditor::create(
            options.path, options.outputPath, dimensionsValue, uomul::EditOptions{.overwriteOutput = options.overwrite});
        if (!editor)
        {
            return fail(editor.error(), error);
        }
        std::expected<void, uomul::Error> result;
        if (options.fillRegion)
        {
            const std::uint32_t width = options.regionWidth == 0 ? 1 : options.regionWidth;
            const std::uint32_t height = options.regionHeight == 0 ? 1 : options.regionHeight;
            if (options.x > std::numeric_limits<std::uint32_t>::max() - width ||
                options.y > std::numeric_limits<std::uint32_t>::max() - height)
            {
                error << "region coordinates overflow\n";
                return 2;
            }
            result = editor->fill(
                {.min = {.x = options.x, .y = options.y}, .maxExclusive = {.x = options.x + width, .y = options.y + height}},
                {.id = static_cast<std::uint16_t>(options.landId), .z = static_cast<std::int8_t>(options.z)});
        }
        else if (options.replaceIdsSelected)
        {
            const std::uint32_t width = options.regionWidth == 0 && options.x <= dimensionsValue.widthTiles
                                            ? dimensionsValue.widthTiles - options.x
                                            : options.regionWidth;
            const std::uint32_t height = options.regionHeight == 0 && options.y <= dimensionsValue.heightTiles
                                             ? dimensionsValue.heightTiles - options.y
                                             : options.regionHeight;
            if (options.x > std::numeric_limits<std::uint32_t>::max() - width ||
                options.y > std::numeric_limits<std::uint32_t>::max() - height)
            {
                error << "region coordinates overflow\n";
                return 2;
            }
            result = editor->replaceIds(
                {.min = {.x = options.x, .y = options.y}, .maxExclusive = {.x = options.x + width, .y = options.y + height}},
                static_cast<std::uint16_t>(options.fromId),
                static_cast<std::uint16_t>(options.toId));
        }
        else if (options.replaceZSelected)
        {
            const std::uint32_t width = options.regionWidth == 0 && options.x <= dimensionsValue.widthTiles
                                            ? dimensionsValue.widthTiles - options.x
                                            : options.regionWidth;
            const std::uint32_t height = options.regionHeight == 0 && options.y <= dimensionsValue.heightTiles
                                             ? dimensionsValue.heightTiles - options.y
                                             : options.regionHeight;
            if (options.x > std::numeric_limits<std::uint32_t>::max() - width ||
                options.y > std::numeric_limits<std::uint32_t>::max() - height)
            {
                error << "region coordinates overflow\n";
                return 2;
            }
            result = editor->replaceZ(
                {.min = {.x = options.x, .y = options.y}, .maxExclusive = {.x = options.x + width, .y = options.y + height}},
                static_cast<std::int8_t>(options.fromZ),
                static_cast<std::int8_t>(options.toZ));
        }
        else if (options.useBlock)
        {
            auto block = uomul::LandBlock{};
            block.fill({.id = static_cast<std::uint16_t>(options.landId), .z = static_cast<std::int8_t>(options.z)});
            result = editor->writeLandBlock({.x = options.blockX, .y = options.blockY}, block);
        }
        else if (options.useTile)
        {
            result = editor->writeLandTile(
                {.x = options.tileX, .y = options.tileY},
                {.id = static_cast<std::uint16_t>(options.landId), .z = static_cast<std::int8_t>(options.z)});
        }
        else
        {
            error << "Specify --tile-x/--tile-y, --block-x/--block-y, --fill --x/--y, --from-id/--to-id, or --from-z/--to-z\n";
            return 2;
        }
        if (!result)
        {
            return fail(result.error(), error);
        }
        result = editor->commit();
        if (!result)
        {
            return fail(result.error(), error);
        }
        output << "Wrote " << options.outputPath << '\n';
        return 0;
    }
    if (options.operation == "generate")
    {
        auto result = uomul::MapDiff::generate(
            options.path, options.otherPath, options.diffDirectory, options.mapIndex, dimensionsValue, options.overwrite);
        if (!result)
        {
            return fail(result.error(), error);
        }
        output << "changed_blocks=" << result->landBlocksChanged << " changed_tiles=" << result->landTilesChanged << '\n';
        return 0;
    }
    if (options.operation == "apply")
    {
        auto result = uomul::MapDiff::apply(
            options.path, options.diffDirectory, options.outputPath, options.mapIndex, dimensionsValue, options.overwrite);
        if (!result)
        {
            return fail(result.error(), error);
        }
        output << "Wrote " << options.outputPath << '\n';
        return 0;
    }
    if (options.operation == "report")
    {
        auto result =
            uomul::MapDiff::report(options.path, options.otherPath, options.reportPath, dimensionsValue, options.overwrite);
        if (!result)
        {
            return fail(result.error(), error);
        }
        output << "changed_blocks=" << result->landBlocksChanged << " changed_tiles=" << result->landTilesChanged << '\n';
        return 0;
    }
    error << "No map operation selected\n";
    return 2;
}

} // namespace im_cpp::app
