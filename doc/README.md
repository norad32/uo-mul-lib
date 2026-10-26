# Project Documentation

`uo-mule` is a Linux C++23 toolkit for inspecting and editing Ultima Online
MUL data. This is the documentation index for the whole project.

## Start here

- [Architecture](architecture/overview.md): understand the libraries,
  applications, GUI layers, and ownership boundaries.
- [Glossary](glossary.md): use the project's standard vocabulary.
- [Extension guide](development/extension-guide.md): add formats, commands,
  documents, panels, dialogs, workflows, and tools.
- [Design guide](development/design-guide.md): GUI presentation and interaction
  conventions.
- [Development quality](development/quality.md): formatting, lint, test naming,
  and verification expectations.

## Project reference

- [Roadmap](project/roadmap.md): ordered implementation milestones.

## Documentation authority

- `architecture/` defines the intended ownership and dependency rules. The
  roadmap identifies migrations where the current implementation has not yet
  reached those boundaries.
- `development/` describes how to extend and present the application.
- `project/` records roadmap, rationale, and maintainer context. It does not
  override current implementation contracts.

If a document disagrees with the implementation, tests, or a newer authoritative
document, fix the documentation or the implementation together. Clearly label
future work and historical context so neither is mistaken for current support.
