#include "gui/presentation/strings.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/providers/data_providers.h"
#include "gui_model/session/workbench_session.h"
#include "uomul/mul_common/mul_common.h"
#include <cstddef>
#include <cstdint>
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
const char * ascending() noexcept
{
    return "Ascending";
}
const char * loading() noexcept
{
    return "Loading...";
}
const char * noItems() noexcept
{
    return "No items to display";
}
const char * detailsCollapsed() noexcept
{
    return "Details collapsed";
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

const char * tileInspectionUnavailable() noexcept
{
    return "Tile inspection data unavailable";
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
