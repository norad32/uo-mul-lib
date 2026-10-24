# GUI Extension Milestones

Milestones are release slices. Each slice is intentionally small and has a
clear dependency and verification boundary. Do not start a later slice while
its prerequisite contract is missing.

## M0. Architecture migration gate

**Depends on:** none.

Complete the active architecture work through the provider and metadata-view
boundary before starting feature milestones.

Deliverables:

- Deferred frame mutations.
- Role-aware source identity.
- Source locks.
- Document descriptor/factory boundary.
- Effective provider bindings.
- Typed dialog/operation path for at least the Map workflow.
- Tool and Tile Source contracts.

Tests:

- Frame activation/close integration.
- Document identity and lock lifecycle.
- Provider/metadata-view consistency.
- Save As and output transaction failure.

## M1. Tiledata contracts

**Depends on:** M0.

Define the version-neutral model before choosing a binary layout.

Deliverables:

- `TiledataSource`.
- `MapTileRecord`.
- `StaticTileRecord`.
- `TiledataProvider` status/revision contract.
- Standalone Tiledata document contract.
- Profile-bound Tiledata provider binding.
- Land/item metadata-view capability descriptors.
- `Use as Tile Source` validation for the Land and Static slots.

Not included:

- Binary layout selection.
- Tiledata editing.
- Profile promotion UI beyond the shared contract.

Tests:

- Record model round trips.
- Provider status and revision transitions.
- Incompatible Land/Static Tile Source rejection.
- Unresolved persisted selections.

## M2. Tiledata reader slice

**Depends on:** M1 and a selected client/layout decision.

Deliverables:

- One strict reader for the selected layout.
- File-size and record-count validation.
- Land and item lookup.
- Names and flags.
- Structured byte-offset diagnostics.
- Profile validation integration.
- Standalone read-only document/metadata view.

Tests:

- Valid fixture.
- Truncated fixture.
- Wrong-layout fixture.
- Boundary record IDs.
- Land/item lookup.
- Missing/invalid profile source.

## M3. Static source model

**Depends on:** M0 and M1.

Deliverables:

- `StaticsSource` with `staidx` and `statics` roles.
- Immutable facet key and dimensions.
- Strict static index/data validation.
- Variable-length block reader.
- Session-only stable static object IDs.
- Static-only document contract.
- Map-like dimensions/facet open workflow.

Tests:

- Valid block with zero, one, and many records.
- Missing companion.
- Short index.
- Misaligned data length.
- Duplicate records.
- Unknown fields retained in the in-memory record.
- Static-only document identity and duplicate reuse.

## M4. Static standalone read/write workspace

**Depends on:** M3 and M0 output transactions.

Deliverables:

- Static workspace with staged logical records.
- Place, delete, move, and property-edit operation contracts.
- Unknown-field preservation.
- Duplicate preservation.
- Source-order preservation.
- Independent dirty state.
- Save Layer and Save As.
- Alias protection.
- Recoverable save failure.

Gesture choice is not part of this milestone. Operations must be testable
without ImGui first.

Tests:

- Operation preflight and mutation.
- Dirty/discard behavior.
- Save failure preserves staged state.
- Paired output publication.
- Same-source and overlapping-output rejection.
- Save As updates only the Static source identity.

## M5. Static defragmentation

**Depends on:** M4.

Deliverables:

- Explicit Defragment Statics command.
- Compact pair writer.
- Manual-by-default preference.
- Optional automatic defragmentation on Save.
- Defragmentation report.

Tests:

- Unused payload removal.
- Index offsets and lengths.
- Duplicate preservation.
- Order preservation.
- Selected object IDs remain valid in the open document.
- Preference and command availability.

## M6. Map and Statics Combine

**Depends on:** M3 and M0 source locks.

Deliverables:

- Filename-hint candidate discovery.
- Explicit candidate/path confirmation.
- Exact facet-key and dimension validation.
- Persistent Combine available action.
- Combined document with one Map and one Statics layer.
- Exclusive source locks.
- Generic Open focus-and-inform behavior.
- One combined tab with layer indicators.

Tests:

- Compatible and incompatible combine.
- Existing standalone Statics document candidate.
- Different directories.
- Same facet key with alternate paths.
- Locked source focus behavior.
- Source lock release on close.

