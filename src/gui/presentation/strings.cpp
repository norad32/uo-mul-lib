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
namespace
{

[[nodiscard]] std::string_view unknown() noexcept
{
    return "Unknown";
}

} // namespace

const char * open() noexcept
{
    return "Open";
}
const char * cancel() noexcept
{
    return "Cancel";
}
const char * save() noexcept
{
    return "Save";
}
const char * discard() noexcept
{
    return "Discard";
}
const char * apply() noexcept
{
    return "Apply";
}
const char * applyAndUse() noexcept
{
    return "Apply and Use";
}
const char * close() noexcept
{
    return "Close";
}
const char * load() noexcept
{
    return "Load";
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
const char * item() noexcept
{
    return "Item";
}
const char * details() noexcept
{
    return "Details";
}
const char * hideUnused() noexcept
{
    return "Hide unused";
}
const char * hideEmpty() noexcept
{
    return "Hide empty";
}
const char * dimensions() noexcept
{
    return "Dimensions";
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
const char * newProfile() noexcept
{
    return "New";
}
const char * saveProfile() noexcept
{
    return "Save Profile";
}
const char * deleteProfile() noexcept
{
    return "Delete Profile";
}
const char * status() noexcept
{
    return "Status";
}
const char * missing() noexcept
{
    return "Missing";
}
const char * searchFolder() noexcept
{
    return "Search folder";
}
const char * chooseFolder() noexcept
{
    return "Choose folder...##MetadataSearch";
}
const char * searchMetadataFolder() noexcept
{
    return "Search Metadata Folder";
}
const char * searchMetadataHint() noexcept
{
    return "Search the selected folder for known metadata files.";
}
const char * validateMetadata() noexcept
{
    return "Validate metadata";
}
const char * metadataValidationErrors() noexcept
{
    return "Metadata validation found errors";
}
const char * metadataValidationPassed() noexcept
{
    return "Metadata validation passed";
}
const char * showWelcomeScreen() noexcept
{
    return "Show Welcome screen at startup";
}
const char * decimal() noexcept
{
    return "Decimal";
}
const char * mapViewTiles() noexcept
{
    return "Map view tiles";
}
const char * texturesRadarcolFallback() noexcept
{
    return "Textures (Radarcol fallback)";
}
const char * preferencesDialogTitle() noexcept
{
    return "Preferences###dialog.preferences";
}
const char * preferencesDialogSubheading() noexcept
{
    return "Configure your UO data profile and application startup behavior.";
}
const char * changesApplied() noexcept
{
    return "Changes applied for this dialog session";
}
const char * artworkBanner() noexcept
{
    return "Artwork banner";
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
const char * statusFormat() noexcept
{
    return "Status: %s";
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
const char * pickSource() noexcept
{
    return "Pick source##RadarcolPickSource";
}
const char * tiledataName() noexcept
{
    return "Tiledata name##fuzzy.name.enabled";
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
    return "Create New...";
}
const char * home() noexcept
{
    return "Home";
}
const char * showHidden() noexcept
{
    return "Show hidden";
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

std::string formatUnavailable(std::string_view detail)
{
    return "Unavailable: " + std::string(detail);
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

const char * metadata() noexcept
{
    return "Metadata";
}

const char * noProfile() noexcept
{
    return "No profile";
}

const char * configured() noexcept
{
    return "Configured";
}

const char * incomplete() noexcept
{
    return "Incomplete";
}

const char * notConfigured() noexcept
{
    return "Not configured";
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
const char * mapMenu() noexcept
{
    return "Map";
}
const char * dataMenu() noexcept
{
    return "Data";
}
const char * preferences() noexcept
{
    return "Preferences";
}
const char * mapCanvas() noexcept
{
    return "Map Canvas";
}
const char * tileInspector() noexcept
{
    return "Tile Inspector";
}
const char * blockInspector() noexcept
{
    return "Block Inspector";
}
const char * radarcol() noexcept
{
    return "Radarcol";
}
const char * texmaps() noexcept
{
    return "Texmap Pair";
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

const char * toolsMenu() noexcept
{
    return "Tools";
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
        case ProfileReadinessState::needsProfile :
            return "No profile";
        case ProfileReadinessState::profileInvalid :
        case ProfileReadinessState::profileIncomplete :
            return "Fallback active";
        case ProfileReadinessState::ready :
            return "Configured";
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

std::string_view dataFilePurpose(DataFileRole role) noexcept
{
    switch (role)
    {
        case DataFileRole::radarcol :
            return "Radar and minimap color data.";
        case DataFileRole::texidx :
            return "Texture index and offset data.";
        case DataFileRole::texmaps :
            return "Texture pixel data.";
        case DataFileRole::tiledata :
            return "Tile flags and tile metadata.";
    }
    return "UO metadata file.";
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
            return "Ready";
        case DataFileStatus::missing :
            return "Missing";
        case DataFileStatus::ambiguous :
            return "Ambiguous";
        case DataFileStatus::invalid :
            return "Invalid";
        case DataFileStatus::overridden :
            return "Override";
        case DataFileStatus::optional :
            return "Optional";
        case DataFileStatus::unsupported :
            return "Unsupported";
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

const char * idInputFormat(IdFormat format) noexcept
{
    return format == IdFormat::decimal ? "%d" : "0x%04X";
}

} // namespace uo_mule::gui::strings
