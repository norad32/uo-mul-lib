#pragma once

#include "gui_model/persistence/preferences.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/providers/data_providers.h"
#include "gui_model/session/workbench_session.h"
#include <filesystem>
#include <string>
#include <string_view>

namespace uo_mule::gui::strings
{

[[nodiscard]] const char * open() noexcept;
[[nodiscard]] const char * cancel() noexcept;
[[nodiscard]] const char * save() noexcept;
[[nodiscard]] const char * discard() noexcept;
[[nodiscard]] const char * apply() noexcept;
[[nodiscard]] const char * applyAndUse() noexcept;
[[nodiscard]] const char * close() noexcept;
[[nodiscard]] const char * load() noexcept;
[[nodiscard]] const char * filter() noexcept;
[[nodiscard]] const char * ascending() noexcept;
[[nodiscard]] const char * loading() noexcept;
[[nodiscard]] const char * noItems() noexcept;
[[nodiscard]] const char * detailsCollapsed() noexcept;
[[nodiscard]] const char * noItemSelected() noexcept;
[[nodiscard]] const char * itemDetailsUnavailable() noexcept;
[[nodiscard]] const char * assetViewUnavailable() noexcept;
[[nodiscard]] const char * grid() noexcept;
[[nodiscard]] const char * table() noexcept;
[[nodiscard]] const char * item() noexcept;
[[nodiscard]] const char * details() noexcept;
[[nodiscard]] const char * hideUnused() noexcept;
[[nodiscard]] const char * dimensions() noexcept;
[[nodiscard]] const char * raw() noexcept;
[[nodiscard]] const char * hexadecimal() noexcept;

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
[[nodiscard]] std::string formatId(std::uint32_t id, IdFormat format);
[[nodiscard]] const char * idInputFormat(IdFormat format) noexcept;

} // namespace uo_mule::gui::strings