## M7. Combined Save, Split, and layer state

**Depends on:** M6 and M4.

Deliverables:

- Independent layer dirty states.
- Save Map and Save Statics actions.
- Layer-specific Save As.
- Combined document split action.
- Preservation of source/output identity and staged changes.
- Recreated standalone Map and Statics tabs.

Tests:

- Saving one layer leaves the other untouched.
- Save As changes only the selected layer identity.
- Split clean document.
- Split dirty document.
- Split after independent layer Save As.
- Reopen source after combined close.

## M8. Static-aware world rendering

**Depends on:** M3, M6, M1, and Art provider contract.

Deliverables:

- World cell context.
- Static layer visibility.
- Tiledata-aware static ordering.
- Art-backed previews.
- Missing Art degraded rendering.
- Missing Tiledata reduced metadata rendering.
- Layer-filtered selection.
- Static inspector listing all statics at a coordinate.

Tests:

- Ordering by Z/Tiledata/source order.
- Duplicate display and selection.
- Land-only and Statics-only selection.
- Inactive-layer inspector context.
- Provider revision cache invalidation.

Manual acceptance:

- Large static-dense map.
- Missing Art.
- Missing Tiledata.
- Multiple statics at one coordinate.
- Combined document navigation.

## M9. Toolbar and tool prototype

**Depends on:** M0, M7, and M8.

Deliverables:

- Tool registry.
- Select and Pan tools.
- Land Edit and Static Edit compatibility.
- Active Land/Statics layer control.
- Land Tile Source slot.
- Static Tile Source slot.
- Explicit metadata-view `Use as Tile Source` action.
- Text fallback and stable tool IDs.

Gesture selection is a prototype task. Do not stabilize placement/move/delete
gestures until the domain operations and inspector are usable headlessly.

Tests:

- Tool compatibility.
- Active layer restrictions.
- Source validation.
- Persisted source slots.
- Unresolved source behavior.
- Toolbar reset/layout behavior.

## M9.1. Icon resources and semantic icon vocabulary

**Status:** Complete. M9.1.a-h are implemented and verified: the pinned font
asset, shared resource lookup, 32-bit semantic mappings, one-time atlas loading,
headless coverage, hidden icon gallery, and mapped-control adoption are complete.
Configured GUI/model builds and focused/full tests pass; runtime package assets
were also verified.

**Depends on:** the implemented M9 tool foundation and the shared presentation
boundary.

This is a presentation milestone. It does not add editing behavior or make
tools depend on font availability.

### M9.1 bite-sized implementation plan

- [x] **M9.1.a Resource contract:** vendor the pinned MDI TTF, Apache license,
  source metadata, and build/install/package asset rules.
- [x] **M9.1.b Resource lookup:** add shared application resource resolution for
  app icon and font, including build-tree, installed, standalone, and explicit
  environment-override paths.
- [x] **M9.1.c Semantic iconography:** add the 81-item `IconId` vocabulary with
  filled and outline variants, one MDI mapping table, UTF-8 encoding, optional
  glyph results, and derived ImGui ranges.
- [x] **M9.1.d Font atlas:** merge the icon font into the default ImGui font
  once after context creation and before renderer atlas construction; preserve
  text-only operation with a logged diagnostic on failure.
- [x] **M9.1.e Headless verification:** test mappings, UTF-8, atlas coverage,
  resource paths, missing-resource diagnostics, and package contents.
- [x] **M9.1.f Icon gallery:** add a hidden Windows/developer panel covering all
  semantic IDs, interaction states, DPI sizes, and icon-plus-label alignment.
- [x] **M9.1.g GUI adoption:** add optional icons to mapped controls and static
  labels; keep important labels visible, use icon-only only for compact controls,
  and leave unmapped controls text-only.
- [x] **M9.1.h Closeout:** run the configured GUI build/tests, record the exact
  milestone status, and update the handoff without touching unrelated changes.

Deliverables:

- Pinned Pictogrammers Material Design Icons desktop TTF and Apache 2.0 license.
- Versioned font asset and recorded upstream source URL.
- Application-owned ImGui codepoint configuration, including 32-bit codepoints
  if required by the selected release.
