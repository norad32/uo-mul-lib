# Project Roadmap

## Known issues

### Reopening an already open document

- Reopening an already-open document must activate its existing tab when
  documents are stacked in the center dockspace.
- Preserve document reuse without duplicate tabs or status-bar errors.

## v0.1.0 — Editing

- Tiledata editor: single-entry editing, CSV import, and CSV export.
- Ellipse selection and Stats tool for maps.
- Menu bar and Status Bar cleanup.
- Automated regression tests and manual acceptance testing.

## v0.2.0 — Metadata and static documents

- Art Document and metadata provider.
- Static file-set Document for `staidx.mul` and `statics*.mul`.
- File-set validation, inspection, and rendering.

## v0.3.0 — Unified Map and Statics editor

- Unified Map and Statics Document.
- Shared coordinates, facets, layers, selection, and editing.
- Static-aware rendering.
- Transactional Save/Discard and split/reopen workflows.

## v0.4.0 onward

Priority remains open. Candidates include Gumps, Hues, Cliloc, Multi files,
Sound files, and Animation files. Gumps are the recommended next format because
they can reuse the indexed-image and file-pair infrastructure from Art.
