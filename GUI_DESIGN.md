# uo-mule GUI Design

Status: authoritative implementation plan; M25 complete

Last reviewed: 2026-10-21

This document is for contributors changing the desktop GUI, MUL document workflows, metadata providers, dialogs, asset views, or map editing tools. It records the current architecture, confirmed product decisions, transitional limitations, remaining milestones, and open design points. It replaces the previous post-M12 roadmap while retaining only the completed-baseline information needed to understand the next work.

## 1. Product Direction

`uo-mule` is a general-purpose Ultima Online MUL viewer/editor. Maps are the first editing workflow. Radarcol, Texmap Pair, and future MUL formats are independent asset/document workflows that must fit the same workbench without adding format-specific state to the frame loop.

The workbench has three related concepts:

- Documents are top-level tabs with canonical identity, source paths, access state, validation state, and dirty state.
- The central document surface renders the active document. The first implementation has one central pane; future split panes are reserved for Compare/Diff work.
- Profile-bound asset palettes are global dockable views. They expose the active profile's effective metadata source to maps, inspectors, and tools.

The central surface and palettes are different roles. A standalone Radarcol or Texmap Pair document is opened as a document tab and rendered centrally. A configured Radarcol or Texmap Pair is also available through a read-only profile-bound palette. The palette is not an independent source browser.

Primary map workflow:

```text
open or create a map
    -> navigate and inspect
    -> select and edit
    -> review staged changes
    -> save to an explicit output
```

Primary standalone asset workflow:

```text
open a supported MUL asset
    -> inspect through the shared asset view
    -> optionally edit when the document permits it
    -> save to an explicit output
    -> optionally promote it to a profile source
```

Primary profile workflow:

```text
configure profile metadata sources
    -> expose effective providers and palettes
    -> use them read-only from maps and tools
    -> rebind open state when the profile changes
```

## 2. Confirmed Product Decisions

### 2.1 Document kinds

| Document | Files | Current/target behavior |
| --- | --- | --- |
| Map | One map `.mul` | View and edit with staged writes |
| Radarcol | One `radarcol.mul` | Standalone view/edit; configured source read-only |
| Texmap Pair | `texidx.mul` + `texmaps.mul` | One logical view-only document initially; editing later |

`texidx.mul` and `texmaps.mul` are one logical document. They must not appear as two independently editable documents.

Future document kinds may include table-only or comparison documents. Unknown files are rejected with a diagnostic; no raw/hex viewer is planned for this phase.

### 2.2 Documents and central rendering

Documents can coexist in the tab collection:

```text
Map 0 | Map 1 | Radarcol | Texmap Pair
```

Opening an already-open canonical identity focuses the existing document rather than creating a duplicate.

The central workbench surface is a native ImGui dock node containing one stable document window per open document. ImGui owns the document tabs:

- Map documents provide map-canvas content.
- Radarcol documents provide the shared asset window with a Radarcol adapter.
- Texmap Pair documents provide the shared asset window with a Texmap adapter.
- Future document types provide their own central content adapter.

The first central-surface milestone renders one visible document pane at a time through native dock tabs. Split panes are intentionally deferred to Compare/Diff work. The future split model may show any open documents, not only maps, but this is an open design point until the Compare workflow is implemented.

Document activation must not mutate application state from inside an ImGui tab-rendering loop. Tab changes and closes are queued or applied at a frame boundary.

### 2.3 Profile-bound palettes

Radarcol and Texmaps are global profile-bound palettes. They remain available with no active map and adapt to the current profile state.

- Radarcol uses the effective provider. It displays a valid configured source, the embedded source when no external source is configured, or the embedded source with a visible fallback warning when configured data is invalid.
- Texmaps requires a valid configured pair. If unavailable, it remains an inline unavailable palette and offers `Configure texture pair`, which opens Preferences.
- A profile-bound palette cannot load a temporary independent source.
- `Configure source` and `Configure texture pair` are adapter-provided actions that route to Preferences.
- Map rendering, inspectors, palettes, and future map tools consume the same effective provider/model. A palette must not become a second source of truth.

Profile changes rebind all open profile-bound state and invalidate provider-dependent caches for every open map, including inactive maps. Navigation, staged edits, selection, and other non-provider state remain intact.

Configured-source identity is based on canonical source paths. If a profile changes, only documents whose canonical identity matches the new configured source become configured/read-only. An existing standalone document at an old path is not silently converted.

### 2.4 Access and editing

Maps have no global Edit Mode. Map documents are editable through compatible tools; edits remain staged and require explicit Save or Save As.

Configured metadata sources are always read-only, including when the source is missing or invalid.

Standalone Radarcol is editable after document-owned editing is complete. Standalone Texmap Pair is view-only initially. Texmap Pair editing follows the stable document and promotion workflows.

Normal standalone Save and profile source promotion are separate actions. Generic Open never changes the active profile.

### 2.5 Map workspace policy

The current code has one `MapWorkspace` behind collection-backed map document records. This is a transitional limitation and contributes to activation complexity.

The target architecture uses a separate workspace manager:

```text
Map document identity
    -> Map workspace manager
        -> one workspace per open map
            -> reader
            -> staged edits
            -> navigation and zoom
            -> selection and display mode
            -> active tool state
            -> provider-dependent render cache
```

Each map workspace is created when its document opens, remains alive while the tab is open, and is destroyed only after close/save-discard resolution. Switching map tabs is prompt-free and restores that map's state. A dirty map is resolved only when it is closed, replaced, or the application quits. Closing an inactive dirty tab may activate that map temporarily for the existing save/discard flow.

Profile changes invalidate provider-dependent caches for every workspace but preserve readers, staged edits, navigation, selection, display mode, and tool state.

The workspace manager is a separate milestone from the immediate Radarcol anchoring fix. M13 stabilizes the current composition first; the later workspace milestone removes single-workspace activation churn.

### 2.6 Radarcol fallback

Maps always have a usable Radarcol source:

```text
No configured Radarcol
    -> embedded default

Configured valid Radarcol
    -> external configured source

Configured missing or invalid Radarcol
    -> embedded default plus visible warning
```

The fallback does not block map viewing, editing, or saving. Status and notifications identify both the effective source and the broken configured path.

## 3. Current Baseline: M00-M12

M00-M12 are broadly implemented. The status is intentionally condensed; the detailed historical implementation notes are no longer the active plan.

- M00: vocabulary and document/source contracts exist in `src/gui/metadata/document_contracts.h`.
- M01: GUI sources are organized under `src/gui` subsystem directories.
- M02: active frame composition is in `src/gui/frontend/frame_composition.cpp`; `application.cpp` delegates to it. The central node hosts per-document windows through `src/gui/frontend/document_surface.*`.
- M03: generic document identity, canonical paths, summaries, and validation state exist.
- M04: `DocumentManager` owns a collection-backed lifecycle with canonical duplicate reuse and map adapters.
- M05: document tabs, activation, dirty/read-only indicators, and empty states exist.
- M06: configured Radarcol and Texmap Pair identities are derived from profile paths and canonical matching.
- M07: embedded Radarcol data and provider are checked in.
- M08: external, embedded, and fallback-with-warning provider selection exists.
- M09: Map Canvas consumes the effective provider and invalidates its low-zoom cache on provider revisions.
- M10: startup and fallback UX permits map use without a valid external Radarcol source.
- M11: generic MUL detection handles maps, Radarcol, and Texmap Pair candidates.
- M12: Radarcol is a reader-backed document/view with configured-source read-only classification.

M25 moves standalone Radarcol editor, output, dirty, access, and error lifecycle into `RadarcolDocument`. `RadarcolPanelState` retains only profile-palette reader/cache and asset-view state; central standalone Radarcol content reads and acts through the document.

### 3.1 Current limitations

- `WorkbenchState` remains a broad service locator for application, document, profile, dialog, panel, provider, and window state.
- `MapWorkspace` is still one active workspace behind multiple map records.
- Standalone Radarcol and the profile-bound Radarcol palette retain separate presentation state while sharing the document/provider access rules.
- Texmaps is still a profile-backed panel and a placeholder document path rather than a real Texmap Pair document.
- The active document surface is now stable for Map and Radarcol; other document kinds still use placeholder content until their document milestones.
- Map-side Radarcol is explicitly read-only; map editing is available through compatible map tools without a global mode switch.
- Docking settings are stored in a custom file, but ImGui settings import/export is not connected.
- Status is map-specific.
- Keyboard shortcuts and file result routing remain partly hard-coded.
- Expensive reads and statistics can occur during frame rendering.
- There is no operation history, undo/redo, or background operation manager.

### 3.2 Current source map

