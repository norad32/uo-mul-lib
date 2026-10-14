# im-cpp

[![Build and test](https://github.com/norad32/im-cpp/actions/workflows/build.yml/badge.svg)](https://github.com/norad32/im-cpp/actions/workflows/build.yml)
[![Release](https://github.com/norad32/im-cpp/actions/workflows/release.yml/badge.svg)](https://github.com/norad32/im-cpp/actions/workflows/release.yml)

A Linux C++23 starter application with a command-line interface and optional Dear ImGui desktop GUI. The project currently requires GCC 14 or newer.

## Prerequisites

- CMake 3.25 or newer and Ninja
- GCC 14 or newer
- For tests: Catch2 (`BUILD_TESTS` is enabled by the debug and release presets)
- For GUI builds: GLFW 3 and OpenGL development libraries

The dependency helper supports apt- and pacman-based Linux distributions:

```sh
./scripts/install-build-dependencies.sh
```

On Ubuntu 24.04, configure with the versioned compiler installed by the helper:

```sh
cmake --preset release-headless -DCMAKE_CXX_COMPILER=g++-14
cmake --build --preset release-headless
ctest --preset release-headless
./build/release-headless/im-cpp check
```

Compiler selection is cached by CMake. Use a fresh build directory if you change compilers.

## Build configurations

| Preset | Purpose |
| --- | --- |
| `debug` / `debug-gui` | Debug GUI build with coverage and clang-tidy enabled by default |
| `debug-headless` | Debug build without GUI |
| `release-gui` | Release build with GUI support and a separate `im-cpp-gui` executable |
| `release-headless` | Release build without GUI |

Build and test any preset with `cmake --build --preset <preset>` and `ctest --preset <preset>`. Direct CMake configuration defaults to `BUILD_TESTS=OFF`, while presets enable tests. Other options include `BUILD_GUI`, `BUILD_GUI_RUNTIME`, `INSTALL_GUI`, `ENABLE_COVERAGE`, and `ENABLE_CLANG_TIDY`. Pass `-DENABLE_CLANG_TIDY=OFF` when configuring a debug preset if clang-tidy is unavailable.

## Dependencies

CMake uses installed spdlog 1.12.0 or newer and CLI11 2.4.2 when available, and otherwise fetches pinned revisions. Installed package consumers require spdlog 1.12.0 or newer. Dear ImGui is fetched for GUI builds. Catch2 is an installed dependency when tests are enabled. GUI builds also require system GLFW and OpenGL development packages.

## Run

The command-line executable supports `check` and `--version`. GUI-enabled builds can open the interface with `im-cpp gui` or by starting without a command. Release GUI builds also provide the standalone `im-cpp-gui` executable.

Preferences are stored below an absolute `$XDG_CONFIG_HOME/im-cpp/`, falling back to `$HOME/.config/im-cpp/` when `XDG_CONFIG_HOME` is unset or relative. Logs use `$XDG_STATE_HOME/im-cpp/logs/` when `XDG_STATE_HOME` is absolute. Otherwise they fall back to `$HOME/.local/state/im-cpp/logs/`.

## VS Code

The `.vscode` directory provides build, clean, test, and launch tasks. Install the recommended extensions, run **Install build dependencies**, then choose **Build** or a matching debug launch configuration.

## Install and package

Install locally with:

```sh
cmake --install build/release-headless --prefix <install-prefix>
```

The `Runtime` component installs the executable and the `Development` component installs headers, libraries, and CMake package files. CPack generates Debian component packages, while release automation also stages Linux archives and Arch packages. Review generated package dependencies and published release assets before relying on them for deployment. The Arch release workflow still needs a tagged-run verification.

CMake consumers can request the exported application target:

```cmake
find_package(im-cpp CONFIG REQUIRED COMPONENTS app)
target_link_libraries(my_target PRIVATE im-cpp::app)
```

The package-consumer fixture is exercised by CTest when tests are enabled.

## Project layout

- `.github/workflows/`: build, test, coverage, and release automation
- `.vscode/`: editor tasks and launch configurations
- `assets/`: desktop integration and icons
- `cmake/`: build configuration, dependency pins, install rules, and packaging
- `packaging/`: Debian and Arch package metadata templates
- `scripts/`: dependency installation and CMake formatting helpers
- `src/`: application, CLI, core, and GUI implementation
- `tests/`: unit tests and installed-package consumer fixture

## Credits

| Library | Purpose | License |
| --- | --- | --- |
| [Dear ImGui](https://github.com/ocornut/imgui) | Immediate-mode GUI | MIT |
| [GLFW](https://github.com/glfw/glfw) | Window and input handling | zlib/libpng |
| [CLI11](https://github.com/CLIUtils/CLI11) | Command-line parsing | BSD-3-Clause |
| [spdlog](https://github.com/gabime/spdlog) | Logging | MIT |
| [Catch2](https://github.com/catchorg/Catch2) | Testing | Boost Software License 1.0 |

## License

This project is licensed under the [MIT License](LICENSE). Third-party components retain their respective licenses.

## Author

[norad32](https://github.com/norad32)
