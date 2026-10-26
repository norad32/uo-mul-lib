#pragma once

#include "gui_model/persistence/preferences.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/providers/data_providers.h"
#include "gui_model/session/workbench_session.h"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace uo_mule::gui::strings
{

[[nodiscard]] const char * open() noexcept;
[[nodiscard]] const char * ok() noexcept;
[[nodiscard]] const char * cancel() noexcept;
[[nodiscard]] const char * save() noexcept;
[[nodiscard]] const char * discard() noexcept;
[[nodiscard]] const char * apply() noexcept;
[[nodiscard]] const char * applyAndUse() noexcept;
[[nodiscard]] const char * close() noexcept;
[[nodiscard]] const char * load() noexcept;
[[nodiscard]] const char * filter() noexcept;
[[nodiscard]] const char * loading() noexcept;
[[nodiscard]] const char * noItems() noexcept;
[[nodiscard]] const char * noItemSelected() noexcept;
[[nodiscard]] const char * itemDetailsUnavailable() noexcept;
[[nodiscard]] const char * assetViewUnavailable() noexcept;
[[nodiscard]] const char * grid() noexcept;
[[nodiscard]] const char * table() noexcept;
[[nodiscard]] const char * item() noexcept;
[[nodiscard]] const char * details() noexcept;
[[nodiscard]] const char * hideUnused() noexcept;
[[nodiscard]] const char * hideEmpty() noexcept;
[[nodiscard]] const char * dimensions() noexcept;
[[nodiscard]] const char * facet() noexcept;
[[nodiscard]] const char * width() noexcept;
[[nodiscard]] const char * height() noexcept;
[[nodiscard]] const char * customDimensions() noexcept;
[[nodiscard]] const char * invalidDimensions() noexcept;
[[nodiscard]] const char * unsupportedFacetSelection() noexcept;
[[nodiscard]] const char * noExactFacetMatch() noexcept;
[[nodiscard]] const char * raw() noexcept;
[[nodiscard]] const char * hexadecimal() noexcept;
[[nodiscard]] const char * hexadecimalName() noexcept;
[[nodiscard]] const char * profile() noexcept;
[[nodiscard]] const char * name() noexcept;
[[nodiscard]] const char * openProfile() noexcept;
[[nodiscard]] const char * none() noexcept;
[[nodiscard]] const char * newProfile() noexcept;
[[nodiscard]] const char * saveProfile() noexcept;
[[nodiscard]] const char * deleteProfile() noexcept;
[[nodiscard]] const char * status() noexcept;
[[nodiscard]] const char * missing() noexcept;
[[nodiscard]] const char * searchFolder() noexcept;
[[nodiscard]] const char * chooseFolder() noexcept;
[[nodiscard]] const char * searchMetadataFolder() noexcept;
[[nodiscard]] const char * showWelcomeScreen() noexcept;
[[nodiscard]] const char * decimal() noexcept;
[[nodiscard]] const char * mapViewTiles() noexcept;
[[nodiscard]] const char * texturesRadarcolFallback() noexcept;
[[nodiscard]] const char * preferencesDialogTitle() noexcept;
[[nodiscard]] const char * preferencesDialogSubheading() noexcept;
[[nodiscard]] const char * artworkBanner() noexcept;
[[nodiscard]] const char * recentFiles() noexcept;
[[nodiscard]] const char * noFilesOpened() noexcept;
[[nodiscard]] const char * openFileToAdd() noexcept;
[[nodiscard]] const char * configurePreferences() noexcept;
[[nodiscard]] const char * showOnStartup() noexcept;
[[nodiscard]] const char * showOnStartupTooltip() noexcept;
[[nodiscard]] const char * autoScroll() noexcept;
[[nodiscard]] const char * pick() noexcept;
[[nodiscard]] const char * fromActiveTile() noexcept;
[[nodiscard]] std::string formatProfilesStoredIn(const std::filesystem::path & path);
[[nodiscard]] const char * chooseMetadataFile() noexcept;
[[nodiscard]] const char * mapsWithoutProfile() noexcept;
[[nodiscard]] const char * openPreferences() noexcept;
[[nodiscard]] std::string formatProfileReadiness(std::string_view readiness);
[[nodiscard]] std::string formatRadarcolSource(std::string_view source);
[[nodiscard]] std::string formatCanvasNoTile(std::string_view canvas, double zoom);
[[nodiscard]] std::string formatCanvasTile(std::string_view canvas,
                                           std::uint32_t tileX,
                                           std::uint32_t tileY,
                                           std::uint32_t blockX,
                                           std::uint32_t blockY,
                                           double zoom);
[[nodiscard]] std::string formatDocumentNoView(std::string_view kind);
[[nodiscard]] const char * documentPlaceholder() noexcept;
[[nodiscard]] const char * helpMenu() noexcept;
[[nodiscard]] const char * undo() noexcept;
[[nodiscard]] const char * redo() noexcept;
[[nodiscard]] const char * cut() noexcept;
[[nodiscard]] const char * paste() noexcept;
[[nodiscard]] const char * configurePreferencesWindow() noexcept;
[[nodiscard]] const char * configurePreferencesLabel() noexcept;
[[nodiscard]] const char * formatLabel() noexcept;
[[nodiscard]] const char * pickSource() noexcept;
[[nodiscard]] const char * tiledataName() noexcept;
[[nodiscard]] const char * goToCoordinate() noexcept;
[[nodiscard]] const char * goToBlock() noexcept;
[[nodiscard]] const char * openEllipsis() noexcept;
[[nodiscard]] const char * createNewEllipsis() noexcept;
[[nodiscard]] const char * home() noexcept;
[[nodiscard]] const char * showHidden() noexcept;
[[nodiscard]] const char * openMulFile() noexcept;
[[nodiscard]] const char * create() noexcept;
[[nodiscard]] const char * createFolder() noexcept;
[[nodiscard]] const char * folderName() noexcept;
[[nodiscard]] const char * newFolder() noexcept;
[[nodiscard]] const char * invalidFolderName() noexcept;
[[nodiscard]] const char * folderAlreadyExists() noexcept;
[[nodiscard]] const char * entriesFormat() noexcept;
[[nodiscard]] const char * sizeFormat() noexcept;
[[nodiscard]] const char * modifiedFormat() noexcept;
[[nodiscard]] const char * facetFormat() noexcept;
[[nodiscard]] const char * dimensionsFormat() noexcept;
[[nodiscard]] const char * blocksFormat() noexcept;
[[nodiscard]] const char * missingFormat() noexcept;
[[nodiscard]] const char * trailingBytesFormat() noexcept;
[[nodiscard]] const char * folderFormat() noexcept;
[[nodiscard]] std::string formatEditingColors(std::size_t count);
[[nodiscard]] const char * mixedColorValues() noexcept;
[[nodiscard]] const char * multipleColorValues() noexcept;
[[nodiscard]] const char * filterSyntaxTooltip() noexcept;
[[nodiscard]] const char * dimensionsUnknown() noexcept;
[[nodiscard]] const char * customMap() noexcept;
[[nodiscard]] const char * modified() noexcept;
[[nodiscard]] const char * opened() noexcept;
[[nodiscard]] std::string formatUnavailable(std::string_view detail);
[[nodiscard]] std::string formatFontScale(bool iconFontAvailable, double scaleX, double scaleY);
[[nodiscard]] std::string formatTile(std::uint32_t x, std::uint32_t y);
[[nodiscard]] std::string formatBlock(std::uint32_t x, std::uint32_t y);
[[nodiscard]] std::string formatTileCoordinates(std::uint32_t minX, std::uint32_t maxX, std::uint32_t minY, std::uint32_t maxY);
[[nodiscard]] std::string formatSelectedTile(std::uint32_t x, std::uint32_t y, std::uint16_t id, std::int32_t z);
[[nodiscard]] std::string formatTileTooltip(std::uint32_t x, std::uint32_t y, std::uint16_t id, std::int32_t z);
[[nodiscard]] const char * id() noexcept;
[[nodiscard]] const char * tileId() noexcept;
[[nodiscard]] const char * elevation() noexcept;
[[nodiscard]] const char * mode() noexcept;
[[nodiscard]] const char * targetUsesActiveTile() noexcept;
[[nodiscard]] const char * source() noexcept;
[[nodiscard]] const char * target() noexcept;
[[nodiscard]] const char * scope() noexcept;
[[nodiscard]] const char * tools() noexcept;
[[nodiscard]] const char * toolOptions() noexcept;
[[nodiscard]] const char * activeTile() noexcept;
[[nodiscard]] const char * region() noexcept;
[[nodiscard]] const char * tiles() noexcept;
[[nodiscard]] const char * blocks() noexcept;
[[nodiscard]] const char * tiledataNamesUnavailable() noexcept;
[[nodiscard]] const char * metadata() noexcept;
[[nodiscard]] const char * noProfile() noexcept;
[[nodiscard]] const char * configured() noexcept;
[[nodiscard]] const char * incomplete() noexcept;
[[nodiscard]] const char * notConfigured() noexcept;
[[nodiscard]] const char * semanticIconVocabulary() noexcept;

[[nodiscard]] const char * fileMenu() noexcept;
[[nodiscard]] const char * viewMenu() noexcept;
[[nodiscard]] const char * editMenu() noexcept;
[[nodiscard]] const char * mapMenu() noexcept;
[[nodiscard]] const char * dataMenu() noexcept;
[[nodiscard]] const char * preferences() noexcept;
[[nodiscard]] const char * mapCanvas() noexcept;
[[nodiscard]] const char * tileInspector() noexcept;
[[nodiscard]] const char * blockInspector() noexcept;
[[nodiscard]] const char * radarcol() noexcept;
[[nodiscard]] const char * texmaps() noexcept;
[[nodiscard]] const char * radarcolFallbackTitle() noexcept;
[[nodiscard]] const char * radarcolFallbackMessage() noexcept;
[[nodiscard]] const char * radarcolFallbackQuestion() noexcept;
[[nodiscard]] const char * createDefaultRadarcol() noexcept;
[[nodiscard]] const char * defaultRadarcolCreated() noexcept;
[[nodiscard]] const char * continueWithEmbedded() noexcept;
[[nodiscard]] const char * closeWithoutSaving() noexcept;
[[nodiscard]] const char * windowsMenu() noexcept;
[[nodiscard]] const char * selectMenu() noexcept;
[[nodiscard]] const char * toolsMenu() noexcept;
[[nodiscard]] const char * worldMenu() noexcept;
[[nodiscard]] const char * noMapOpen() noexcept;
[[nodiscard]] const char * noTileUnderPointerOrSelected() noexcept;
[[nodiscard]] const char * hoverBlockToInspect() noexcept;
[[nodiscard]] const char * clear() noexcept;
[[nodiscard]] const char * sourceTexmapUnavailable() noexcept;
[[nodiscard]] const char * sourceTexmapUnsupportedDimensions() noexcept;
[[nodiscard]] const char * sourceTexmapInvalidPixels() noexcept;
[[nodiscard]] const char * sourceTexmapsDocumentClosed() noexcept;
[[nodiscard]] const char * sourceTexmapSlotEmpty() noexcept;
[[nodiscard]] const char * selectedTexmapSlotEmpty() noexcept;
[[nodiscard]] const char * exportTexmap() noexcept;
[[nodiscard]] const char * chooseTexmapExportFolder() noexcept;
[[nodiscard]] const char * exportCancelled() noexcept;
[[nodiscard]] const char * chooseTextureReplacementFolder() noexcept;
[[nodiscard]] const char * openTextureImage() noexcept;
[[nodiscard]] const char * metadataSourceNotConfigured() noexcept;
[[nodiscard]] const char * metadataSourceFileNotConfigured() noexcept;
[[nodiscard]] const char * metadataDocumentCouldNotOpen() noexcept;
[[nodiscard]] const char * tileNotVisibleWithCurrentFilter() noexcept;
[[nodiscard]] const char * textureNotVisibleWithCurrentFilter() noexcept;
[[nodiscard]] std::string formatRadarcolFallbackWarning(std::string_view detail);
[[nodiscard]] const char * noExternalRadarcolFallbackWarning() noexcept;
[[nodiscard]] const char * pickMapTileOrAsset() noexcept;
[[nodiscard]] std::string formatEditingRadarcols(std::size_t count);
[[nodiscard]] std::string formatSelectedColors(std::size_t count);

[[nodiscard]] std::string_view providerSourceName(ProviderSourceKind source) noexcept;
[[nodiscard]] std::string_view profileReadinessName(ProfileReadinessState state) noexcept;
[[nodiscard]] std::string_view dataFileLabel(DataFileRole role) noexcept;
[[nodiscard]] std::string_view dataFilePurpose(DataFileRole role) noexcept;
[[nodiscard]] std::string_view dataFileName(DataFileRole role) noexcept;
[[nodiscard]] std::string_view dataFileStatusLabel(DataFileStatus status) noexcept;

[[nodiscard]] std::string formatError(const uomul::Error & error);
[[nodiscard]] std::string formatProfileStatus(std::string_view name);
[[nodiscard]] std::string formatMapStatus(const std::filesystem::path & path);
[[nodiscard]] std::string formatStateStatus(bool dirty);
[[nodiscard]] std::string formatDataStatus(ProfileReadinessState state);
[[nodiscard]] std::string formatRadarcolStatus(ProviderSourceKind source);
[[nodiscard]] std::string formatSaveChangesPrompt(std::string_view filename);
[[nodiscard]] const char * discardChangesWarning() noexcept;
[[nodiscard]] std::string formatId(std::uint32_t id, IdFormat format);
[[nodiscard]] const char * idInputFormat(IdFormat format) noexcept;

} // namespace uo_mule::gui::strings
