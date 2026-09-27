#!/bin/sh
#
# run_bgfx_frame_bench.sh — build and run the headless bgfx frame benchmark.
#
# See tools/bgfx_frame_bench.cpp. Renders a representative Endless-mode frame
# offscreen (no display needed) and reports where the frame time goes, so
# render-path changes can be measured instead of guessed at.
#
# Usage: tools/run_bgfx_frame_bench.sh [bench args...]
#   e.g. tools/run_bgfx_frame_bench.sh --gridres=133x89
#        tools/run_bgfx_frame_bench.sh --no-grid --no-glow
#
# Requires a bgfx checkout (BGFX_HOME, default ~/projects/bgfx) with the static
# libraries built and the embedded shaders generated (tools/compile_shaders.sh).

set -e

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
bgfx=${BGFX_HOME:-$HOME/projects/bgfx}

bindir=
for d in "$bgfx/.build/osx-arm64/bin" "$bgfx/.build/osx-x64/bin" "$bgfx/.build/linux64/bin" \
         "$bgfx/.build/x64/bin" "$bgfx/.build/bin"; do
    if [ -d "$d" ]; then
        bindir=$d
        break
    fi
done

if [ -z "$bindir" ]; then
    echo "run_bgfx_frame_bench.sh: no built bgfx libraries under $bgfx/.build" >&2
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

out=${TMPDIR:-/tmp}/bgfx_frame_bench

# Skip recompiling when the binary is newer than every source it is built from
# (repeated A/B runs on the same sources are the common case here).
rebuild=1
if [ -x "$out" ] && [ -z "$(find "$root/src/render" "$root/tools/bgfx_frame_bench.cpp" -newer "$out" -print -quit 2>/dev/null)" ]; then
    rebuild=0
fi

if [ "$rebuild" = "1" ]; then
    echo "compiling $out"
    c++ -std=c++20 -Wall -Wextra -O2 -ggdb \
        -Isrc -DUSE_BGFX_RENDERER \
        $(pkg-config --cflags sdl3) \
        -I"$bgfx/include" -I"$bgfx/../bx/include" -I"$bgfx/../bimg/include" \
        -o "$out" \
        src/render/bgfx_backend.cpp src/render/bgfx_bridge.cpp tools/bgfx_frame_bench.cpp \
        $libs $(pkg-config --libs sdl3) \
        -framework Cocoa -framework IOKit -framework OpenGL -framework QuartzCore \
        -weak_framework Metal -weak_framework MetalKit -weak_framework VideoToolbox \
        -weak_framework CoreMedia -weak_framework CoreVideo
fi

exec "$out" "$@"
