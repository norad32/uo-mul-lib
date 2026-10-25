# GUI Design Guide

This guide defines presentation conventions. Ownership and lifecycle rules are
defined by the [GUI architecture](../architecture/gui.md). Use the [project
glossary](../glossary.md) for standard vocabulary.

## Theme

The GUI uses the [Catppuccin Mocha](https://github.com/catppuccin/catppuccin)
palette. Palette values and semantic roles are defined in
[`theme.h`](../../src/gui/presentation/theme/theme.h). Presentation code uses
semantic roles instead of introducing local colors.

| Semantic role | Palette value | Use in the GUI |
| --- | --- | --- |
| `canvasBackground` | Base `#1E1E2E` | OpenGL clear color and *Canvas* background |
| `panelBackground` | Mantle `#181825` | *Panel* and recent-item backgrounds |
| `popupBackground` | Mantle `#181825` | Popup backgrounds |
| `border` | Surface 1 `#45475A` | *Canvas*, asset, and inspector borders |
| `selection` | Lavender `#B4BEFE` | Selected assets, tiles, and map outlines |
| `selectionOverlay` | Selection at low alpha | Filled asset selection regions |
| `accent`, `heading` | Mauve `#CBA6F7` | Accents, headings, and docking preview |
| `primaryText` | Text `#CDD6F4` | Map labels, coordinates, and primary text |
| `subheading`, `greeting` | Lavender `#B4BEFE` | Secondary headings and welcome text |
| `mutedText` | Subtext 0 `#A6ADC8` | Hints, paths, dimensions, and secondary status |
| `hoveredSurface` | Surface 1 `#45475A` | Hovered rows and controls |
| `activeSurface` | Surface 2 `#585B70` | Active rows, buttons, and tools |
| `warning` | Peach `#FAB387` | Warnings and recoverable problems |
| `error` | Red `#F38BA8` | Errors and invalid state |
| `success` | Green `#A6E3A1` | Successful operations and valid state |
| `info` | Sapphire `#74C7EC` | Informational state |
| `unavailable` | Maroon `#EBA0AC` | Unavailable map or asset data |
| `checkerboardDark`, `checkerboardLight` | `#404040`, `#808080` | Texture preview transparency background |
| `traceLog`, `debugLog`, `infoLog` | Overlay 2, Blue, Teal | Trace, debug, and info log entries |
| `warningLog`, `errorLog`, `neutralLog` | Yellow, Red, Subtext 1 | Warning, error, and fallback log entries |

Use the semantic status colors consistently. Do not assign a warning color to
an error or use color as the only indication of state. Pair color with text,
icons, or structure.

## Strings

User-facing strings belong to the presentation string layer in
[`ui_strings.h`](../../src/gui/presentation/strings/ui_strings.h) and
[`ui_strings.cpp`](../../src/gui/presentation/strings/ui_strings.cpp).

- Use shared string functions for labels, menu names, status text, tooltips, and
  common empty or unavailable messages.
- Use formatter functions for errors, profile status, provider status, file
  labels, and numeric IDs.
- Keep domain names, file-format names, stable IDs, and serialized keys in their
  owning layers. Presentation formatting may change their display without
  changing their identity.
- Keep user-facing text when an icon is present. A missing icon
  font must leave the text usable.
- Keep labels concise and describe the action or state directly.

## Symbols

The GUI uses the [Material Design Icons](https://github.com/Templarian/MaterialDesign-Webfont)
font. The bundled font is
[`materialdesignicons-webfont.ttf`](../../assets/fonts/materialdesignicons-webfont.ttf).

Semantic icon definitions live in
[`iconography.h`](../../src/gui/presentation/icons/iconography.h) and their
mapping lives in
[`iconography.cpp`](../../src/gui/presentation/icons/iconography.cpp).

- Use `IconId` and semantic names instead of raw codepoints or font names.
- Use `iconLabel` for icon-plus-label controls. Text remains the fallback when
  the icon font is unavailable.
- Give icon-only controls a tooltip and a stable ImGui ID.

## Layout

The workbench uses one dock space with a central *Document window* region, left
and right *Panel* regions, a top-level *Menu Bar*, and a fixed *Status Bar*.

### Document windows

A *Document window* occupies the central dock region by default. Each window
represents exactly one *Document*. Its title combines the document kind and
display name. A *Dirty* document shows an asterisk.

Document window identity uses the stable document ID. Display titles may change
without changing the window identity.

### Panels

The *Panel* registry owns panel visibility, titles, and stable IDs. Default
panels are visible except the Icon Gallery. The default layout places inspectors
on the left and asset panels on the right. Users may dock or hide panels, and
the layout store persists window geometry, docking data, and panel visibility.

Panels present shared or auxiliary content. They do not own a *Document*
lifecycle. Panel keys remain stable when display titles or implementation files
change.

### Tool Palette

The Tool Palette is a *Panel* in the upper-left dock region. It contains:

- the available tool grid.
- options for the active tool.
- the active tile and its picker.
- Tool buttons use larger symbols for quick recognition. Text labels remain the
  fallback when the icon font is unavailable.

Tool availability comes from the tool registry and the active *Document*. The
palette sends typed requests through the command and operation boundaries. It
does not mutate document data during rendering.

## Dialogs

*Dialogs* are transient request and result surfaces. Standard dialogs:

- use the shared automatic-size helper.
- center over the main viewport.
- show a heading and muted guidance first.
- keep primary content compact.
- place Cancel and the primary *Command* in a right-aligned footer.
- use icon-plus-label controls when an icon is available.

Fixed-size dialogs are reserved for intentionally large browsers or editors.
The owning contract documents that exception.
