# Project Glossary

These are the standard terms used throughout the project documentation.

| Term | Meaning | Example |
| --- | --- | --- |
| File | One path on disk | `radarcol.mul` |
| File set | Related files on disk | `texidx.mul` and `texmaps.mul` |
| Document | An open editable file or file set | A Texmap Pair document |
| Changes | Unsaved file edits | Replacing texture 42 |
| View state | Selection and display state | Selected tile and zoom level |
| Command | A user request | Save or Discard |
| Operation | Internal work for a command | Validate and write map data |
| Workflow | A multi-step GUI transition that coordinates commands, dialogs, and operations | Open a file or replace textures |
| Save | Write changes to files | Save a changed map |
| Discard | Remove unsaved changes | Close without saving |
| Dirty | The document has changes | A dirty document shows unsaved edits |
| Workbench | The composed application surface that hosts document windows, panels, dialogs, commands, and status regions. | The map editing workbench |

## GUI vocabulary

These terms describe the visible workbench surfaces. A Document window and a
Panel may both be docked, but they have different owners and purposes.

| Term | Meaning | Example |
| --- | --- | --- |
| Document window | A session- and document-managed surface for exactly one open Document. It may be docked, but it is not an auxiliary Panel. | Map document window |
| Panel | An auxiliary dock surface managed by panel layout state and not tied to exactly one Document. | Tile Inspector or Logs |
| Dialog | A transient surface that requests user input or a decision and returns a result. | Open Map dialog |
| Canvas | An interactive content region hosted by a Document window or Panel. It renders visual or spatial data and receives direct interaction. | Map Canvas |
| Menu Bar | A persistent top-level command region containing application menus. | File, Edit, View, Windows |
| Status Bar | A fixed workbench feedback region showing status, diagnostics, selection, or context. | Dirty state and hovered tile |

Use these terms consistently in documentation and source code.
