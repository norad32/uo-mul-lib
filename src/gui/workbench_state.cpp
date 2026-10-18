#include "gui/workbench_state.h"
#include "gui/command_registry.h"
#include "gui/panels/map_canvas_panel.h"
#include "gui/preferences.h"
#include <utility>

namespace uo_mule::gui
{

WorkbenchState::WorkbenchState(Preferences initialPreferences) : preferences(std::move(initialPreferences))
{
    mapCanvasState.zoom = defaultMapCanvasZoom;
    registerDefaultCommands(commands);
}

} // namespace uo_mule::gui