- GUI resource lookup shared by the application icon and font asset.
- One-time font loading after ImGui context creation and before renderer atlas
  construction.
- Semantic `IconId` vocabulary independent of MDI names and codepoints.
- One reviewed mapping table used for both UTF-8 glyph lookup and the ImGui
  glyph range.
- Semantic mappings for the 81-item presentation vocabulary, including filled
  and outline variants, canonical MDI names, all current tools, and common
  dialog/status concepts.
- Explicit fallback state when the font is missing or cannot be loaded.

The planned presentation boundary is:

```text
src/gui/
├── application/gui_resources.*
├── presentation/fonts/imgui_fonts.*
└── presentation/icons/iconography.*
```

The font module owns atlas construction only. Resource lookup owns installed and
development paths. Iconography owns semantic names, codepoints, UTF-8 glyphs,
and availability. None of these modules know about documents, tools, panels, or
profile state.

Tests and acceptance:

- Every semantic icon has a valid mapping and UTF-8 encoding.
- Every mapped codepoint is included in the atlas range.
- Missing fonts produce a diagnostic and preserve text controls.
- No persisted identifier contains an icon codepoint.
- Build-tree execution finds the font.
- Installed normal and standalone GUI runtimes find the font.
- GUI-capable packages contain the font and license.
- A hidden developer icon gallery verifies normal/hovered/active/disabled states,
  normal/high-DPI sizing, and icon-plus-label alignment.

Do not use glyphs for dynamic map overlays. Do not add raw MDI constants to map
operation renderers. Associate icons with tool descriptors only after the
semantic tool model exists.

## M9.6. Shared selection mask

**Status:** Sparse mask ownership, workspace selection operations, Select
All/None/Invert commands, clipped block conversion, reverse block-to-tile
conversion, sparse bounds, and sparse selection statistics are implemented and
covered by focused tests. Rectangular fields remain only as a compatibility
bridge for existing public consumers.

**Depends on:** the implemented M9 tool foundation. M9.1 glyph resources are
presentation-only and do not block the headless selection contracts below.

Replace the current rectangular-only selection ownership with one document-local
sparse tile/block mask. Keep this slice independent from Fill and Replace.

Deliverables:

- Sparse tile and block membership.
- Rectangle click and drag selection.
- Shift add and Ctrl subtract.
- Select All, Select None, and Invert.
- Covering-block conversion when changing from tiles to blocks.
- Clipped edge-block handling.
- Selection survives tool changes and remains document-local.

Tests:

- Sparse membership and empty masks.
- Click and drag regions.
- Add and subtract behavior.
- Select All, None, and Invert.
- Tile-to-covering-block conversion.
- Edge-block clipping.
- Document activation preserves separate masks.

## M9.7. Selection rendering and canvas routing

**Status:** Pan, Space-temporary panning, rectangle selection, Free Select
capture, Shift/Ctrl combine behavior, sparse tile/block mask rendering, and
animated marching ants are implemented, with mask rendering limited to the
outer boundary of contiguous tile/block selections. Boundary segments use a
continuous phase and remain stable at high zoom; the live Free Select path uses
one continuous animated polyline for diagonal segments. Explicit canvas
picking and Alt-click Active Tile sampling are implemented. Manual interaction
acceptance remains.

**Depends on:** the implemented M9 tool foundation and M9.6.

Route canvas gestures through the active tool and render the selection without
adding mutation operations.

Deliverables:

- Move/Pan left-drag behavior.
- Space-temporary panning from other tools.
- Rectangle selection rendering.
- Freehand lasso capture and automatic close on release.
- Sparse-mask marching-ants rendering with tile/block spacing.
- Alt-click and explicit eyedropper sampling into Active Tile.

Manual acceptance:

- Pan behavior matches the selected Move tool and temporary Space behavior.
- Rectangle and lasso gestures preserve modifiers.
- Marching-ants borders remain visible at different zoom levels.
- Sampling updates Active Tile without changing the active tool.

## M9.8. Active Tile and selection tool options

**Status:** Fuzzy selection options and synchronous headless scanning are
implemented and wired through the palette/canvas path; focused model tests and
the configured GUI build pass. Manual visual acceptance and preference
persistence remain.