| Concern | Location |
| --- | --- |
| Platform/bootstrap and presentation | `src/gui/application/application.*` |
| Active frame composition | `src/gui/frontend/frame_composition.*` |
| Broad application state | `src/gui/application/workbench_state.*` |
| Session, readiness, document sync | `src/gui/profile/workbench_session.*` |
| Map behavior and staged writes | `src/gui/documents/map_workspace.*` |
| Generic document records | `src/gui/documents/map_document.*`, `document_identity.*` |
| Radarcol document adapter | `src/gui/documents/radarcol_document.*` |
| Profile/discovery/validation | `src/gui/profile/*` |
| Effective data providers | `src/gui/providers/*` |
| Shared asset shell, later | `src/gui/assets/*` |
| Current Radarcol/Texmaps views | `src/gui/views/radarcol_panel.*`, `texmaps_panel.*` |
| Shared dialogs | `src/gui/dialogs/*` |
| Commands and menus | `src/gui/commands/*` |
| Layout and preferences | `src/gui/persistence/*` |
| GUI tests | `tests/gui/*` |

## 4. M13 Implementation: Central Document Stabilization

### 4.1 Radarcol anchoring and flicker

The current standalone Radarcol document is rendered by `drawEmptyDocumentState()` as a separately positioned viewport window. It is not hosted in the central dockspace. At the same time, document tab activation can close/open the map workspace during tab rendering, and `drawDockSpace()` rebuilds the entire dock tree when its position differs from a cursor-derived expected position.

The combined behavior explains the reported sequence:

```text
open map
    -> open/activate Radarcol
    -> map workspace closes
    -> map-oriented dock tree remains/rebuilds
    -> Radarcol is positioned outside the central dock surface
    -> activation and geometry changes produce visible movement/flicker
```

M13 implements the stabilization fix:

1. Use the central document dock node as the document host.
2. Render map, Radarcol, and future document content through one stable window per document.
3. Stop using viewport-positioned standalone document windows for active documents.
4. Apply document close operations after document-window rendering at a frame boundary.
5. Rebuild the dock tree only on first creation, explicit Reset Workspace, or a versioned layout migration. A node-position difference alone must not rebuild user layout.
6. Enforce configured Radarcol read-only in both palette and central document contexts.
7. Keep the current one-workspace limitation explicit until the workspace-manager milestone.
8. Add a regression test for the persisted central surface identity and retain the existing canonical configured-Radarcol read-only coverage. Manual acceptance remains required for resize, log-panel changes, and layout reset.

Implemented changes:

- Added stable per-document window identities through `documentWindowName()` in `src/gui/frontend/document_surface.*`.
- Routed map and standalone Radarcol rendering through native document windows docked into the central node. ImGui owns their tab presentation.
- Made the map canvas document content rather than a toggleable panel, and removed its panel command/registry entry.
- Deferred document close mutations until after document-window rendering through pending actions in `WorkbenchState`.
- Removed dock-node position changes as a dock-tree rebuild trigger. The dock tree is rebuilt only for first creation or explicit layout reset; schema migration remains a future persistence concern.
- Passed `readOnly = true` to the profile-bound map-side Radarcol palette.
- Removed the ineffective Map Canvas visibility command from the View menu while retaining its stable dock identity.

The native GLFW decoration transition between welcome and workbench modes remains separately instrumented work. It may cause a compositor flash, but it is not the primary architectural fix for map-to-Radarcol anchoring.

M13 deliberately does not implement the per-map workspace manager. The current single `MapWorkspace` remains a documented limitation until M23.

M13 verification:

```text
cmake --build --preset debug-gui
ctest --preset debug-gui
clang-tidy src/gui/frontend/frame_composition.cpp src/gui/frontend/document_surface.h src/gui/views/map_canvas_panel.cpp src/gui/views/radarcol_panel.cpp src/gui/application/workbench_state.cpp src/gui/commands/menu_bar.cpp -p build/debug-gui
```

The configured `debug-gui` test run passed 333 tests. Manual runtime acceptance is still required for map -> Radarcol -> map switching, window resizing, log-panel changes, welcome/workbench transitions, and explicit layout reset.

## 5. Shared Asset Window Framework

Radarcol and Texmaps share presentation mechanics but not ownership or asset semantics. Future MULs must be able to use the same framework without adding format-specific branches to frame composition.

### 5.1 Shell boundary

The shared shell owns:

- Stable window identity and visible title placement.
- Header, source/status region, toolbar slots, and action slots.
- Scroll container and virtualized grid layout.
- Generic selection, range, and multi-selection behavior.
- Keyboard navigation.
- Hover and tooltip placement.
- Details pane below the grid, with collapsible session state.
- Empty, loading, unavailable, invalid, and table-not-available states.
- Capability-driven filtering and sorting controls.
- Generic drag payload metadata.
- Stable item identity handling and selection preservation.

The shell does not own:

- Readers or source paths.
- Profile mutation.
- Asset interpretation.
- Query execution.
- Format-specific caches.
- Editing transactions or output writes.
- Save, Discard, Save As, or promotion policy.

Adapters provide item and action behavior through descriptors/callbacks. The shell renders adapter-provided actions without knowing whether an action opens Preferences, opens a source, saves, discards, or promotes.

### 5.2 Asset adapter contract

The initial contract must support visual and record-oriented assets:

```text
AssetAdapter
├── stable kind and view ID
├── source/context identity
├── display title and source summary
├── access and validation state
├── known item count
├── fixed grid/table metrics
├── query capabilities
├── query/filter/sort state
├── visible-range item requests
├── loading and unavailable item states
├── stable item IDs and labels
├── optional visual preview
├── item details
├── adapter-provided actions
├── optional extension slots
└── optional drag payloads
```

The first implementation assumes a known finite item count and fixed row/cell metrics. Unknown or unbounded streams, variable-height rows, and variable-size grid cells are open points for later.

Adapters own filtering and sorting. The shell emits query changes; the adapter returns the new known count and visible item descriptions. Query changes apply immediately. Selection is preserved by stable item ID when the item remains in the result.

The generic selection API supports ranges and multiple selected IDs from the beginning. Radarcol and Texmaps may initially restrict the visible interaction to one selected item. A generic drag payload is defined but no concrete drop target is required in the first implementation.

### 5.3 Grid-first loading

The first renderer is a virtualized grid:

- The scrollbar represents the known total range.
- Page numbers and page buttons are removed from the user interface.
- Only visible cells are requested/rendered.
- Reads are synchronous initially.
- Unavailable visible cells render loading placeholders until the adapter supplies data.
- A normal text/record cell is available when an item has no image preview.
- Grid metrics are fixed per adapter/view.
- Default presentation is grid for Radarcol and Texmaps.

The first framework did not expose a table toggle. M20 adds the capability-driven toggle and shared table renderer while preserving table capability for table-only future MULs.

### 5.4 Table renderer

After Radarcol and Texmaps have migrated to the grid shell, M20 adds a shared table renderer using the same adapter, query, selection, details, and action contracts. The renderer supports table-only record adapters without requiring a second framework; optional preview columns remain a later extension point.

When table support exists, grid/table mode is available in palettes and central asset documents. The selected mode is persisted per canonical source/document context.

### 5.5 Asset state persistence

Asset view state is required to persist across application restarts once workspace persistence is complete. M17 defines the state model, reset behavior, and serialization boundary; M35 connects it to durable layout/workspace storage. The key includes asset kind, canonical source identity, and palette/document context. State includes:

- Query/filter.
- Sort key and order.
- Selected item or items.
- Scroll position.
- Details-pane collapse state.
- Grid/table mode once table mode exists.

Palette selection is one global selection per asset kind: one Radarcol selection and one Texmap selection. A palette selection includes source identity and is available to compatible map/tools. Selection is restored for the source when possible and cleared when profile rebinding changes the effective source.

Standalone central document selection is local to the document/source. It persists for navigation but does not replace the palette selection exposed to map operations.

### 5.6 Profile-bound asset behavior

Profile-bound Radarcol:

- Uses the effective provider/model.
- Shows embedded fallback when no external source is configured.
- Shows fallback warning when configured data is invalid.
- Is read-only.
- Offers `Configure source` through Preferences.

Profile-bound Texmaps:

- Uses the effective configured pair model.
- Shows an inline unavailable state when the pair is missing or invalid.
- Is read-only.
- Offers `Configure texture pair` through Preferences.

Neither palette may load an unrelated temporary source.

## 6. Dialog and String Architecture

### 6.1 Centralized strings

Add an English-only shared GUI vocabulary layer, likely under:

```text
src/gui/strings/ui_strings.h
src/gui/strings/ui_strings.cpp
```

