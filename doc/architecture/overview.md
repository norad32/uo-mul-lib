# Project Architecture Overview

This page is the architecture entry point. The [architecture layers](layers.md)
document defines component boundaries, dependencies, and ownership.

`uo-mule` combines the standalone `uomul` library, the CLI application, and an
optional desktop GUI. The GUI uses a headless model and a separate presentation
runtime.

## Executables

The normal `uo-mule` executable composes `app` and, when enabled, `gui`. A
separately packaged GUI runtime can also be built with `BUILD_GUI_RUNTIME`.
Entry points remain thin. Commands and GUI behavior belong to their owning
components.

For detailed GUI ownership, see [GUI architecture](gui.md). For file and
document transitions, see [File and Document Lifecycle](lifecycle.md).
For feature placement, see the [extension guide](../development/extension-guide.md).