**Depends on:** M9.7 and M2.

Implement the options needed by selection tools and the shared Active Tile
control. Tiledata remains an optional provider for fuzzy name matching.

Deliverables:

- Bounded Active Tile ID and Z fields.
- Rectangle/Lasso shared Tile/Block option.
- Per-field Fuzzy ID, Z, and normalized name inputs.
- Per-field Fuzzy eyedroppers.
- Contiguous and whole-map Fuzzy scope.
- ID/Z/name AND predicates.
- Tiledata-unavailable disabled name criterion.
- Synchronous tile-only fuzzy scanning.

Tests:

- Numeric bounds and invalid input.
- Fuzzy predicate combinations.
- Exact normalized name matching.
- Tiledata-unavailable behavior.
- Contiguous and whole-map result sets.
- Fuzzy block-mode rejection.

Implemented slice:

- Fuzzy scope supports contiguous flood selection and whole-map scanning.
- ID, Z, and normalized Tiledata-name predicates combine with AND semantics.
- Tiledata-name matching is disabled when the provider is unavailable.
- Fuzzy selection remains tile-only, with synchronous scanning and explicit
  canvas execution.
- Focused tests cover option bounds, normalized names, contiguous versus
  whole-map results, unavailable Tiledata, and block-mode rejection.

## M9.9. Masked Fill and Replace operations

**Status:** Implemented headlessly and wired through the palette/canvas path;
focused model tests and the configured GUI build pass. Manual visual acceptance
of palette controls, canvas click execution, and status-bar feedback remains.

**Depends on:** M9.6 and M9.8.

Connect edit tools to staged MapWorkspace operations using the shared sparse
selection mask. Do not add undo/redo in this slice.

Deliverables:

- Fill ID-only, Z-only, and ID+Z modes from Active Tile.
- Replace ID, Z, and ID+Z modes.
- Independent Replace source/target ID/Z fields.
- Manual and eyedropper source/target population.
- Canvas-click execution for both tools.
- No-selection rejection with status-bar feedback.
- Changed-count reporting and dirty-state updates.

Tests:

- Masked ID, Z, and ID+Z Fill.
- Masked ID, Z, and ID+Z Replace.
- Unselected cells remain unchanged.
- No-selection rejection.
- Failed operations preserve prior dirty state.
- Successful operations retain tool and selection.

Implemented slice:

- `MapSelectionMask` exposes sparse tile/block iteration without exposing its
  encoded keys.
- `MapWorkspace::fillMasked` and `replaceMasked` stage only selected cells,
  preserve unselected cells, return changed-tile counts, and retain the mask
  after a successful transaction.
- Fill and Replace modes, independent Replace source/target fields, palette
  controls, canvas click routing, and transient status-bar feedback are wired.
- Focused tests cover all six masked modes, sparse tile/block behavior,
  unselected-cell preservation, no-selection rejection, changed counts, and
  tool option bounds.

## M9.10. Tool option persistence and menu orientation

**Status:** Tool-option persistence and menu orientation are implemented through the existing
preferences store; selection masks remain session-local and are never restored
after restart. Region mode (`Tiles`/`Blocks`) is persisted globally, while each
open document retains its own mask during session switching. The visible menu
taxonomy is oriented as File, Edit, Select, World, Tools, View, Windows, and
Help. Manual menu traversal acceptance remains.

**Depends on:** the implemented M9 tool foundation, M9.8, and M9.9.

Persist the global tool configuration and complete the first menu orientation
without prematurely implementing unrelated menu commands.

Deliverables:

- Persist active tool and all tool options.
- Persist Tool Palette layout and visibility through the layout system.
- `File`, `Edit`, `Select`, `World`, `Tools`, `View`, `Windows`, and `Help`
  top-level taxonomy documented.
- Preferences is placed at the bottom of `Edit`, after the reserved editing
  commands.
- Select All, None, and Invert in Select.
- Map navigation/display commands oriented under World.
- Panel visibility oriented under Windows.
- Edit remains reserved for future Undo/Redo and clipboard actions.

Tests and acceptance:

- Preference round trips and unknown-key tolerance.
- Defaults for missing tool settings.
- Palette layout reset.
- Tool activation agrees across palette, Tools, and Select.
- Manual menu traversal matches the documented taxonomy.