Centralize reusable:

- Common button labels.
- Dialog titles.
- Menu labels.
- Document-kind names.
- Provider source names.
- Access and validation labels.
- Profile readiness labels.
- Common status messages.
- Diagnostics and error prefixes.
- Dynamic path, count, dimension, and identifier formatters.

Stable ImGui IDs remain separate from visible labels. Changing `Open` to another visible label must not change IDs such as `###dialog.preferences`, `###view.radarcol`, or persisted layout keys.

Localization is not part of this phase. The layer is for vocabulary consistency, not translation resources.

All new user-facing GUI strings must be added to `src/gui/strings/ui_strings.h` and `src/gui/strings/ui_strings.cpp` before they are used. Do not introduce new labels, titles, button captions, tooltips, status text, or diagnostics as local literals in views, dialogs, commands, or asset adapters. Stable ImGui IDs and file-format serialization names remain separate from this display-string rule. M16 asset contracts contain identifiers, adapter data, and action IDs, not user-facing display text; adapter labels must use the central string location when they are introduced.

### 6.2 Dialog contracts

Dialogs currently own too much workflow state and are coordinated through broad `WorkbenchState` mutation. Introduce typed dialog requests, results, actions, validation, and modal lifecycle before adding more document workflows.

The shared dialog infrastructure must provide:

- Stable popup identity separate from title text.
- Consistent title, spacing, width, and minimum-size rules.
- Standard default/cancel keyboard behavior.
- Standard footer action ordering.
- Shared validation and error presentation.
- Common path-picker rows.
- Common dirty, overwrite, and destructive-action confirmation.
- Explicit pending versus applied state.
- Typed result delivery to commands/session/document workflows.

The visual redesign preserves the current theme while improving hierarchy, spacing, action clarity, and validation visibility.

Existing workflows to migrate:

1. File browser and file-type selector.
2. Map facet selection.
3. Map open/create/save-as.
4. Dirty-change, overwrite, and error flows.
5. Preferences and profile editing.
6. Radarcol Save, Save As, Discard, and promotion.
7. Texmap Pair companion and promotion.

Unavailable profile-bound palettes initially reuse Preferences rather than introducing a special source dialog.

## 7. Remaining Milestones

Each milestone is a focused change set. It must build, add targeted headless coverage, and document manual GUI acceptance where visual or interaction behavior changes.

### M13: Stabilize central document composition

**Status:** Complete. The central document window identity is shared by map and standalone Radarcol content, tab mutations are deferred to the frame boundary, dock rebuilding no longer follows node-position changes, the map-side Radarcol palette is explicitly read-only, and confirmed dead/duplicate GUI code has been removed.

Fix central document anchoring, deferred tab mutations, dock rebuild behavior, and configured Radarcol read-only enforcement. This milestone keeps the current single `MapWorkspace` limitation.

Exit criteria:

- Active documents render inside one stable central docked surface.
- Map -> Radarcol -> map switching does not unanchor or flicker the document view.
- Docking is not rebuilt merely because the node position changes.
- Configured Radarcol has no editing controls in any context.
- Regression and manual acceptance coverage exists.

### M14: Remove confirmed dead and duplicate code

**Status:** Complete. The inactive legacy compositor, forwarding `ViewRegistry`, unused internal document/view APIs, unused Texmaps preview API, duplicate map-availability predicate, stale suppressions, and duplicate packed-color conversion/Radarcol content rendering were removed or consolidated. `DataProviders::tileData()` remains because it is part of the active provider contract and is covered by GUI tests.

Remove the inactive legacy compositor, unused `ViewRegistry`, confirmed-unused APIs, duplicate conversion helpers, duplicate previews, duplicate Radarcol render paths, duplicate command predicates, and stale no-op suppressions. Preserve only APIs with confirmed consumers or an explicit documented near-term owner.

M14 implementation notes:

- Deleted the `UO_MULE_LEGACY_FRAME_COMPOSITION` block from `src/gui/application/application.cpp`.
- Removed `ViewRegistry` and its target entry; `PanelRegistry` remains the current registry until the shared view framework milestone.
- Removed unused `defaultDockWindows()`, `DocumentManager::findMutable()`, `DocumentManager::hasActiveDocument()`, `WorkbenchSession::closeDocument()`, and `texmapsTexturePreview()` APIs.
- Added `src/gui/context/color_conversion.*` and routed Radarcol and 5-bit texture conversion through the shared helpers.
- Consolidated the standalone and docked Radarcol content body while preserving their separate window ownership and access modes.
- Reused `requiresMapDocument()` for the remaining equivalent map command predicate.
- Removed stale no-op suppressions in `new_map_dialog.cpp`.

M14 verification:

```text
cmake --build --preset debug-gui
ctest --preset debug-gui
clang-tidy src/gui/application/application.cpp src/gui/application/workbench_state.cpp src/gui/commands/command_registry.cpp src/gui/dialogs/new_map_dialog.cpp src/gui/documents/map_document.cpp src/gui/frontend/frame_composition.cpp src/gui/profile/workbench_session.cpp src/gui/providers/data_providers.cpp src/gui/providers/embedded_radarcol.cpp src/gui/views/radarcol_panel.cpp src/gui/views/texmaps_panel.cpp src/gui/context/color_conversion.cpp -p build/debug-gui
```

The configured `debug-gui` build and test run passed. The test suite contains 333 tests. No stable persisted ImGui window IDs were changed.

### M15: Centralize reusable GUI strings

**Status:** Complete. Added `src/gui/strings/ui_strings.*` for shared English display vocabulary and dynamic status/error formatters, migrated duplicated readiness/provider/data-file display helpers, and preserved serialization names and stable ImGui IDs.

Create the English-only string/vocabulary layer and migrate common labels, source names, status labels, diagnostics, dialog actions, and dynamic formatters. Preserve stable IDs.

M15 implementation notes:

- Added common action and menu/view labels to `src/gui/strings/ui_strings.*`.
- Centralized provider source names, profile readiness labels, data-file display labels, data-file purposes, and user-facing data-file status labels.
- Centralized profile, map, staged-state, data-readiness, Radarcol-source, and error formatters.
- Migrated frame composition, welcome screen, Preferences, provider status, and shared error formatting to the string layer.
- Kept persistence/serialization vocabulary such as `radarcol`, `found_and_validated`, and `output_collision_policy` in their domain APIs.
- Kept stable ImGui IDs such as `###view.map-canvas` and `###dialog.preferences` unchanged.
- Added focused string vocabulary and formatter tests in `tests/gui/ui_strings_tests.cpp`.

M15 verification:

```text
cmake --build --preset debug-gui
ctest --preset debug-gui
clang-tidy src/gui/strings/ui_strings.cpp src/gui/providers/data_providers.cpp src/gui/views/common.cpp src/gui/frontend/frame_composition.cpp src/gui/frontend/welcome_screen.cpp src/gui/dialogs/preferences_dialog.cpp -p build/debug-gui
```

The configured `debug-gui` build passed, all 337 tests passed, and the explicit clang-tidy run completed without findings.

### M16: Define shared asset-window contracts

**Status:** Complete. Added format-neutral asset adapter contracts under `src/gui/assets/` for capabilities, source context, queries, visible ranges, item states, selection, details, actions, extension slots, drag payloads, and persisted view state. No rendering or reader ownership was added.

Add the shell and adapter contracts for arbitrary visual/record assets, known counts, virtualized visible ranges, selection, queries, details, actions, optional extensions, and future table-only adapters.

M16 implementation notes:

- Added `src/gui/assets/asset_window_contracts.*` as the headless contract boundary for the future shared asset shell.
- Added presentation capabilities for grid and future table views.
- Added query capabilities for filtering and sorting.
- Added known item counts, fixed cell metrics, source/context identity, and visible-range requests.
- Added loading, empty, invalid, and unavailable item states.
- Added generic single/multiple selection with stable item IDs and an anchor item.
- Added item details, adapter-controlled actions, optional extension slots, and future drag payloads.
- Added `AssetWindowState` for presentation, query, selection, scroll, and details-pane state.
- Kept item loading, query execution, reader ownership, source mutation, rendering, and document editing outside the contract implementation.
- Added focused capability and selection tests in `tests/gui/asset_window_contracts_tests.cpp`.

M16 verification:

```text
cmake --build --preset debug-gui
ctest --preset debug-gui
clang-tidy src/gui/assets/asset_window_contracts.cpp src/gui/strings/ui_strings.cpp src/gui/providers/data_providers.cpp src/gui/views/common.cpp src/gui/frontend/frame_composition.cpp src/gui/frontend/welcome_screen.cpp src/gui/dialogs/preferences_dialog.cpp -p build/debug-gui
```

