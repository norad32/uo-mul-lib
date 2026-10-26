#include "gui/dialogs/map_facet_dialog.h"
#include "gui/dialogs/map_dimension_fields.h"
#include "gui/dialogs/map_workflow_dialog.h"
#include "gui/presentation/dialog_presentation.h"
#include "gui/presentation/iconography.h"
#include "gui/presentation/strings.h"
#include "gui/presentation/theme.h"
#include <imgui.h>

namespace uo_mule::gui
{

namespace
{

constexpr float fieldWidth = 200.0F;
constexpr float fieldLabelWidth = 52.0F;

} // namespace

MapFacetDialogResult drawMapFacetDialog(NewFileDialogState & state)
{
    if (state.openCandidates.empty())
    {
        ImGui::TextColored(theme::imguiColor(theme::warning), "%s", strings::noExactFacetMatch());
    }

    const bool validDimensions = drawMapDimensionFields(
        state.openFacet, state.openWidth, state.openHeight, true, {.fieldWidth = fieldWidth, .labelWidth = fieldLabelWidth});
    if (!validDimensions)
    {
        ImGui::TextColored(theme::imguiColor(theme::warning), "%s", strings::invalidDimensions());
    }

    beginDialogFooter({strings::ok(), strings::cancel()}, false);
    if ((dialogButton(iconLabel(IconId::ok, strings::ok()).c_str(), validDimensions) || ImGui::IsKeyPressed(ImGuiKey_Enter)) &&
        validDimensions)
    {
        endDialogFooter();
        return MapFacetDialogResult::accept;
    }
    ImGui::SameLine();
    if (dialogButton(iconLabel(IconId::cancel, strings::cancel()).c_str()))
    {
        endDialogFooter();
        return MapFacetDialogResult::cancel;
    }
    endDialogFooter();
    return MapFacetDialogResult::none;
}

} // namespace uo_mule::gui
