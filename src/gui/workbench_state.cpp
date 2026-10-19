#include "gui/workbench_state.h"
#include "gui/command_registry.h"
#include "gui/panels/map_canvas_panel.h"
#include "gui/preferences.h"
#include <utility>

namespace uo_mule::gui
{

WorkbenchState::WorkbenchState(Preferences initialPreferences) : preferences(std::move(initialPreferences)), views(panelRegistry)
{
    mapCanvasState.zoom = defaultMapCanvasZoom;
    registerDefaultCommands(commands);
    session.updateProfile(activeProfile);
}

} // namespace uo_mule::gui
