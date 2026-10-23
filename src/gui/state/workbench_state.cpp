#include "gui/state/workbench_state.h"
#include "gui/commands/command_registry.h"
#include "gui/views/world/map_canvas_view.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/world/map_canvas_state.h"
#include <utility>

namespace uo_mule::gui
{

WorkbenchState::WorkbenchState(Preferences initialPreferences) :
    preferences(std::move(initialPreferences)), persistedPreferences(preferences)
{
    workspace.canvasState().zoom = defaultMapCanvasZoom;
    registerDefaultCommands(commands);
    session.updateProfile(activeProfile);
}

} // namespace uo_mule::gui