The configured `debug-gui` build passed, all 334 tests passed, and the explicit clang-tidy run completed without findings.

### M17: Implement the virtualized grid shell

**Status:** Complete. Added the reusable ImGui asset shell with virtualized known-range grid layout, synchronous adapter range requests, loading placeholders, selection, details, actions, capability-driven query controls, and testable grid layout helpers. Radarcol and Texmaps migration remains M18/M19.

Implement the shared grid-first shell, bottom details pane, adapter actions, synchronous visible-range loading, loading placeholders, keyboard navigation, generic selection API, capability-driven filter/sort controls, the asset-state serialization boundary, and global `Reset Workspace` integration. Do not add page controls or a disabled table toggle. Durable restart persistence is completed by M35.

M17 implementation notes:

- Added `src/gui/assets/asset_window.*` as the reusable ImGui shell built on the M16 adapter contracts.
- Added responsive fixed-metric grid layout calculation with known item counts and explicit scroll-position-based visible-row virtualization. The shell no longer depends on ImGui table/clipper row measurement.
- Added synchronous visible-range requests through `AssetRangeLoader`; missing returned items render loading placeholders.
- Added generic selection updates, adapter actions, capability-driven filtering/sort direction controls, and bottom item details to preserve grid width.
- Grid cells now render only the Radarcol swatch or Texmap thumbnail; raw item IDs are exposed through the shared hover tooltip without `Color`/`Texture` prefixes.
- The grid computes the first and last visible rows from scroll position and adapter cell height, preventing the grid from stopping after only one or two visible rows.
- The grid child consumes the remaining window height after the fixed header and bottom details pane, avoiding an extra shell scrollbar for normal asset views.
- Added inline unavailable presentation for adapters without a supported grid view.
- Routed all new shell labels through `src/gui/strings/ui_strings.*`.
- Kept durable restart persistence and `Reset Workspace` storage integration for M35, as planned.
- Did not migrate Radarcol or Texmaps panels in M17; those remain M18/M19 work.
- Added focused layout tests in `tests/gui/asset_window_tests.cpp`.

M17 verification:

```text
cmake --build --preset debug-gui
ctest --preset debug-gui
clang-tidy src/gui/assets/asset_window.cpp src/gui/assets/asset_window_contracts.cpp src/gui/strings/ui_strings.cpp src/gui/providers/data_providers.cpp src/gui/views/common.cpp src/gui/frontend/frame_composition.cpp src/gui/frontend/welcome_screen.cpp src/gui/dialogs/preferences_dialog.cpp -p build/debug-gui
```

The configured `debug-gui` build passed, all 340 tests passed, and the explicit clang-tidy run completed without findings.

### M18: Migrate Radarcol to the shared shell

**Status:** Complete. The profile-bound Radarcol palette and standalone Radarcol document use the shared asset shell with adapter-provided range loading, swatch rendering, details, source/edit extension controls, and stable selection. M25 subsequently moved standalone editor/output ownership into `RadarcolDocument`.

Use the same shell for the global profile-bound palette and standalone Radarcol central documents. Remove duplicated source/page/grid rendering. Keep palette selection available to map/tools while standalone selection remains local. Preserve Radarcol-specific color editing through document/adapter intents.

M18 implementation notes:

- Replaced Radarcol's dedicated page/table renderer with an asset adapter and virtualized grid.
- Preserved configured-source read-only behavior and standalone edit controls through adapter extension slots.
- Preserved the existing map color cache/provider-facing API; standalone editor ownership was intentionally completed by M25.
- Added Radarcol item range loading, raw-color details, and swatch cells.
- Kept `###view.radarcol` and `###document.radarcol` context identities stable through adapter view IDs.

### M19: Migrate Texmaps to the shared shell

**Status:** Complete. The profile-bound Texmaps palette now uses the shared asset shell with continuous known-range scrolling, adapter-owned texture preview cells, selection/details, and inline source diagnostics. Page buttons and panel-owned page rendering were removed; pair document creation remains M28.

Replace page-based Texmaps UI with continuous virtualized scrolling, lazy visible-range loading, shared selection/details behavior, adapter-owned texture previews, and inline configured-source diagnostics.

M19 implementation notes:

- Replaced Texmaps page navigation and page buttons with the shared virtualized grid.
- Added adapter range loading over the reader's known texture count.
- Kept texture pixel conversion and thumbnail drawing in the Texmaps adapter.
- Preserved profile synchronization and file-browser result routing.
- Kept incomplete/missing configured pair state inline through the shared shell's empty/unavailable behavior.
- Texmap Pair standalone document rendering remains M28; this milestone migrates the existing palette.

M18/M19 verification:

```text
cmake --build --preset debug-gui
ctest --preset debug-gui
clang-tidy src/gui/assets/asset_window.cpp src/gui/assets/asset_window_contracts.cpp src/gui/strings/ui_strings.cpp src/gui/views/radarcol_panel.cpp src/gui/views/texmaps_panel.cpp src/gui/providers/data_providers.cpp src/gui/views/common.cpp src/gui/frontend/frame_composition.cpp src/gui/frontend/welcome_screen.cpp src/gui/dialogs/preferences_dialog.cpp -p build/debug-gui
```

The configured `debug-gui` build passed, all 340 tests passed, and the explicit clang-tidy run completed without findings.

### M20: Add the shared table renderer

**Status:** Complete. The shared asset shell now supports capability-driven table mode alongside grid mode, including table-only record adapters. Grid/table presentation is restored per canonical source/document context for the current session; durable restart persistence remains M35.

Implement table mode for the existing adapter contract, including table-only record adapters. Add the grid/table toggle and per-source/document presentation persistence.

M20 implementation notes:

- Added the shared virtualized ImGui table renderer to `src/gui/assets/asset_window.cpp`.
- Table rows use the existing stable item IDs, range loader, selection behavior, and `AssetDetails` loader. The details column renders the same title and fields shown in the bottom selection-details pane.
- Added a capability-driven Grid/Table toggle. Grid-only adapters keep the existing grid path; table-only adapters default to the table path and do not require fixed cell metrics.
- Added in-memory presentation restoration keyed by asset kind, canonical source paths, and adapter context ID. This preserves independent palette/document choices during the session without introducing the durable M35 persistence boundary early.
- Enabled both grid and table presentations for the Radarcol and Texmaps adapters.
- Added contract coverage for table range requests and per-context presentation state.

M20 verification:

```text
cmake --build --preset debug-gui
ctest --preset debug-gui
clang-tidy src/gui/assets/asset_window.cpp src/gui/assets/asset_window_contracts.cpp src/gui/strings/ui_strings.cpp src/gui/views/radarcol_panel.cpp src/gui/views/texmaps_panel.cpp -p build/debug-gui
```

The configured `debug-gui` build passed, all 343 tests passed, and the explicit clang-tidy run completed without findings.

### M21: Add dialog contracts and modal host

**Status:** Complete. Added typed dialog requests/results and a shared single-workflow modal host. Existing dialogs retain their current visuals and workflow state; M22 owns the visual migration.

Introduce typed dialog requests/results and shared modal lifecycle without requiring a complete visual migration in the same change.

M21 implementation notes:

- Added `src/gui/dialogs/dialog_contracts.*` with stable `DialogId` values, popup IDs, typed request payloads, typed result payloads, and common dialog actions.
- Added `src/gui/dialogs/dialog_host.*` as the single active-modal lifecycle owner. Requests are rejected while another workflow is active unless explicit replacement is requested; completion and cancellation publish typed results.
- Added host arbitration to the menu/frame dialog path for map workflows, Preferences, the file browser, and the file selector. Existing file I/O and dialog-specific state remain in their current owners.
- Kept stable ImGui popup identities separate from visible titles. Existing visual dialog implementations remain unchanged pending M22.
- Added headless coverage for stable popup IDs, request replacement, typed completion payloads, and cancellation.

M21 verification:

```text
cmake --build --preset debug-gui
ctest --preset debug-gui
clang-tidy src/gui/dialogs/dialog_contracts.cpp src/gui/dialogs/dialog_host.cpp src/gui/commands/menu_bar.cpp src/gui/application/workbench_state.cpp -p build/debug-gui
```

The configured `debug-gui` build passed, all 347 tests passed, and the explicit clang-tidy run completed without findings.

### M22: Redesign and migrate dialogs

**Status:** Complete. Standardized the existing dialog presentation while preserving the current theme, stable popup identities, validation behavior, and output safety. Migrated the file selector/browser, map workflow/facet, Preferences, and path-selection flows through shared sizing, constraints, footer, and button helpers.

