#pragma once

#include "gui/views/panel_registry.h"
#include <filesystem>
#include <string>

namespace uo_mule::gui
{

inline constexpr std::uint32_t currentLayoutSchemaVersion = 1;

[[nodiscard]] std::filesystem::path layoutPath();
void loadLayout(PanelRegistry & registry, std::string & dockingIni);
void saveLayout(const PanelRegistry & registry, const std::string & dockingIni);

} // namespace uo_mule::gui
