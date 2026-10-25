# Extension Guide

This guide describes where new features belong and which project files they
usually touch. The [architecture layers](../architecture/layers.md) define
component ownership. The [GUI architecture](../architecture/gui.md) defines
GUI contracts. The [project glossary](../glossary.md) defines standard terms.

Use the [development quality guide](quality.md) for formatting, lint, test
placement, and test naming. Check the [roadmap](../project/roadmap.md) before
adding deferred domain work.

## Before coding

- Identify the *File* or *File set* and its semantic roles.
- Decide whether the feature is a library reader, editor, *Document*, provider,
  *Document window*, *Panel*, *Dialog*, *Command*, *Workflow*, or *Operation*.
- Assign ownership to the narrowest package that can enforce the invariants.
- Define stable IDs, persistence impact, diagnostics, and failure behavior.
- Add headless contracts and tests before connecting GUI presentation.

## Required flow

Surfaces read state and emit typed requests. The frame applies the request after
rendering completes.

```text
Document window / Panel -> Command -> Workflow / Operation -> Changes -> redraw
```

Rendering code does not write *Files*, mutate profiles, close *Documents*, or
erase collections during traversal.

## New MUL library

A new MUL library uses one public interface header and separate reader and editor
implementations.

| Touch | Responsibility |
| --- | --- |
| `src/uomul/include/uomul/<family>/<family>.h` | Public data types, options, reader API, and editor API |
| `src/uomul/src/<family>/<family>_reader.cpp` | Validation, file opening, and read operations |
| `src/uomul/src/<family>/<family>_editor.cpp` | Staging, edits, commit, and discard |
| `src/uomul/src/<family>/<family>_common.h/.cpp` | Private shared implementation when needed |
| `src/uomul/CMakeLists.txt` | Register implementation sources |
| `tests/uomul/<family>/*_reader_tests.cpp` | Valid input, malformed input, and read failures |
| `tests/uomul/<family>/*_editor_tests.cpp` | Edits, staging, commit, discard, and output failures |
| `doc/formats/<family>.md` | File shape, validation, and supported behavior |

The interface header stays the only public header for the new library. The
reader validates input before exposing usable data. The editor stages changes
and publishes output through an explicit transaction. The library has no GUI,
profile, or application dependencies.

Use role-aware *File set* identity when the MUL library uses companion files.
Return structured errors for invalid paths, malformed input, unsupported data,
and output failures. Add library tests before adding a model or GUI integration.

## New Document

A new *Document* connects a *File* or *File set* to lifecycle and presentation
contracts.

| Touch | Responsibility |
| --- | --- |
| `src/gui_model/documents/<name>_document.h/.cpp` | Identity, validation, *Changes*, read model, and document operations |
| `src/gui_model/documents/document_contracts.h/.cpp` | Document kind, stable name, access, and source policy |
| `src/gui_model/documents/document_descriptors.h/.cpp` | Creation summary and document factory registration |
| `src/gui_model/source/` | Source identity, role validation, and file locks when needed |
| `src/gui_model/lifecycle/` | Open, activate, close, *Save*, and *Discard* transitions when needed |
| `src/gui_model/CMakeLists.txt` | Register model implementation sources |
| `tests/gui_model/<name>_document_tests.cpp` | Open, validation, edits, dirty state, save, discard, and failure behavior |
| `src/gui/documents/` or `src/gui/frame/` | Document window adapter and presentation dispatch |
| `src/gui/CMakeLists.txt` | Register GUI implementation sources |
| `tests/gui/` | Window composition and presentation behavior |

Keep *Changes* separate from *View state*. Keep file identity, lifecycle, and
dirty state in the model. A *Document window* presents one *Document* and emits
requests. It does not own paths, readers, or mutations.

Add persistence only when the state has a defined lifetime. Give document kinds,
window IDs, and file roles stable names before storing them.

## New Panel

A new *Panel* presents auxiliary or shared content. It does not own a
*Document* lifecycle.

| Touch | Responsibility |
| --- | --- |
| `src/gui_model/layout/panel_registry.h/.cpp` | `PanelId`, title, stable key, and default visibility |
| `src/gui/panels/<name>_panel.h/.cpp` | Panel rendering and request adapters |
| `src/gui/frame/workbench_frame.cpp` | Draw the visible panel and connect model state |
| `src/gui/commands/command_registry.h/.cpp` | Stable show or hide *Command* and availability |
| `src/gui/commands/menu_bar.cpp` | Add the panel command to the menu |
| `src/gui/presentation/strings/` | Panel title, labels, status, and disabled reasons |
| `src/gui/presentation/icons/` | Semantic symbols when the panel uses icons |
| `src/gui/CMakeLists.txt` | Register panel sources |
| `tests/gui/` | Empty, unavailable, invalid, and interaction presentation |

Keep paths, readers, provider selection, and mutations in the owning model.
Use adapters and contracts between the *Panel* and `gui_model`. Add a stable
panel key before saving visibility or layout state.

## New Dialog

A new *Dialog* has a headless request and result contract and a separate ImGui
renderer.

| Touch | Responsibility |
| --- | --- |
| `src/gui_model/dialogs/dialog_contracts.h/.cpp` | Dialog ID, request payload, result payload, and stable popup ID |
| `src/gui_model/dialogs/dialog_host.h/.cpp` | Request ownership, replacement, cancellation, and generation checks |
| `src/gui/dialogs/<name>_dialog.h/.cpp` | Rendering, input collection, and result emission |
| `src/gui/presentation/dialog/` | Shared sizing, centering, headings, and button presentation |
| `src/gui/workflows/` | Multi-step decisions that use the dialog result |
| `src/gui/presentation/strings/` | Dialog labels, guidance, diagnostics, and button text |
| `src/gui/presentation/icons/` | Semantic symbols for dialog actions |
| `src/gui_model/CMakeLists.txt` and `src/gui/CMakeLists.txt` | Register model and renderer sources |
| `tests/gui_model/` | Open, cancel, validation failure, replacement, and acceptance contracts |
| `tests/gui/` | Rendering and request-routing behavior |

Keep validation, selection state, and accepted results in the model contract.
The renderer presents state and emits a typed result. The frame or *Workflow*
applies that result after rendering. Follow the [dialog rules in the design
guide](design-guide.md#dialogs).

## Providers and metadata

Add provider contracts under `src/gui_model/providers/`. Bind profile defaults
outside the *Document window*. Providers expose effective read models, source
identity, status, and revision.

Metadata browsing belongs to metadata *Documents*. A metadata *Document window*
consumes its provider and routes edits to the document *Changes*. Add tests for
available, missing, invalid, unsupported, fallback, and rebinding states.

## Commands, Workflows and Operations

- Add a stable *Command* ID, label, availability, and disabled reason in the
  command registry.
- Put a multi-step *Workflow* under `src/gui/workflows/`. Define decisions,
  cancellation, success, and recoverable failure states.
- Put domain mutation logic in a headless *Operation* under
  `src/gui_model/operations/`. Define validation, scope, affected data, and
  *Changes* behavior.
- Add focused model or workflow tests before connecting menus, panels, or tools.

## Persistent state

Classify new state as frame, session, preference, layout, *Document*, *Changes*,
*View state*, or asset state. Define its owner, stable key, schema impact, and
reset behavior. Keep state with different lifetimes in separate persistence
records. Add round-trip and reset tests when the state is persisted.