Preserve the current theme while standardizing all existing dialog hierarchy, actions, validation, errors, and path-selection behavior. Reuse Preferences for profile-bound palette configuration.

M22 implementation notes:

- Added `src/gui/dialogs/dialog_presentation.*` for shared modal sizing, minimum-size constraints, standard modal flags, footer separators/groups, and disabled action buttons.
- Migrated file selector/browser, map open/create/save/facet/dirty/overwrite/error, and Preferences dialogs to the shared presentation layer.
- Preserved stable popup IDs such as `###dialog.file-browser`, `###dialog.file-selector`, `###dialog.preferences`, and the existing map workflow IDs.
- Preserved domain-owned validation, file I/O, dirty-change resolution, collision policy, overwrite protection, and Preferences-based profile configuration.
- Standardized footer action ordering and disabled-state presentation without moving workflow execution into ImGui helpers.
- Added focused tests for dialog size bounds and relative standard/large/browser sizing.

M22 verification:

```text
cmake --build --preset debug-gui
ctest --preset debug-gui
clang-tidy src/gui/dialogs/dialog_presentation.cpp src/gui/dialogs/preferences_dialog.cpp src/gui/dialogs/file_browser_dialog.cpp src/gui/dialogs/file_selector_dialog.cpp src/gui/dialogs/new_map_dialog.cpp src/gui/dialogs/map_facet_dialog.cpp -p build/debug-gui
```

The configured `debug-gui` build passed, all 349 tests passed, and the explicit clang-tidy run completed without findings.

### M23: Add the map workspace manager

**Status:** Complete. Added one resident map workspace and map-canvas state per open map document. Activation now switches resident state without reopening maps, preserving staged edits, navigation, selection, display mode, tool state, and dependent caches. Profile changes invalidate provider-dependent canvas caches for the active and resident workspaces.

Create one persistent workspace per open map tab. Preserve reader, staged edits, navigation, selection, display mode, tool state, and dependent cache. Switching tabs is prompt-free; dirty resolution is scoped to close, replace, and quit. Invalidate provider-dependent caches for all workspaces on profile changes.

M23 implementation notes:

- Added `src/gui/documents/map_workspace_manager.*` keyed by map `DocumentId`, with resident `MapWorkspace` and `MapCanvasPanelState` instances.
- Kept existing panel and command APIs stable through active-workspace forwarding while moving canvas navigation/cache state into the manager.
- Map activation switches to the resident workspace instead of calling `openMap()` on a single global workspace. Failed activation leaves the current active workspace intact.
- Dirty-map switching is prompt-free once the map is resident. Save/discard handling remains in close, replace, and quit workflows.
- Provider changes invalidate low-detail canvas caches for every resident workspace without clearing map edits, selection, navigation, or display state.
- Added regression coverage for independent map/canvas state preservation and cache invalidation.

M23 verification:

```text
cmake --build --preset debug-gui
ctest --preset debug-gui
clang-tidy src/gui/documents/map_workspace_manager.cpp src/gui/profile/workbench_session.cpp src/gui/frontend/frame_composition.cpp src/gui/commands/command_registry.cpp src/gui/application/workbench_state.cpp -p build/debug-gui
```

The configured `debug-gui` build passed, all 351 tests passed, and the explicit clang-tidy run completed without findings.

### M24: Remove global map Edit Mode

**Status:** Complete. Removed the global map editing switch and made compatible map operations immediately available while preserving staged output safety and configured Radarcol read-only behavior.

Remove `editMode_`, `edit.toggle`, Edit Mode menus/status, and all operation gating based on the global switch. Gate by compatible document/tool state and preserve staged output safety.

M24 implementation notes:

- Removed `MapWorkspace::editMode_`, its accessors, the workspace-manager forwarding API, and the `edit.toggle` command/menu path.
- Map mutations now require an open map and compatible operation inputs; edits continue to use temporary working copies and remain dirty until explicit Save, Save As, or Discard resolution.
- Removed Edit Mode text from the map canvas, map information, and Edit Tools views. Edit Tools reports only the open-map requirement when operations are unavailable.
- Standalone Radarcol editing no longer depends on map state. The profile-bound Radarcol palette remains read-only, while standalone document editing remains governed by document access.
- Updated workspace and command tests to cover edits without a mode toggle and retained dirty transitions, discard, save output, and selection-scope protection.

M24 verification:

```text
cmake --build --preset debug-gui
ctest --preset debug-gui
clang-tidy src/gui/documents/map_workspace.cpp src/gui/documents/map_workspace_manager.cpp src/gui/commands/command_registry.cpp src/gui/commands/menu_bar.cpp src/gui/frontend/frame_composition.cpp src/gui/tools/edit_tools_panel.cpp src/gui/views/map_canvas_panel.cpp src/gui/views/map_info_panel.cpp src/gui/views/radarcol_panel.cpp -p build/debug-gui
```

The configured `debug-gui` build passed, all 351 tests passed, and the explicit clang-tidy run completed without findings.

### M25: Move Radarcol editing into the document

**Status:** Complete. `RadarcolDocument` now owns standalone Radarcol editor, output, dirty, access, validation/error, and save/discard lifecycle state. The central shared view consumes the document reader and emits typed document intents; configured sources remain immutable and read-only.

Move editor, output, dirty, validation, and save/discard state from `RadarcolPanelState` into `RadarcolDocument`. The shared view emits document intents. Configured sources remain read-only.

M25 implementation notes:

- Added `RadarcolDocumentIntent` and document-owned lifecycle methods for begin edit, raw color staging, Save, Discard, Save As, access changes, and recoverable errors. View actions are queued and processed after central view rendering, so file I/O does not run inside ImGui rendering callbacks.
- `RadarcolDocument` now privately owns `RadarColEditor`, output path, overwrite policy, dirty state, access state, and reader refresh after a successful commit. Public accessors and `open()`/`invalid()` factories expose only the document contract needed by views and document management.
- Configured Radarcol documents reject editing and abort any staged editor if access becomes read-only, including after profile rebinding.
- The central Radarcol adapter reads directly from `RadarcolDocument`; the panel no longer mirrors standalone reader/editor/output/document identity state.
- Document summaries synchronize Radarcol validation and dirty state for tab presentation. Profile-bound palette selection/cache state remains local to `RadarcolPanelState`.
- Added headless coverage for staged color changes, reader refresh after Save, Discard, failed/in-place output creation, and configured-source rejection.

M25 verification:

```text
cmake --build --preset debug-gui
ctest --preset debug-gui
clang-tidy src/gui/documents/radarcol_document.cpp src/gui/documents/map_document.cpp src/gui/profile/workbench_session.cpp src/gui/views/radarcol_panel.cpp src/gui/frontend/frame_composition.cpp src/gui/commands/menu_bar.cpp -p build/debug-gui
```

The configured `debug-gui` build passed, all 354 tests passed, and the explicit clang-tidy run completed without findings.

### M26: Complete standalone Radarcol save workflows

Implement Save, Discard, Save As, same-source protection, collision policy, recoverable failures, and redesigned confirmation/error dialogs.

### M27: Promote Radarcol as a metadata source

Implement transactional Save as Map Metadata Source, profile creation/activation, path rebinding, provider invalidation, and in-place transition to configured read-only.

### M28: Add real Texmap Pair documents

Add a reader-backed two-path document and central shared asset view. Show pair validity, paths, texture count, access, and source state. Keep editing disabled.

### M29: Promote Texmap Pairs

Implement atomic pair promotion, profile rebinding, and in-place configured read-only transition.

### M30: Add generic status and hover snapshots

Make status independent of map-only state. Support map, Radarcol, Texmap, palette, inspector, tool, document, source, access, item, and selection context.

### M31: Introduce tool contracts

Add stable tool IDs, compatibility, options, active state, shortcuts, disabled reasons, and typed intents.

### M32: Add the Tool Palette

Create a real adaptive palette view with stable IDs, text fallback, tooltips, active/disabled states, and default docking.

### M33: Connect selection and navigation tools

Route tile selection, block selection, and pan/move through tools while preserving current behavior.

### M34: Connect fill and replace tools

Route map mutations through typed operation requests and preserve staged writes and output safety.

### M35: Make layout and workspace persistence real

Connect `ImGui::LoadIniSettingsFromMemory()` and `ImGui::SaveIniSettingsToMemory()`, add schema migration, persist panel/layout state, and persist asset/document workspace state. Implement `Reset Workspace` without closing documents or discarding edits.

### M36: Add Texmap Pair editing

Add staged pair edits, atomic writes, Save, Save As, Discard, collision handling, and source promotion through the shared document/dialog architecture.

