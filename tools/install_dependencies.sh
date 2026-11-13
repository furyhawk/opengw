#!/usr/bin/env bash

set -euo pipefail

SDL3_VERSION="${SDL3_VERSION:-3.4.16}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

log() {
    printf "[deps] %s\n" "$*"
}

die() {
    printf "[deps] error: %s\n" "$*" >&2
    exit 1
}

run_privileged() {
    if [[ "$(id -u)" -eq 0 ]]; then
        "$@"
    elif command -v sudo >/dev/null 2>&1; then
        sudo "$@"
    else
        die "this step needs root privileges; re-run as root or install sudo"
    fi
}

has_sdl3_pkgconfig() {
    command -v pkg-config >/dev/null 2>&1 && pkg-config --exists sdl3
}

install_sdl3_from_source_linux() {
    local workdir srcdir builddir
    workdir="$(mktemp -d)"
    srcdir="${workdir}/SDL"
    builddir="${workdir}/build"

    log "SDL3 dev package not found; building SDL ${SDL3_VERSION} from source"
    git clone --depth 1 --branch "release-${SDL3_VERSION}" https://github.com/libsdl-org/SDL.git "${srcdir}"
    cmake -S "${srcdir}" -B "${builddir}" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DSDL_TESTS=OFF \
        -DSDL_EXAMPLES=OFF
    cmake --build "${builddir}" --parallel
    run_privileged cmake --install "${builddir}"

    if [[ -f /usr/local/lib/pkgconfig/sdl3.pc ]]; then
        if [[ -f /etc/profile ]]; then
            if ! grep -q '/usr/local/lib/pkgconfig' /etc/profile; then
                log "adding /usr/local/lib/pkgconfig to /etc/profile"
                run_privileged sh -c 'printf "\n# Added by Trigonometry Wars dependency installer\nexport PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:\${PKG_CONFIG_PATH}\n" >> /etc/profile'
            fi
        fi
        export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
    fi

    rm -rf "${workdir}"
}

install_linux() {
    if command -v apt-get >/dev/null 2>&1; then
        log "detected Debian/Ubuntu (apt)"
        run_privileged apt-get update
        run_privileged apt-get install -y \
            build-essential g++ pkgconf git cmake ninja-build \
            libasound2-dev libdbus-1-dev libudev-dev \
            libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev \
            libxi-dev libxss-dev libxxf86vm-dev libxtst-dev libxrender-dev libxfixes-dev \
            libwayland-dev libxkbcommon-dev libdrm-dev \
            libgl1-mesa-dev libglu1-mesa-dev libegl1-mesa-dev libvulkan-dev
        if ! has_sdl3_pkgconfig; then
            install_sdl3_from_source_linux
        fi
    elif command -v dnf >/dev/null 2>&1; then
        log "detected Fedora/RHEL (dnf)"
        run_privileged dnf install -y \
            gcc-c++ make pkgconf-pkg-config git cmake ninja-build \
            SDL3-devel mesa-libGL-devel mesa-libGLU-devel mesa-libEGL-devel
    elif command -v pacman >/dev/null 2>&1; then
        log "detected Arch Linux (pacman)"
        run_privileged pacman -Sy --needed --noconfirm \
            base-devel pkgconf git cmake ninja sdl3 mesa glu
    else
        die "unsupported Linux package manager; install SDL3, OpenGL dev files, and a C++20 compiler manually"
    fi
}

install_macos() {
    command -v brew >/dev/null 2>&1 || die "Homebrew is required on macOS (https://brew.sh)"
    log "installing dependencies with Homebrew"
    brew install sdl3 pkg-config
}

main() {
    local os
    os="$(uname -s)"
    log "repository: ${REPO_ROOT}"

    case "${os}" in
        Linux)
            install_linux
            ;;
        Darwin)
            install_macos
            ;;
        *)
            die "unsupported OS: ${os}"
            ;;
    esac

    if has_sdl3_pkgconfig; then
        log "SDL3 detected: $(pkg-config --modversion sdl3)"
    else
        log "SDL3 pkg-config entry not found in current shell; if SDL3 was just installed to /usr/local, restart your shell"
    fi

    log "dependency installation complete"
    log "next step: run 'make' or 'make run' in ${REPO_ROOT}"
}

main "$@"
