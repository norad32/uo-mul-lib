#pragma once

#include <imgui.h>
#include <initializer_list>

namespace uo_mule::gui
{

struct DialogWindowSpec
{
    ImVec2 initialSize;
    ImVec2 minimumSize;
};

[[nodiscard]] DialogWindowSpec standardDialogSpec() noexcept;
[[nodiscard]] DialogWindowSpec selectorDialogSpec() noexcept;
[[nodiscard]] DialogWindowSpec largeDialogSpec() noexcept;
[[nodiscard]] DialogWindowSpec browserDialogSpec() noexcept;
[[nodiscard]] DialogWindowSpec mapDialogSpec() noexcept;

[[nodiscard]] bool beginDialog(const char * popupId,
                               bool * visible,
                               DialogWindowSpec spec,
                               ImGuiWindowFlags extraFlags = ImGuiWindowFlags_None);
void endDialog();
void dialogHeading(const char * text);
void dialogSubheading(const char * text);
void dialogSeparatorText(const char * text);
void beginDialogFooter(std::initializer_list<const char *> labels);
void endDialogFooter();
[[nodiscard]] bool dialogButton(const char * label, bool enabled = true);

} // namespace uo_mule::gui