## M10. Basic static editing UI

**Depends on:** M4, M8, and M9.

Deliverables:

- Static inspector refinement to one object.
- Initial placement gesture chosen from the prototype.
- Initial movement gesture chosen from the prototype.
- Delete action through inspector or command.
- Static property editing.
- Configurable Z policy modes:
  - explicit Z;
  - land Z;
  - topmost/static-aware Z.

Tests:

- Place/delete/move through UI intents.
- Active-layer enforcement.
- Art and Tiledata requirements.
- Static-only coordinate editing.
- Staged Save/Discard.

## M11. Metadata promotion

**Depends on:** M1, M2, M0 promotion workflow.

Implement one format at a time:

1. Radarcol completion and shared promotion transaction.
2. Texmap Pair promotion.
3. Art promotion.
4. Tiledata promotion.

Each format must:

- Resolve dirty state.
- Save the current output.
- Replace the active profile source.
- Persist the profile.
- Rebind providers/documents.
- Close the standalone document.
- Preserve recoverable state on partial failure.

Tests per format:

- Clean promotion.
- Dirty Save/Discard/Cancel.
- Profile persistence failure.
- Provider rebind failure.
- Read-only transition.
- Metadata-view source/status update.

## M12. Generic indexed-image infrastructure

**Depends on:** M0 and source-set validation.

Deliverables:

- Sparse indexed entry model.
- Index bounds and sentinel policies.
- Lazy payload lookup.
- Decode result/status model.
- Image dimension metadata.
- Source revision/cache keys.
- Separate codec boundary.

Art and Gump must use the shared infrastructure but retain separate codecs.

Tests:

- Empty entry.
- Sparse range.
- Invalid index.
- Payload bounds.
- Decode failure.
- Cache invalidation.

## M13. Art

**Depends on:** M12, M1, and M11 promotion infrastructure.

Deliverables:

- Art reader for the initial legacy MUL form.
- Land/static ID namespaces.
- Art provider and metadata view.
- Standalone Art document.
- Static Tile Source integration.
- Lazy thumbnail/cache behavior.
- Read/write editor boundary, even if editing is a later slice.

Tests:

- Land/static ID ranges.
- Empty and variable-size images.
- Invalid payload.
- Static Tile Source validation.
- Metadata-view/document source separation.

## M14. Gumps

**Depends on:** M12.

Deliverables:

- Gump reader and codec.
- Gump provider/metadata-view/document.
- Sparse/large ID range handling.
- Decode diagnostics.
- No toolbar Tile Source integration initially.

Tests:

- Empty gump.
- Sparse high ID.
- Invalid/compressed unsupported payload.
- Metadata-view/document state separation.

## M15. Cliloc

**Depends on:** M0 source descriptors.

Deliverables:

- Language-qualified Cliloc source.
- Standalone document.
- Strict parser for the selected format.
- Duplicate ID policy.
- Encoding policy.
- Structured parse diagnostics.

Cliloc remains document-only initially. It is not profile-bound or a promotion
target in this plan.

Tests:

- Language identity.
- Variable-length entries.
- Invalid lengths.
- Unsupported compression.
- Duplicate IDs.
- Reopen and persistence behavior.

## M16. Tiledata editor

**Depends on:** M2, M11, and the selected layout's writer design.

Deliverables:

- Tiledata staged editor.
- Land/item property operations.
- Save/Save As.
- Profile promotion reuse.
- Unknown-field preservation where supported.

Tests:

- Land/item edits.
- Dirty/discard/save.
- Invalid output.
- Profile source protection.
- Provider rebind after promotion.

## M17. Operation history

**Depends on:** M4, M7, and M10.

Deliverables:

- Operation records.
- Bounded undo/redo.
- Land/static operation descriptions.
- Dirty state derived from staged history where safe.
- Save/Discard integration.

Do not claim universal undo for arbitrary file or provider operations.

Tests:

- Ordering.
- Failed operation.
- Undo/redo dirty transitions.
- Bounded history.
- Combined-layer scope.

## M18. Background operations

**Depends on:** M8 and M17 where history is affected.

Deliverables:

- Operation IDs and generations.
- Cancellation policy.
- Stale-result rejection.
- Progress/diagnostics surface.
- Worker shutdown behavior.

Candidate operations are static loading, image decoding, statistics, bulk edits,
and diff. Move only measured frame-path work.

Tests:

- Cancellation.
- Shutdown.
- Source/document rebind during work.
- Stale result.
- Failure and progress publication.

## M19. Remaining metadata editors

**Depends on:** M13, M14, M15, M16, and the shared output/document workflows.

Complete editing one format at a time. Each format is a separate reviewable
slice; do not combine their binary writers into one milestone.

### M20.1 Art editor

Deliverables:

- Staged land/static image edits.
- Replace/remove operations.
- Format-size validation.
- Save/Save As and collision handling.
- Source/output alias protection.
- Promotion reuse for profile Art.

Tests:

- Land and static namespaces.
- Invalid dimensions/encoded size.
- Dirty/discard/save failure.
- Cache invalidation after Save.
- Profile read-only enforcement.

### M20.2 Gump editor

Deliverables:

- Staged image replacement/removal.
- Sparse and high-ID handling.
- Save/Save As for the supported legacy source.
- Decode and output diagnostics.
- No profile promotion unless a later product decision adds it.

Tests:

- Empty and sparse entries.
- High IDs.
- Invalid payloads.
- Dirty/discard/save failure.
- Source/output alias protection.

### M20.3 Cliloc editor

Deliverables:

- Staged string edits.
- Duplicate-ID policy enforcement.
- Encoding validation.
- Save/Save As and language-preserving identity.
- Recoverable parse/write diagnostics.

Tests:

- Add/update/delete entries.
- Duplicate IDs.
- Invalid text/lengths.
- Dirty/discard/save failure.
- Language identity and reopen behavior.

## M20. Future overlays and alternate containers

**Depends on:** format requirements and user demand.

UOP, Verdata, alternate client layouts, and additional static layers are not
initial scope. Add them only through a source-set/codec milestone with explicit
read/write and recovery decisions.

## M21. Deferred domain work

These items remain blocked until the product and domain decisions are complete.
Do not implement speculative GUI models for them.

### M21.1 Statics and Combined documents

Requires source-set identity, exact role-aware locks, immutable facet metadata,
Combine/Split contracts, and a Static workspace model.

Deliverables:

- Statics source set and validation.
- Static workspace and staged records.
- Combined document with independent Map and Statics layers.
- Combine/Split workflows and lock lifecycle.

Tests:

- Compatible and incompatible Combine.
- Exact source-lock acquisition and release.
- Static-only and Combined document identity.
- Dirty Split and recoverable failure behavior.

### M21.2 Tools and active layers

Requires stable tool descriptors, compatibility rules, options, shortcuts,
disabled reasons, typed intents, and an active Land/Statics layer policy.

Deliverables:

- Tool registry and stable tool IDs.
- Select/Pan and layer-compatible editing tools.
- Typed tool intents and operation routing.
- Text fallback for unavailable icon assets.

Tests:

- Tool compatibility and activation.
- Active-layer restrictions.
- Typed intent construction and staged mutation routing.
- Inactive-document behavior.

### M21.3 Tile Sources

Requires the Tiledata client/version and binary layout decision. Static editing
and default-Z policies must be specified before dependent tools are implemented.

Deliverables:

- Land and Static Tile Source slots.
- Source identity, namespace, numeric ID, metadata snapshot, preview reference,
  and unresolved state.
- Explicit metadata-view `Use as Tile Source` validation.

Tests:

- Compatible and incompatible source selection.
- Persisted unresolved selections.
- Missing profile source behavior.
- Independent Land and Static slot state.

## M22. Final integration cleanup

Depends on the completion of M0 through M20 and the domain decisions for M21.

Deliverables:

- Integration tests for actual Map and Radarcol central window identities.
- Frame-level activation and close tests.
- Provider/metadata-view integration tests.
- Output-set publication failure integration tests.
- Recent-file recording for every created asset document.
- Removal of obsolete panel/provider duplication.

Build hygiene:

- Split CMake registration into subsystem targets only if omissions become
  frequent.
- Keep architecture and extension contracts updated with every new subsystem.