### M37: Add operation records and undo/redo

Add operation IDs, descriptions, preflight, affected counts, bounded history, and Undo/Redo for supported map operations.

### M38: Implement Diff and split document views

Create a Compare document with explicit Base and Target identities. Add the multi-pane central surface, allowing arbitrary open documents where the Compare workflow requires it. Do not implement splits as ad hoc map-panel state.

### M39: Add background operations and diagnostics

Move expensive statistics, diff, and asset operations off the frame path where measurement requires it. Add progress, cancellation, generations, stale-result rejection, and structured diagnostics.

## 8. Future Milestone Implementation Notes

The following notes are binding defaults for future sessions. If implementation reveals a reason to change one, update this document before changing the architecture. New user-facing strings belong in `src/gui/strings/` and new stable IDs require an explicit persistence decision.

### M20: Shared table renderer

- Build on `AssetAdapter`, `AssetWindowState`, `AssetQuery`, `AssetSelection`, `AssetDetails`, and adapter actions. Do not create a second record-view framework.
- Extend the adapter only with table-specific data that the current contract cannot express, such as column descriptors, column labels, sort keys, cell states, and optional preview cells.
- Keep table rendering capability-driven. An adapter that supports grid only must continue to render its grid; a table-only adapter should show the existing inline unavailable state until table support is declared.
- Use the same known-count range loading and adapter-owned query execution as the grid. Do not reintroduce page controls.
- Preserve stable item IDs and selection when changing between grid and table.
- Keep the bottom details pane and adapter action slots shared between both presentations.
- Do not add durable presentation persistence here unless the M35 persistence boundary is already available; define the state field now and persist it later if necessary.
- Test column capability declarations, table range requests, stable selection, query changes, empty/invalid rows, table-only adapters, and grid/table transitions.

### M21: Dialog contracts and modal host

- Introduce typed dialog request/result types and a single modal-host ownership path. Existing dialogs may remain visually unchanged during this milestone.
- Dialogs render state and emit typed actions; commands/session/document workflows perform file I/O, profile mutation, save/discard, and operation resolution.
- Define common actions such as accept, apply, cancel, back, discard, overwrite, and choose path without forcing every dialog to support every action.
- Keep stable popup IDs separate from visible titles and source all new visible text through `src/gui/strings/`.
- Establish one active modal workflow unless an explicit stacked-modal case is documented. File browser ownership must be unambiguous.
- Do not migrate all existing dialogs or redesign visual hierarchy in M21. That is M22.
- Test request/result round trips, cancellation, validation errors, default/cancel keyboard actions, modal replacement, and file-browser result delivery.

### M22: Dialog redesign and migration

- Preserve the current theme, stable IDs, and existing save/output safety behavior while improving spacing, hierarchy, validation, and footer actions.
- Migrate one workflow at a time: file selector/browser, map facet/open/create/save, dirty/overwrite/error flows, Preferences, Radarcol, and Texmap Pair.
- Split `NewMapDialogState` responsibilities where practical; do not add more unrelated workflow modes to the same state machine.
- Reuse Preferences for unavailable profile-bound palette configuration until a dedicated source workflow is justified.
- All destructive or output-changing actions must use typed dialog results and the existing collision/same-source policies.
- Test each migrated workflow headlessly and perform manual keyboard, resize, validation, overwrite, and cancellation acceptance.

### M23: Map workspace manager

- Add a separate manager keyed by canonical map `DocumentId` or document identity. Each open map gets one resident workspace while its tab remains open.
- Move or encapsulate reader, staged edits, navigation, pan/zoom, selection, display mode, active tool state, and map-dependent cache ownership. Do not make `DocumentManager` a second mutable workspace.
- Activation switches the active workspace reference; it must not close/reopen maps merely to switch tabs.
- Switching dirty maps is prompt-free. Save/discard resolution applies only to close, replace, or quit, including an inactive dirty tab close.
- Profile/provider revisions invalidate every workspace's dependent render cache without clearing staged edits or view state.
- Keep M13's deferred frame-boundary activation behavior.
- Do not add workspace restart persistence here; that belongs to M35.
- Test two maps with independent navigation/selection/staged changes, prompt-free switching, inactive dirty close, failed activation, profile cache invalidation, and workspace destruction after close.

### M24: Remove global map Edit Mode

- Remove `MapWorkspace::editMode_`, `editMode()`, `setEditMode()`, `CommandId::editToggle`, menu entries, and UI text once all consumers are migrated.
- Map mutations are available when a compatible map document/tool is active. Dirty/staged state replaces Edit Mode as the safety indicator.
- Preserve source protection, staged writes, Save, Save As, Discard, collision policy, and same-source checks.
- Radarcol editing must not depend on the map's Edit Mode; configured Radarcol remains read-only.
- Update map information, Edit Tools, Radarcol controls, command availability, and tests together. Do not leave a hidden compatibility switch.
- Test edits with no mode toggle, disabled non-map commands, staged dirty transitions, and source/output safety.

### M25: Move Radarcol editing into the document

- Add editor/output/staged dirty/validation state to `RadarcolDocument`. The document owns lifecycle; the asset view only renders snapshots and emits intents.
- Keep configured-source access immutable and read-only even when the source is invalid.
- Preserve local standalone selection/view state separately from profile palette selection.
- Define document intents for begin edit, edit color, save, discard, save as, and recoverable error display. Do not write files from the ImGui renderer.
- Remove editor ownership from `RadarcolPanelState` only after all standalone and central view paths use the document adapter.
- Test editor lifecycle, failed creation, staged color changes, abort, commit failure, document dirty summaries, and configured-source rejection.

### M26: Standalone Radarcol save workflows

- Implement Save, Discard, Save As, collision confirmation, same-source rejection, output-directory validation, and recoverable failure handling using M21/M22 dialogs.
- Normal Save updates the standalone document source/output state as defined by the document workflow but never changes the active profile.
- A failed commit leaves the document open and editable with staged state or a clear recoverable error; do not silently discard the editor.
- Keep the shared asset shell responsible only for actions and display. The document/command workflow performs writes.
- Test source/output aliases, output collisions, missing directories, repeated saves, failed writes, discard, and tab dirty indicators.

### M27: Radarcol profile promotion

- Implement promotion as an explicit transaction: validate Save As output, commit asset, create/name profile if needed, persist profile path, activate/rebind profile, then transition the document to configured read-only.
- If profile persistence or activation fails after asset output, retain a recoverable standalone document state and report the partial result clearly.
- Do not close the document or mutate the profile during generic Open or normal Save.
- Clear/recompute palette selection and provider-dependent caches after source rebinding.
- Test no-profile creation, existing-profile promotion, collision/same-source cases, profile-save failure, provider rebind, and in-place access transition.

### M28: Real Texmap Pair documents

- Add a reader-backed `TexmapPairDocument` with two canonical source paths, pair validation, reader/error state, source/access summary, and known texture count.
- A pair opened from either companion becomes one document identity; incomplete pairs remain in the browser/selector workflow.
- Render the central document through the shared asset shell with a Texmap adapter. Do not reuse the profile palette state as the document's ownership boundary.
- Keep standalone and configured pair documents read-only in this milestone. No TexMapsEditor integration yet.
- Test pair identity ordering, duplicate reuse, missing companion flow, invalid pair diagnostics, configured-source classification, central activation, and texture count/details.

### M29: Texmap Pair promotion

- Promotion updates both `texidx.mul` and `texmaps.mul` profile paths atomically from the application's perspective.
- Generic Open remains profile-neutral. Promotion is the only action that changes active profile paths.
- Rebind the shared texture provider and all affected palettes/documents after successful activation.
- On failure, preserve the previous profile and document access/source state and report whether an asset output was already committed.
- Test both companion orderings, missing/invalid pair paths, profile creation, partial persistence failure, provider rebind, and in-place read-only transition.

### M30: Generic status and hover snapshots

- Define a document/view-independent `HoverSnapshot` and global `StatusSnapshot`; views publish snapshots rather than writing map-only fields in `WorkbenchState`.
- Include view ID, document/source context, description, coordinates/asset ID when applicable, selection, zoom, active tool, provider source, and access state.
- Map, Radarcol palette/document, Texmaps palette/document, inspectors, and tools each publish only their own snapshot.
- The status bar renders hovered context on the left and global document/profile/dirty/access state on the right. Use `src/gui/strings/` for labels and formatters.
- Do not make status snapshots the ownership path for selection or document edits.
- Test precedence, stale snapshot clearing, no-hover fallback, source/access labels, and standalone metadata status.

### M31: Tool contracts

