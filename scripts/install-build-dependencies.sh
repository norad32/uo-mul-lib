#!/usr/bin/env bash
set -euo pipefail

if (( EUID == 0 )); then
    SUDO=()
elif command -v sudo >/dev/null 2>&1; then
    SUDO=(sudo)
else
    printf '%s\n' "This installer needs root privileges. Install sudo or run it as root." >&2
    exit 1
fi

install_apt() {
    local -a packages=()
    local package
    local -a required_packages=(
        build-essential
        catch2
        g++-14
        cmake
        ninja-build
        clang-tidy
        clang-format
        gcovr
        libglfw3-dev
        libgl1-mesa-dev
        libspdlog-dev
        pkg-config
        python3-pip
        pipx
    )

    for package in "${required_packages[@]}"; do
        if [[ "$(dpkg-query -W -f='${db:Status-Status}' "$package" 2>/dev/null || true)" != "installed" ]]; then
            packages+=("$package")
        fi
    done

    if ((${#packages[@]})); then
        "${SUDO[@]}" apt-get update
        "${SUDO[@]}" apt-get install -y "${packages[@]}"
    fi

    if ! command -v cmake-format >/dev/null 2>&1 && apt-cache show cmake-format >/dev/null 2>&1; then
        "${SUDO[@]}" apt-get install -y cmake-format
    fi
}

install_pacman() {
    local -a packages=()
    local package
    local -a required_packages=(
        base-devel
        catch2
        cmake
        git
        ninja
        gcc
        clang-tools-extra
        gcovr
        glfw-x11
        mesa
        spdlog
        pkgconf
        python-pipx
    )

    for package in "${required_packages[@]}"; do
        if ! pacman -Q "$package" >/dev/null 2>&1; then
            packages+=("$package")
        fi
    done

    if ((${#packages[@]})); then
        "${SUDO[@]}" pacman -S --needed --noconfirm "${packages[@]}"
    fi
}

if command -v apt-get >/dev/null 2>&1; then
    install_apt
elif command -v pacman >/dev/null 2>&1; then
    install_pacman
else
    printf '%s\n' "Unsupported package manager. This script supports apt and pacman." >&2
    exit 1
fi

CMAKE_FORMAT_BIN=""
PIPX_BIN_DIR=""
if ! command -v cmake-format >/dev/null 2>&1; then
    if ! command -v pipx >/dev/null 2>&1; then
        printf '%s\n' "pipx is required to install cmake-format." >&2
        exit 1
    fi
    if ! pipx list --short 2>/dev/null | awk '$1 == "cmakelang" { found = 1 } END { exit !found }'; then
        pipx install cmakelang
    fi
    if ! pipx runpip cmakelang show pyyaml >/dev/null 2>&1; then
        pipx inject cmakelang pyyaml
    fi

    PIPX_BIN_DIR="$(pipx environment --value PIPX_BIN_DIR)"

    if [[ -x "${PIPX_BIN_DIR}/cmake-format" ]]; then
        CMAKE_FORMAT_BIN="${PIPX_BIN_DIR}/cmake-format"
        if [[ ":${PATH}:" != *":${PIPX_BIN_DIR}:"* ]]; then
            pipx ensurepath
            export PATH="${PIPX_BIN_DIR}:${PATH}"
        fi
    fi
fi

for tool in cmake ninja; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        printf 'Dependency installation completed, but %s is still not on PATH.\n' "$tool" >&2
        exit 1
    fi
done
if ! command -v g++-14 >/dev/null 2>&1 && ! command -v g++ >/dev/null 2>&1; then
    printf '%s\n' "Dependency installation completed, but no GCC compiler is on PATH." >&2
    exit 1
fi

for tool in g++ cmake ninja clang-tidy clang-format cmake-format gcovr; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        printf 'Dependency installation completed, but %s is still not on PATH.\n' "$tool" >&2
        exit 1
    fi
done

printf '%s\n' "Build and formatting tools are installed."
