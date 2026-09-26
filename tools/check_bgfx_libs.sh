#!/bin/sh
#
# check_bgfx_libs.sh — sanity-check the bgfx link flags before linking.
#
# bgfx links statically against bimg (image parsing, format queries) and bx.
# When one of those archives is missing from the link line the build dies with a
# wall of "undefined symbols: bimg::..." messages that say nothing about the
# real cause (a partial library list), so the flags are inspected here first.
#
# The library *names* are checked exactly — that is what actually breaks the
# link.  File existence is only reported as a note: on sandboxed/agent setups
# stat-ing a path outside the project can be denied even though the file is
# there, and this must never fail a build for that reason.
#
# Usage: tools/check_bgfx_libs.sh <link flags...>
# Diagnostics go to stderr and the exit status is always 0.

if [ "$#" -eq 0 ]; then
    echo "check_bgfx_libs.sh: no link flags given" >&2
    exit 0
fi

have_bgfx=""
have_bimg=""
have_bx=""
unverifiable=""

for arg in "$@"; do
    case "$arg" in
        *.a|*.dylib)
            case "$arg" in
                *libbgfx*) have_bgfx="$arg" ;;
                *libbimg*) have_bimg="$arg" ;;
                *libbx*)   have_bx="$arg" ;;
            esac
            if [ ! -f "$arg" ]; then
                unverifiable="$unverifiable $arg"
            fi
            ;;
    esac
done

if [ -n "$unverifiable" ]; then
    echo "bgfx: note: these link libraries could not be read:" >&2
    for f in $unverifiable; do
        echo "      $f" >&2
    done
    echo "      If they are genuinely missing, point BGFX_HOME at your bgfx" >&2
    echo "      checkout (or set BGFX_LIBDIR / BGFX_LIBS); 'make bgfx-env'" >&2
    echo "      prints what is being used." >&2
fi

# A static bgfx archive needs its companions; the names alone tell us that.
case "$have_bgfx" in
    *.a)
        if [ -z "$have_bimg" ]; then
            echo "bgfx: a static libbgfx is linked without a bimg archive, but" >&2
            echo "      bgfx references bimg (imageParse, imageGetSize, ...)." >&2
            echo "      Add libbimg to BGFX_LIBS -- unless you link a *shared*" >&2
            echo "      bgfx library, which bundles bimg and bx." >&2
        fi
        if [ -z "$have_bx" ]; then
            echo "bgfx: a static libbgfx is linked without the bx archive; add libbx." >&2
        fi
        ;;
esac

exit 0