- Add stable `ToolId`, descriptors, document compatibility, options, active state, shortcut metadata, disabled reasons, and typed intents.
- Tools do not own documents or mutate views directly. They emit commands/operation requests consumed by document/workspace workflows.
- Keep existing selection, pan, fill, replace, and future Diff semantics intact while introducing adapters.
- Tool and icon IDs are persisted vocabulary; changing them requires a migration decision. New labels go through `src/gui/strings/`.
- Test compatibility, activation, disabled reasons, options, shortcut metadata, and intent construction.

### M32: Tool Palette

- Add a real adaptive palette view with stable view ID, default docking above inspectors, text fallback, tooltips, active/disabled states, and document-kind adaptation.
- The palette selects tools; it does not duplicate tool behavior or directly edit `MapWorkspace`.
- Preserve existing Edit Tools operations through adapters during the migration, then remove panel-only assumptions in M33/M34.
- Do not require an icon font. Symbolic icon IDs must always have text fallback.
- Test default layout descriptors, visibility, stable IDs, incompatible document state, keyboard activation, and reset layout behavior.

### M33: Selection and navigation tools

- Route tile selection, block selection, and pan/move through M31 tool intents while preserving current mouse/keyboard behavior.
- Map workspace remains the mutation/navigation owner; tools only construct intents and display options.
- Selection context must remain compatible with the existing map inspectors and status snapshots.
- Test activation, drag/range behavior, reverse selections, pan, zoom, keyboard shortcuts, and no-map disabled behavior.

### M34: Fill and replace tools

- Route fill, tile replacement, and elevation replacement through typed operation requests.
- Keep all edits staged in the map workspace and preserve explicit Save/Save As/Discard behavior.
- Preflight compatibility and affected scope before mutation; do not introduce undo history until M37.
- Ensure global Edit Mode is not reintroduced as an operation gate.
- Test whole-map, block, selection, invalid-region, staged-dirty, failure, and source-protection behavior.

### M35: Layout and workspace persistence

- Connect custom layout storage to `ImGui::LoadIniSettingsFromMemory()` at startup and `ImGui::SaveIniSettingsToMemory()` on controlled save/shutdown.
- Version layout and workspace schemas separately where their keys/state lifetimes differ. Preserve stable ImGui IDs and provide migration/reset behavior for removed views.
- Persist panel visibility, dock settings, document/workspace state where safe, and asset state keyed by asset kind, canonical source identity, and palette/document context.
- `Reset Workspace` resets layout, asset view state, and map view/tool state but does not close documents, discard edits, or alter profile files. The action is immediate and currently non-undoable.
- Do not persist raw reader/editor objects; persist only safe identities and view/query/navigation state.
- Test round trips, schema migration, unknown IDs, corrupt files, reset behavior, staged-edit preservation, and restart restoration.

### M36: Texmap Pair editing

- Add `TexMapsEditor` ownership to the Texmap Pair document, not the palette renderer.
- Stage paired index/data changes and commit them atomically from the GUI's perspective.
- Reuse Radarcol document editing/output safety patterns: Save, Save As, Discard, collisions, same-source protection, recoverable failure, and promotion.
- Configured pairs remain read-only. Normal edits must not mutate profile paths.
- Test edits to empty/existing textures, paired output failures, atomicity, discard, dirty state, and configured-source rejection.

### M37: Operation records and undo/redo

- Add operation records with stable IDs, descriptions, scope, affected counts, preflight result, and reversible state where supported.
- Start with map fill/replace operations. Do not claim universal undo for arbitrary reader or file operations.
- Derive dirty state consistently from staged operation history without writing source files during Undo/Redo.
- Keep operation execution separate from the ImGui frame; integrate background execution later where needed.
- Test bounded history, undo/redo ordering, failed operations, dirty transitions, Save/Discard interaction, and source protection.

### M38: Diff and split document views

- Add a Compare document with explicit Base and Target map identities and dimension compatibility.
- Implement the multi-pane central surface only here; do not add ad hoc split state to map panels before this milestone.
- Decide pane creation, arbitrary-document support, pane-qualified status, tool context, and persistence using a small prototype first. Record decisions in the open-points section.
- Diff must not mutate either source map. Navigation from a difference must identify the target document/pane explicitly.
- Test identity, dimension validation, empty/equal/different maps, pane activation, status context, and restart/reset behavior.

### M39: Background operations and diagnostics

- Move measured long-running work such as statistics, diff, texture preview loading, or asset queries off the frame path.
- Every worker result needs an operation ID, generation/source identity, cancellation policy, and stale-result rejection before publishing to GUI state.
- Define worker shutdown and application-close behavior before adding threads.
- Use structured diagnostics and progress state; user-facing text goes through `src/gui/strings/`.
- Do not background cheap synchronous reads without a measured UX/performance reason.
- Test cancellation, shutdown, stale profile/document results, progress, failure reporting, and no frame-thread mutation races.

## 9. Dependency Graph

```text
M00-M12 baseline
       |
       v
M13 central surface and anchoring
       |
       v
M14 cleanup -> M15 strings
                    |
                    v
              M16 asset contracts
                    |
                    v
              M17 virtualized grid
                /             \
               v               v
        M18 Radarcol      M19 Texmaps
                \             /
                 v           v
                  M20 table renderer
                         |
                         v
                  M21 dialogs -> M22 dialog migration
                         |
                         v
                  M23 map workspaces -> M24 no Edit Mode
                         |
                         v
                  M25 Radarcol document editing
                         |
                         v
                  M26 save -> M27 promotion

M28 Texmap documents -> M29 promotion

M30 status
  |
M31 tools -> M32 palette -> M33 navigation -> M34 mutations
  |
M35 persistence/reset
  |
M36 Texmap editing -> M37 history -> M38 Diff/splits -> M39 background work
```

M13, M14, and M15 should happen before adding more asset-specific UI. M16 and M17 are shared infrastructure and must precede both Radarcol and Texmaps migration. M23 is intentionally separate from M13 because the anchoring bug can be fixed without redesigning map workspace ownership. M38 is the first milestone that implements split document panes.

## 10. Confirmed Open Points for Later

These are deliberately not resolved by the current roadmap. Each must be decided when its owning milestone becomes active, using measurements and a small prototype rather than assumptions.

### 10.1 Central document splits

- Whether split panes should be user-created and freely rearranged or primarily managed by Compare/Diff.
- Whether arbitrary documents, such as Map plus Radarcol, are useful in a split or whether the feature should focus on Map comparisons.
- Whether each pane needs independent tool and selection context.
- Whether panes should share a single status bar or publish pane-qualified status.
- Whether split arrangement belongs in document state, workspace state, or layout persistence.

### 10.2 Map workspace memory

- Whether all open maps can remain resident indefinitely for expected map sizes.
- Whether a bounded workspace cache or disk-backed inactive workspace is needed.
- Whether map canvas textures should be discarded while inactive and rebuilt on activation.
- Whether per-map tool state should persist independently from map view state.
- Whether staged edits need memory limits or a visible resource diagnostic.

### 10.3 Asset loading and virtualization

- Whether synchronous visible-range reads are fast enough for real Texmap files and slow storage.
- Whether neighborhood prefetch improves interaction enough to justify complexity.
- Whether background loading is needed, and if so, which generation/cancellation contract is required.
- Whether fixed cell metrics are sufficient for future visual assets.
- Whether unknown-count or streaming MUL sources should be supported.
- Whether a GPU-backed texture cache is needed for large texture collections.
- Whether item loading should expose retry, cancellation, and partial-source diagnostics.

### 10.4 Asset queries and selection

- Which Radarcol filters and sort keys are genuinely useful beyond ID, category, and presence.
- Which Texmap fields are reliable enough for sorting/filtering across invalid entries.
- When multi-selection becomes actionable for map tools.
- Whether palette selections should support cross-kind operations in the future.
- Whether selection should survive a source revision when the same stable item ID still exists.
- Whether drag payloads should eventually target map tools, inspectors, exporters, or profile workflows.

### 10.5 Table and record views

- Whether the shared table renderer needs variable row heights.
- Whether table-only MULs need grouped rows, trees, relationships, or nested details.
- Whether arbitrary record adapters need a richer schema/type system than descriptors and callbacks.
- Whether a record view should support column customization and per-source column persistence.
- Whether table and grid should share one query toolbar or allow format-specific query layouts.

### 10.6 Profile and palette behavior

- Whether a profile-bound palette should offer a direct shortcut to a specific Preferences section.
- Whether profile changes should show a rebind summary when many documents or maps are affected.
- Whether invalid configured Texmaps should offer a diagnostic path to pair discovery.
- Whether profile-bound palettes should expose source revisions or only human-readable source status.
- Whether future profile sources need a common provider capability registry.

