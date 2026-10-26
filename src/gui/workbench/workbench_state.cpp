#include "gui/workbench/workbench_state.h"
#include "gui/commands/command_registry.h"
#include "gui/documents/map_document_window.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/world/map_canvas_state.h"
#include <utility>

namespace uo_mule::gui
{

WorkbenchState::WorkbenchState(Preferences initialPreferences) :
    preferences(std::move(initialPreferences)), persistedPreferences(preferences)
{
    menuBarState.fileDialog.dialogHost = &dialogHost;
    menuBarState.preferencesDialog.dialogHost = &dialogHost;
    fileBrowser.dialogHost = &dialogHost;
    fileSelector.dialogHost = &dialogHost;
    toolState.loadPreferences(preferences.tools);
    mapDocuments.canvasState().zoom = defaultMapCanvasZoom;
    registerDefaultCommands(commands);
    session.updateProfile(activeProfile);
}

} // namespace uo_mule::gui
