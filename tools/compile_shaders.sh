#!/bin/sh
#
# compile_shaders.sh — (re)generate the embedded bgfx shaders.
#
# The game's reworked renderer runs on bgfx when built with `USE_BGFX=1`.
# bgfx needs *pre-compiled* shader binaries; this script runs the `shaderc`
# tool from a bgfx checkout and writes the byte arrays as C headers next to
# the shader sources, where the backend includes them.
#
# You only need to run this when a shader source (*.sc) changes.
#
# Usage:
#   tools/compile_shaders.sh [shaderc-path]
#
# shaderc is found (in order) from:
#   * the argument,
#   * $SHADERC,
#   * $BGFX_HOME/.build/<platform>/bin/shadercRelease,
#   * ~/projects/bgfx/.build/osx-arm64/bin/shadercRelease.

set -e

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)

shaderc=${1:-${SHADERC:-}}
bgfx=${BGFX_HOME:-$HOME/projects/bgfx}

if [ -z "$shaderc" ]; then
    for candidate in \
        "$bgfx/.build/osx-arm64/bin/shadercRelease" \
        "$bgfx/.build/osx-x64/bin/shadercRelease" \
        "$bgfx/.build/linux64/bin/shadercRelease" \
        "$bgfx/.build/x64/bin/shadercRelease" \
        "$bgfx/.build/bin/shadercRelease" \
        "$(command -v shaderc 2>/dev/null || true)"
    do
        if [ -n "$candidate" ] && [ -x "$candidate" ]; then
            shaderc=$candidate
            break
        fi
    done
fi

if [ -z "$shaderc" ] || [ ! -x "$shaderc" ]; then
    echo "compile_shaders.sh: shaderc not found." >&2
    echo "  Build it with:  make -C \"$bgfx\" shaderc" >&2
    echo "  or pass the path: tools/compile_shaders.sh /path/to/shadercRelease" >&2
    exit 1
fi

if [ ! -f "$bgfx/src/bgfx_shader.sh" ]; then
    echo "compile_shaders.sh: bgfx sources not found at $bgfx" >&2
    echo "  Set BGFX_HOME to a bgfx checkout." >&2
    exit 1
fi

out=$root/src/render/shaders
cd "$out"

for vs in vs_solid vs_tex vs_blur; do
    echo "  vertex   $vs"
    "$shaderc" -f "$vs.sc" -o "$vs.bin.h" \
        --type vertex --platform osx -p metal \
        -i "$bgfx/src" --varyingdef varying.def.sc --bin2c "$vs"
done

for fs in fs_solid fs_tex fs_blur; do
    echo "  fragment $fs"
    "$shaderc" -f "$fs.sc" -o "$fs.bin.h" \
        --type fragment --platform osx -p metal \
        -i "$bgfx/src" --varyingdef varying.def.sc --bin2c "$fs"
done

echo "compile_shaders.sh: wrote $(ls *.bin.h | wc -l | tr -d ' ') headers in $out"