### 10.7 Dialog and string evolution

- Whether the English-only string layer should later become localization-ready.
- Whether dynamic diagnostics need structured message IDs rather than formatted strings.
- Whether the dialog host should support stacked modals or enforce one modal workflow at a time.
- Whether dialog state should be restorable after application restart.
- Whether `Reset Workspace` should eventually offer category-specific reset options.

### 10.8 Persistence and recovery

- Whether asset view state needs schema migration independent of layout schema.
- Whether persisted canonical paths need platform-specific normalization/versioning.
- Whether workspace reset should remain immediate and non-undoable after user feedback.
- Whether document tabs and per-map workspaces should be restorable across application restarts.
- Whether crash recovery should preserve staged map or asset edits.

### 10.9 Background operations and history

- Which operations need worker threads first: statistics, texture preview loading, diff, or asset queries.
- Whether operation history should cover metadata edits as well as map edits.
- Whether Radarcol and Texmap edits can share one operation/history contract.
- Whether cancellation should roll back partial model changes or only prevent result publication.
- Whether progress belongs in the status bar, Operations view, or both.

## 11. Cleanup Inventory

M14 completed the confirmed dead-code and duplicate-code cleanup. `DataProviders::tileData()` was intentionally retained because it is part of the active provider contract and is covered by GUI tests.

Remaining cleanup is tied to later ownership milestones:

- Share checkerboard and preview drawing as part of the shared asset window work.
- Remove remaining panel-local provider authority as effective asset models are introduced.
- Reduce direct file-result routing in `menu_bar.cpp` as typed dialog/document results become available.

Any future removal must still check tests, build targets, installed headers, and documented ownership before deleting an API. No compatibility wrapper is needed for purely internal dead code.

## 12. Verification Strategy

Every milestone must record the commands used and preserve the normal baseline:

```text
cmake --build --preset debug-gui
ctest --preset debug-gui
```

Production GUI changes should run the repository's configured static checks for touched sources. Do not copy historical test counts into this document; record the current count from the verification run in the milestone change.

### 12.1 Headless coverage

Add or maintain focused tests for:

- Document identity and canonical duplicate reuse.
- Profile configured-source rebinding.
- Provider fallback and revisions.
- Central document activation requests and deferred mutations.
- Workspace manager lifecycle and prompt-free map switching.
- Provider cache invalidation across inactive map workspaces.
- Asset adapter descriptors and capability declarations.
- Visible-range and known-count calculations.
- Stable selection, range selection, filtering, and sorting behavior.
- Per-source asset state persistence and Reset Workspace.
- Radarcol and Texmap adapter models.
- Table-only adapter representation.
- Dialog requests, results, validation, and common actions.
- Promotion atomicity and read-only transitions.
- Layout serialization and ImGui docking round-trip.
- Operation history, cancellation, and stale-result rejection.

### 12.2 Manual acceptance matrix

At minimum, manually verify:

- Map -> Radarcol -> map switching with no unanchored window or flicker.
- Resize and log-panel changes without dock-tree reconstruction.
- Explicit layout reset.
- No profile with embedded Radarcol fallback.
- Missing and invalid configured Radarcol.
- Missing and invalid configured Texmap Pair with Preferences action.
- Global palettes with no active map.
- Standalone Radarcol central document and local selection restoration.
- Profile-bound Radarcol palette selection exposed to map/tools.
- Continuous Radarcol and Texmap scrolling without page controls.
- Loading placeholders during visible-range reads.
- Filtering and sorting with stable item selection.
- Large texture collections and slow storage.
- Map tab switching with staged changes and no save prompt.
- Closing an inactive dirty map tab through save/discard.
- Reset Workspace preserving documents and staged edits while resetting view state.
- Configured sources remaining read-only after profile changes.
- Dialog keyboard behavior, validation, overwrite, and error flows.
- Paths containing spaces and non-ASCII names.
- Small and large windows.

## 13. Immediate Next Work

The next implementation work is limited to M26:

1. Complete standalone Radarcol Save, Discard, Save As, collision, same-source, and recoverable failure workflows using the M21/M22 dialog boundary.
2. Keep durable asset presentation persistence deferred to M35.

Do not begin Radarcol document-owned editing or Texmap Pair document work until the dialog boundary and shared asset behavior remain stable.

## 14. Future Session Handoff

This section is the starting point for the next GUI session.

### Current completed state

- M13: central document surface, deferred tab mutations, dock rebuild stabilization, and configured Radarcol read-only enforcement.
- M14: dead/duplicate GUI cleanup and shared color conversion.
- M15: centralized English display strings under `src/gui/strings/`. All new user-facing strings must be added there first.
- M16: format-neutral asset adapter contracts under `src/gui/assets/asset_window_contracts.*`.
- M17: shared responsive asset grid shell under `src/gui/assets/asset_window.*`.
- M18: Radarcol palette and standalone view migrated to the shared shell.
- M19: Texmaps palette migrated to the shared shell; standalone Texmap Pair remains M28.
- M20: shared table renderer, Grid/Table toggle, table-only fallback, and session-scoped per-source/document presentation restoration.
- M21: typed dialog contracts and single-workflow modal host.
- M22: shared dialog presentation rules and migration of existing dialog workflows with stable IDs and output safety preserved.
- M23: resident per-map workspaces and canvas state with prompt-free activation and provider-cache invalidation.
- M24: map mutations gated by compatible document/tool state with the global Edit Mode removed.
- M25: standalone Radarcol editor, output, dirty, access, validation, and document intents owned by `RadarcolDocument`.

### Most recent grid correction

The original shared grid used an ImGui table plus `ImGuiListClipper`. That produced only one or two visible rows even when the reader contained many items. The current implementation uses:

- A fixed adapter cell height.
- The grid child sized to the remaining window height after header/details.
- Scroll-position-based first/last visible row calculation.
- Adapter range requests for only the visible row window.
- Explicit item placement without an ImGui table.

The details pane is fixed below the grid. Radarcol and Texmap cells render only the swatch/thumbnail. The raw numeric ID is exposed through the shared hover tooltip; do not reintroduce `Color N` or `Texture N` text beside cells.

### First verification steps

Run these commands before making further GUI changes:

```text
cmake --build --preset debug-gui
ctest --preset debug-gui
clang-tidy src/gui/assets/asset_window.cpp src/gui/views/radarcol_panel.cpp src/gui/views/texmaps_panel.cpp -p build/debug-gui
```

The last verified baseline was 351 passing tests. `git diff --check` must also pass.

### First manual checks

Use the existing project-root fixtures and verify:

1. Open the Radarcol palette and confirm the grid shows more than two rows when the source has more items.
2. Open the Texmaps palette and confirm the grid shows more than two rows and scrolls continuously.
3. Resize the window and confirm the number of columns changes with available width.
4. Confirm the header stays fixed, the grid receives the remaining vertical space, and the details pane stays fixed at the bottom.
5. Hover a Radarcol swatch and Texmap thumbnail and confirm the raw numeric ID tooltip.
6. Select an item and confirm details update below the grid.
7. Confirm empty/invalid sources do not create an extra overflowing shell scrollbar.
8. Confirm configured Radarcol remains read-only and standalone Radarcol editing controls still appear where permitted.

If the two-row symptom returns, inspect `src/gui/assets/asset_window.cpp` first. Do not restore table/clipper virtualization without a reproducible row-height test.

### Next implementation sequence

1. M24: map mutations are available without a global mode switch; dirty/staged state and output safety remain authoritative.
2. M25: standalone Radarcol editor/output/dirty state is owned by `RadarcolDocument` and rendered through document intents.
3. M26-M27: complete Radarcol save and profile-promotion workflows.
4. M28-M29: add real Texmap Pair documents and promotion.

### Known transitional limitations

- Radarcol standalone editor/output/dirty state is document-owned; profile-palette reader/cache state remains panel-local until later provider/view consolidation.
- Texmaps reader and thumbnail loading are still held by `TexmapsPanelState` until a fuller adapter/document ownership pass.
- Asset view state is modeled but durable restart persistence waits for M35.
- Table mode is implemented in the shared shell; durable presentation persistence remains deferred to M35.
- Dialog requests/results, single-modal ownership, and shared dialog presentation rules are implemented; domain-specific dialog workflows remain transitional until their later ownership milestones.
- Map workspace state is resident per open map document; durable workspace restart persistence remains deferred to M35.
- Split document panes remain deferred to M38.
- Manual visual acceptance is still required; headless tests cannot prove compositor flicker, exact responsive layout, or scroll feel.
