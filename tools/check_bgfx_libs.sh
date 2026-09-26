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
# link.  A static libbgfx named without its bimg/bx companions is a link that
# cannot possibly succeed, so that case is a hard error (exit 1) with an
# explanation instead of a page of undefined symbols.  File existence, on the
# other hand, is only reported as a note: on sandboxed/agent setups stat-ing a
# path outside the project can be denied even though the file is there, and
# this must never fail a build for that reason.
#
# Usage: tools/check_bgfx_libs.sh <link flags...>
# Diagnostics go to stderr.

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
status=0
case "$have_bgfx" in
    *.a)
        if [ -z "$have_bimg" ] || [ -z "$have_bx" ]; then
            missing=""
            [ -z "$have_bimg" ] && missing="$missing libbimg"
            [ -z "$have_bx" ] && missing="$missing libbx"
            echo "bgfx: the link line has a static libbgfx but no$missing next to it," >&2
            echo "      so bgfx's references into bimg/bx cannot be resolved." >&2
            echo "      A static libbgfx always needs all three archives; only a" >&2
            echo "      *shared* bgfx bundles bimg and bx." >&2
            echo "      Put the missing archives in BGFX_LIBDIR, or pass a full" >&2
            echo "      list in BGFX_LIBS ('make bgfx-env' shows what is used)." >&2
            status=1
        fi
        ;;
esac

exit $status
