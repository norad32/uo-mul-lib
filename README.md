# uo-mule

[![Build and test](https://github.com/norad32/uo-mul-lib/actions/workflows/build.yml/badge.svg)](https://github.com/norad32/uo-mul-lib/actions/workflows/build.yml)
[![Release](https://github.com/norad32/uo-mul-lib/actions/workflows/release.yml/badge.svg)](https://github.com/norad32/uo-mul-lib/actions/workflows/release.yml)
[![License](https://img.shields.io/github/license/norad32/uo-mul-lib)](LICENSE)

`uo-mule` is a Linux C++23 toolkit for Ultima Online MUL data. It includes:

- `uomul`, a standalone format library for map, radarcol, texmap, and tiledata files;
- a CLI for map inspection, editing, and diff operations; and
- an optional Dear ImGui desktop GUI for browsing and working with the data.

## Build

Requirements: GCC 14+, CMake 3.25+, Ninja, and Catch2 for tests. GUI builds
also need GLFW 3 and OpenGL development packages.

```sh
./scripts/install-build-dependencies.sh
cmake --preset release-headless -DCMAKE_CXX_COMPILER=g++-14
cmake --build --preset release-headless
ctest --preset release-headless
```

For the desktop GUI, use the `release-gui` preset instead. Other useful presets
include `debug-headless`, `debug-cli`, `debug-gui`, and `debug-uomul`.

## Use

```sh
./build/release-headless/uo-mule --help
./build/release-headless/uo-mule check
./build/release-headless/uo-mule map --help
```

Install the library and CMake package with `cmake --install`. Consumers can use
the exported `uo-mule::uomul` target and, when enabled, `uo-mule::app`; CPack
and the release workflow produce Debian, Linux archive, and Arch package
artifacts.

## Credits

| Library | Purpose | License |
| --- | --- | --- |
| [Dear ImGui](https://github.com/ocornut/imgui) | Immediate-mode GUI | MIT |
| [GLFW](https://github.com/glfw/glfw) | Window and input handling | zlib/libpng |
| [CLI11](https://github.com/CLIUtils/CLI11) | Command-line parsing | BSD-3-Clause |
| [spdlog](https://github.com/gabime/spdlog) | Logging | MIT |
| [Catch2](https://github.com/catchorg/Catch2) | Testing | Boost Software License 1.0 |
| [Material Design Icons](https://github.com/Templarian/MaterialDesign-Webfont) | GUI icon font | Apache-2.0 |

## License

This project is licensed under the [MIT License](LICENSE). Third-party components retain their respective licenses.

## Author

[norad32](https://github.com/norad32)
