#!/bin/sh
#
# run_bgfx_backend_test.sh — build and run the headless bgfx backend test.
#
# See tools/bgfx_backend_test.cpp for what is verified. Requires a bgfx
# checkout (BGFX_HOME, default ~/projects/bgfx) with the static libraries built
# and the embedded shaders generated (tools/compile_shaders.sh).
#
# Usage: tools/run_bgfx_backend_test.sh [--no-run]

set -e

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
bgfx=${BGFX_HOME:-$HOME/projects/bgfx}

# Locate the built bgfx libraries.
bindir=
for d in "$bgfx/.build/osx-arm64/bin" "$bgfx/.build/osx-x64/bin" "$bgfx/.build/linux64/bin" \
         "$bgfx/.build/x64/bin" "$bgfx/.build/bin"; do
    if [ -d "$d" ]; then
        bindir=$d
        break
    fi
done

if [ -z "$bindir" ]; then
    echo "run_bgfx_backend_test.sh: no built bgfx libraries under $bgfx/.build" >&2
    exit 1
fi

libs=""
for lib in bgfx bimg bx; do
    for f in "$bindir/lib${lib}Release.a" "$bindir/lib${lib}.a"; do
        if [ -f "$f" ]; then
            libs="$libs $f"
            break
        fi
    done
done

out=${TMPDIR:-/tmp}/bgfx_backend_test
echo "compiling $out"

c++ -std=c++20 -Wall -Wextra -O1 -ggdb \
    -Isrc -DUSE_BGFX_RENDERER \
    $(pkg-config --cflags sdl3) \
    -I"$bgfx/include" -I"$bgfx/../bx/include" -I"$bgfx/../bimg/include" \
    -o "$out" \
    src/render/bgfx_backend.cpp src/render/bgfx_bridge.cpp tools/bgfx_backend_test.cpp \
    $libs $(pkg-config --libs sdl3) \
    -framework Cocoa -framework IOKit -framework OpenGL -framework QuartzCore \
    -weak_framework Metal -weak_framework MetalKit -weak_framework VideoToolbox \
    -weak_framework CoreMedia -weak_framework CoreVideo

if [ "$1" != "--no-run" ]; then
    "$out"
fi
