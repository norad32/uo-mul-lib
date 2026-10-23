#include "gui/state/workbench_state.h"
#include "gui/commands/command_registry.h"
#include "gui/persistence/preferences.h"
#include "gui/ui/world/map_canvas_view.h"
#include <utility>

namespace uo_mule::gui
{

WorkbenchState::WorkbenchState(Preferences initialPreferences) : preferences(std::move(initialPreferences))
{
    workspace.canvasState().zoom = defaultMapCanvasZoom;
    registerDefaultCommands(commands);
    session.updateProfile(activeProfile);
}

} // namespace uo_mule::gui
