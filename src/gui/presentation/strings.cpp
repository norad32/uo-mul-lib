#include "gui/presentation/strings.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/providers/data_providers.h"
#include "gui_model/session/workbench_session.h"
#include "uomul/mul_common/mul_common.h"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>

namespace uo_mule::gui::strings
{
const char * open() noexcept
{
    return "Open";
}
const char * ok() noexcept
{
    return "OK";
}
const char * cancel() noexcept
{
    return "Cancel";
}
const char * retry() noexcept
{
    return "Retry";
}
const char * save() noexcept
{
    return "Save";
}
const char * saveAs() noexcept
{
    return "Save As...";
}
const char * discard() noexcept
{
    return "Discard";
}
const char * overwrite() noexcept
{
    return "Overwrite";
}
const char * filter() noexcept
{
    return "Filter";
}
const char * loading() noexcept
{
    return "Loading...";
}
const char * noItems() noexcept
{
    return "No items to display";
}
const char * noItemSelected() noexcept
{
    return "No item selected";
}
const char * itemDetailsUnavailable() noexcept
{
    return "Item details unavailable";
}
const char * assetViewUnavailable() noexcept
{
    return "This asset view is not available yet.";
}
const char * grid() noexcept
{
    return "Grid";
}
const char * table() noexcept
{
    return "Table";
}
const char * hideEmpty() noexcept
{
    return "Hide empty";
}
const char * dimensions() noexcept
{
    return "Dimensions";
}
const char * facet() noexcept
{
    return "Facet";
}
const char * width() noexcept
{
    return "Width";
}
const char * height() noexcept
{
    return "Height";
}
const char * customDimensions() noexcept
{
    return "Custom dimensions";
}
const char * invalidDimensions() noexcept
{
    return "Dimensions must be positive multiples of 8 tiles.";
}
const char * unsupportedFacetSelection() noexcept
{
    return "Select a supported facet preset or custom dimensions";
}
const char * noExactFacetMatch() noexcept
{
    return "No exact known size match, custom dimensions is selected.";
}
const char * raw() noexcept
{
    return "Raw";
}
const char * hexadecimal() noexcept
{
    return "Hex";
}
const char * hexadecimalName() noexcept
{
    return "Hexadecimal";
}

const char * profile() noexcept
{
    return "Profile";
}
const char * name() noexcept
{
    return "Name";
}
const char * openProfile() noexcept
{
    return "Open Profile";
}
const char * none() noexcept
{
    return "None";
}
const char * deleteProfile() noexcept
{
    return "Delete";
}
const char * searchFolder() noexcept
{
    return "Search folder";
}
const char * searchMetadataFolder() noexcept
{
    return "Search Metadata Folder";
}
const char * showWelcomeScreen() noexcept
{
    return "Show welcome at startup";
}
const char * decimal() noexcept
{
    return "Decimal";
}
const char * mapViewTiles() noexcept
{
    return "Map tiles";
}
const char * texturesRadarcolFallback() noexcept
{
    return "Textures";
}
const char * welcome() noexcept
{
    return "Welcome";
}
const char * recentFiles() noexcept
{
    return "Recent Files";
}
const char * noFilesOpened() noexcept
{
    return "No files opened yet";
}
const char * openFileToAdd() noexcept
{
    return "Open a file to add it to this list.";
}
const char * configurePreferences() noexcept
{
    return "Configure the active data profile and application preferences.";
}
const char * showOnStartup() noexcept
{
    return "Show on startup";
}
const char * showOnStartupTooltip() noexcept
{
    return "Show the welcome screen after a valid profile is loaded when the application starts.";
}
const char * autoScroll() noexcept
{
    return "Autoscroll";
}
const char * pick() noexcept
{
    return "Pick";
}
const char * fromActiveTile() noexcept
{
    return "From Active Tile";
}
std::string formatProfilesStoredIn(const std::filesystem::path & path)
{
    return "Profiles are stored in " + path.string();
}
const char * chooseMetadataFile() noexcept
{
    return "Choose Metadata File";
}
const char * configurePreferencesLabel() noexcept
{
    return "Configure Preferences";
}
const char * mapsWithoutProfile() noexcept
{
    return "Maps are available without a profile. Embedded Radarcol is used until an external source is configured.";
}
const char * openPreferences() noexcept
{
    return "Open Preferences";
}
std::string formatProfileReadiness(std::string_view readiness)
{
    return "Profile status: " + std::string(readiness);
}
std::string formatRadarcolSource(std::string_view source)
{
    return "Radarcol source: " + std::string(source);
}
std::string formatCanvasNoTile(std::string_view canvas, double zoom)
{
    return std::format("{}: No tile under pointer  Zoom {:.3g}x", canvas, zoom);
}
std::string formatCanvasTile(std::string_view canvas,
                             std::uint32_t tileX,
                             std::uint32_t tileY,
                             std::uint32_t blockX,
                             std::uint32_t blockY,
                             double zoom)
{
    return std::format("{}: Tile ({}, {})  Block ({}, {})  Zoom {:.3g}x", canvas, tileX, tileY, blockX, blockY, zoom);
}
std::string formatDocumentNoView(std::string_view kind)
{
    return std::string(kind) + " is active, but has no view yet.";
}
const char * documentPlaceholder() noexcept
{
    return "This document type is represented as a placeholder until its view is implemented.";
}
const char * helpMenu() noexcept
{
    return "Help";
}
const char * undo() noexcept
{
    return "Undo";
}
const char * redo() noexcept
{
    return "Redo";
}
const char * cut() noexcept
{
    return "Cut";
}
const char * paste() noexcept
{
    return "Paste";
}
const char * configurePreferencesWindow() noexcept
{
    return "Configure Preferences###startup.configure-profile";
}
const char * formatLabel() noexcept
{
    return "Format";
}
const char * goToCoordinate() noexcept
{
    return "Go to Coordinate...";
}
const char * goToBlock() noexcept
{
    return "Go to Block...";
}
const char * openEllipsis() noexcept
{
    return "Open...";
}
const char * createNewEllipsis() noexcept
{
    return "New...";
}
const char * home() noexcept
{
    return "Home";
}
const char * showHidden() noexcept
{
    return "Show hidden";
}
const char * openMulFile() noexcept
{
    return "Open mul file";
}
const char * desktop() noexcept
{
    return "Desktop";
}
const char * documents() noexcept
{
    return "Documents";
}
const char * downloads() noexcept
{
    return "Downloads";
}
const char * music() noexcept
{
    return "Music";
}
const char * pictures() noexcept
{
    return "Pictures";
}
const char * videos() noexcept
{
    return "Videos";
}
const char * root() noexcept
{
    return "Root";
}
const char * places() noexcept
{
    return "Places";
}
const char * recentLocations() noexcept
{
    return "Recent Locations";
}
const char * fileSummary() noexcept
{
    return "File Summary";
}
const char * selectFileMetadata() noexcept
{
    return "Select a file to inspect its metadata.";
}
const char * map() noexcept
{
    return "Map";
}
const char * openable() noexcept
{
    return "Openable";
}
const char * invalidMapCandidate() noexcept
{
    return "Invalid map candidate";
}
const char * requiresSelection() noexcept
{
    return "Requires selection";
}
const char * pathNotDirectory() noexcept
{
    return "Path is not a directory";
}
const char * chooseValidMulOutputPath() noexcept
{
    return "Choose a valid .mul output path";
}
const char * chooseMatchingFileOrDirectory() noexcept
{
    return "Choose a matching file or directory";
}
const char * back() noexcept
{
    return "Back";
}
const char * forward() noexcept
{
    return "Forward";
}
const char * up() noexcept
{
    return "Up";
}
const char * refresh() noexcept
{
    return "Refresh";
}
const char * filename() noexcept
{
    return "Filename";
}
const char * sizeLabel() noexcept
{
    return "Size";
}
const char * dateLabel() noexcept
{
    return "Date";
}
const char * files() noexcept
{
    return "Files";
}
const char * summary() noexcept
{
    return "Summary";
}
const char * selectFolder() noexcept
{
    return "Select Folder";
}
const char * create() noexcept
{
    return "Create";
}
const char * createFolder() noexcept
{
    return "Create folder";
}
const char * folderName() noexcept
{
    return "Folder name";
}
const char * newFolder() noexcept
{
    return "New Folder";
}
const char * invalidFolderName() noexcept
{
    return "Enter a valid folder name";
}
const char * folderAlreadyExists() noexcept
{
    return "A folder with that name already exists";
}
const char * metadataPaths() noexcept
{
    return "Metadata Paths";
}
const char * settings() noexcept
{
    return "Settings";
}
const char * tiledataFormat() noexcept
{
    return "Tiledata format";
}
const char * idFormat() noexcept
{
    return "ID Format";
}
const char * entriesFormat() noexcept
{
    return "%u entries";
}
const char * sizeFormat() noexcept
{
    return "Size: %s";
}
const char * modifiedFormat() noexcept
{
    return "Modified: %s";
}
const char * facetFormat() noexcept
{
    return "Facet: %s";
}
const char * dimensionsFormat() noexcept
{
    return "Dimensions: %u x %u";
}
const char * blocksFormat() noexcept
{
    return "Blocks: %llu / %llu";
}
const char * missingFormat() noexcept
{
    return "Missing: %llu";
}
const char * trailingBytesFormat() noexcept
{
    return "Trailing bytes: %llu";
}
const char * folderFormat() noexcept
{
    return "Folder: %s";
}
std::string formatEditingColors(std::size_t count)
{
    return "Editing " + std::to_string(count) + " color" + (count == 1U ? "" : "s");
}
const char * mixedColorValues() noexcept
{
    return "Selected colors have different values.\nApply assigns one value to all.";
}
const char * multipleColorValues() noexcept
{
    return "Multiple colors are being edited.\nApply assigns one value to all.";
}

const char * filterSyntaxTooltip() noexcept
{
    return "Words, \"phrases\", or /regex/. Spaces mean OR, commas mean AND.";
}

const char * dimensionsUnknown() noexcept
{
    return "Dimensions unknown";
}

const char * customMap() noexcept
{
    return "Custom";
}

const char * modified() noexcept
{
    return "Modified";
}

const char * opened() noexcept
{
    return "Opened";
}

const char * unknown() noexcept
{
    return "Unknown";
}

std::string formatTimestamp(std::string_view prefix, std::string_view value)
{
    return std::string(prefix) + ": " + std::string(value);
}

std::string formatDimensions(std::uint32_t width, std::uint32_t height)
{
    return std::format("{} x {}", width, height);
}

std::string formatEntries(std::size_t count)
{
    return std::format("{} entries", count);
}

std::string formatVersion(std::string_view version)
{
    return "Version " + std::string(version);
}

std::string formatFontScale(bool iconFontAvailable, double scaleX, double scaleY)
{
    return std::format(
        "Font: {} | Display scale: {:.2g}x, {:.2g}x", iconFontAvailable ? "available" : "text fallback", scaleX, scaleY);
}

std::string formatTile(std::uint32_t x, std::uint32_t y)
{
    return std::format("Tile: ({}, {})", x, y);
}

std::string formatBlock(std::uint32_t x, std::uint32_t y)
{
    return std::format("Block: ({}, {})", x, y);
}

std::string formatTileCoordinates(std::uint32_t minX, std::uint32_t maxX, std::uint32_t minY, std::uint32_t maxY)
{
    return std::format("Tile coordinates: X {}-{}, Y {}-{}", minX, maxX, minY, maxY);
}

std::string formatSelectedTile(std::uint32_t x, std::uint32_t y, std::uint16_t id, std::int32_t z)
{
    return std::format("Selected tile ({}, {}): ID {}, Z {}", x, y, id, z);
}

std::string formatTileTooltip(std::uint32_t x, std::uint32_t y, std::uint16_t id, std::int32_t z)
{
    return std::format("Tile ({}, {}): ID {}, Z {}", x, y, id, z);
}

const char * id() noexcept
{
    return "ID";
}

const char * tileId() noexcept
{
    return "Tile ID";
}

const char * elevation() noexcept
{
    return "Elevation";
}

const char * mode() noexcept
{
    return "Mode";
}

const char * targetUsesActiveTile() noexcept
{
    return "Target uses Active Tile.";
}

const char * source() noexcept
{
    return "Source";
}

const char * target() noexcept
{
    return "Target";
}

const char * scope() noexcept
{
    return "Scope";
}

const char * tools() noexcept
{
    return "Tools";
}

const char * toolOptions() noexcept
{
    return "Tool Options";
}

const char * activeTile() noexcept
{
    return "Active Tile";
}

const char * region() noexcept
{
    return "Region";
}

const char * tiles() noexcept
{
    return "Tiles";
}

const char * blocks() noexcept
{
    return "Blocks";
}

const char * tiledataNamesUnavailable() noexcept
{
    return "Tiledata names unavailable.";
}

const char * noProfile() noexcept
{
    return "No profile";
}

const char * configured() noexcept
{
    return "Complete";
}

const char * incomplete() noexcept
{
    return "Incomplete";
}

const char * notConfigured() noexcept
{
    return "Missing";
}

const char * semanticIconVocabulary() noexcept
{
    return "Semantic icon vocabulary";
}

const char * fileMenu() noexcept
{
    return "File";
}
const char * viewMenu() noexcept
{
    return "View";
}
const char * editMenu() noexcept
{
    return "Edit";
}
const char * preferences() noexcept
{
    return "Preferences";
}
const char * radarcol() noexcept
{
    return "Radarcol";
}
const char * radarcolFallbackTitle() noexcept
{
    return "Radarcol fallback";
}

const char * radarcolFallbackMessage() noexcept
{
    return "Embedded Radarcol colors are active so map editing can continue.";
}

const char * radarcolFallbackQuestion() noexcept
{
    return "Would you like to create a default radarcol.mul file for this profile?";
}

const char * createDefaultRadarcol() noexcept
{
    return "Create Default Radarcol";
}

const char * defaultRadarcolCreated() noexcept
{
    return "Default Radarcol created and configured";
}

const char * continueWithEmbedded() noexcept
{
    return "Continue with Embedded";
}

const char * closeWithoutSaving() noexcept
{
    return "Close without saving";
}

const char * windowsMenu() noexcept
{
    return "Windows";
}

const char * selectMenu() noexcept
{
    return "Select";
}

const char * worldMenu() noexcept
{
    return "World";
}

const char * noMapOpen() noexcept
{
    return "No map open";
}

const char * noTileUnderPointerOrSelected() noexcept
{
    return "No tile under pointer or selected";
}

const char * hoverBlockToInspect() noexcept
{
    return "Hover a block in the map canvas to inspect it";
}

const char * clear() noexcept
{
    return "Clear";
}

const char * sourceTexmapUnavailable() noexcept
{
    return "The source Texmap texture is unavailable.";
}

const char * sourceTexmapUnsupportedDimensions() noexcept
{
    return "The source Texmap texture has unsupported dimensions.";
}

const char * sourceTexmapInvalidPixels() noexcept
{
    return "The source Texmap texture has invalid pixel data.";
}

const char * sourceTexmapsDocumentClosed() noexcept
{
    return "The source Texmaps document is no longer open.";
}

const char * sourceTexmapSlotEmpty() noexcept
{
    return "The source Texmap slot is empty.";
}

const char * selectedTexmapSlotEmpty() noexcept
{
    return "A selected Texmap slot is empty.";
}

const char * exportTexmap() noexcept
{
    return "Export Texmap";
}

const char * chooseTexmapExportFolder() noexcept
{
    return "Choose Texmap Export Folder";
}

const char * exportCancelled() noexcept
{
    return "Export cancelled.";
}

const char * chooseTextureReplacementFolder() noexcept
{
    return "Choose Texture Replacement Folder";
}

const char * openTextureImage() noexcept
{
    return "Open Texture Image";
}

const char * metadataSourceNotConfigured() noexcept
{
    return "The metadata source is not configured";
}

const char * metadataSourceFileNotConfigured() noexcept
{
    return "The metadata source file is not configured";
}

const char * metadataDocumentCouldNotOpen() noexcept
{
    return "The metadata document could not be opened";
}

const char * tileNotVisibleWithCurrentFilter() noexcept
{
    return "Tile is not visible with the current filter.";
}

const char * textureNotVisibleWithCurrentFilter() noexcept
{
    return "Texture is not visible with the current filter.";
}

std::string formatRadarcolFallbackWarning(std::string_view detail)
{
    return "Configured Radarcol is unavailable; using Embedded Radarcol fallback. " + std::string(detail);
}

const char * noExternalRadarcolFallbackWarning() noexcept
{
    return "No external Radarcol is configured; using Embedded Radarcol fallback.";
}

const char * pickMapTileOrAsset() noexcept
{
    return "Pick a map tile or asset. Press Esc to cancel.";
}

std::string formatEditingRadarcols(std::size_t count)
{
    return "Editing " + std::to_string(count) + " Radarcols. Press Esc to cancel.";
}

std::string formatSelectedColors(std::size_t count)
{
    return "Selected colors: " + std::to_string(count);
}

std::string_view providerSourceName(ProviderSourceKind source) noexcept
{
    switch (source)
    {
        case ProviderSourceKind::none :
            return "Radarcol unavailable";
        case ProviderSourceKind::embeddedDefault :
            return "Embedded Radarcol";
        case ProviderSourceKind::externalConfigured :
            return "Configured Radarcol";
        case ProviderSourceKind::embeddedDefaultWithWarning :
            return "Embedded Radarcol (fallback)";
    }
    return "Radarcol unavailable";
}

std::string_view profileReadinessName(ProfileReadinessState state) noexcept
{
    switch (state)
    {
        case ProfileReadinessState::noProfile :
            return "No profile";
        case ProfileReadinessState::incomplete :
            return "Incomplete";
        case ProfileReadinessState::complete :
            return "Complete";
    }
    return unknown();
}

std::string_view dataFileLabel(DataFileRole role) noexcept
{
    switch (role)
    {
        case DataFileRole::radarcol :
            return "Radar colors";
        case DataFileRole::texidx :
            return "Texture index";
        case DataFileRole::texmaps :
            return "Texture data";
        case DataFileRole::tiledata :
            return "Tile data";
    }
    return "Data";
}

std::string_view dataFileName(DataFileRole role) noexcept
{
    switch (role)
    {
        case DataFileRole::radarcol :
            return "radarcol.mul";
        case DataFileRole::texidx :
            return "texidx.mul";
        case DataFileRole::texmaps :
            return "texmaps.mul";
        case DataFileRole::tiledata :
            return "tiledata.mul";
    }
    return "Unknown metadata file";
}

std::string_view dataFileStatusLabel(DataFileStatus status) noexcept
{
    switch (status)
    {
        case DataFileStatus::found :
            return "Found";
        case DataFileStatus::foundAndValidated :
            return "Valid";
        case DataFileStatus::missing :
            return "Missing";
        case DataFileStatus::ambiguous :
            return "Ambiguous";
        case DataFileStatus::invalid :
            return "Invalid";
    }
    return unknown();
}

std::string formatError(const uomul::Error & error)
{
    return error.path.empty() ? error.message : error.path.string() + ": " + error.message;
}

std::string formatProfileStatus(std::string_view name)
{
    return "Profile: " + std::string(name.empty() ? "None" : name);
}

std::string formatMapStatus(const std::filesystem::path & path)
{
    return "Map: " + (path.empty() ? std::string("None") : path.string());
}

std::string formatStateStatus(bool dirty)
{
    return "State: " + std::string(dirty ? "Staged changes" : "Clean");
}

std::string formatDataStatus(ProfileReadinessState state)
{
    return "Data: " + std::string(profileReadinessName(state));
}

std::string formatRadarcolStatus(ProviderSourceKind source)
{
    return "Radarcol: " + std::string(strings::providerSourceName(source));
}

std::string formatSaveChangesPrompt(std::string_view filename)
{
    return "Save changes to \"" + std::string(filename) + "\" before closing?";
}

const char * discardChangesWarning() noexcept
{
    return "If you close without saving, your changes will be discarded.";
}

std::string formatId(std::uint32_t id, IdFormat format)
{
    if (format == IdFormat::decimal)
    {
        return std::to_string(id);
    }
    return std::format("0x{:04X}", id);
}

} // namespace uo_mule::gui::strings
