# Trigonometry Wars

Trigonometry Wars is an open-source, twin-stick arena shooter inspired by
*Geometry Wars*. Control a small neon ship on a vector-style grid, dodge swarms
of enemies, survive as long as you can and chase the high score.

This repository began as an **SDL3** (previously SDL2) port of the original
[OpenGW project](https://sourceforge.net/projects/opengw/), modernised to
C++20, and is now developed as **Trigonometry Wars**.

The renderer has been modernised to **OpenGL 3.3 core profile**: the legacy
fixed-function pipeline (`glBegin`/`glEnd`, GLU, CPU blur read-back) has been
replaced by a small shader/VAO/VBO backend (`src/render/gl3.{h,cpp}`) and the
"bloom" glow is produced with a GPU framebuffer-object Gaussian blur — no
more per-frame CPU image read-back.

An alternative **bgfx render backend** (`src/render/bgfx_backend.cpp`) can be
selected with `USE_BGFX=1`. It implements exactly the same `gfx_*` API (see
`src/render/gl3.h`), so no game code changes: vertex transform, wide lines and
points, batching, textures and the glow/blur pass are simply driven through
bgfx. On macOS bgfx has no OpenGL backend any more (upstream removed it), so
that build renders with **Metal** — `make run-bgfx` gives you the Metal path
with the game's existing look. `src/render/bgfx_bridge.{hpp,cpp}` owns the bgfx
device (window/layer, resize, vsync, frame submission).

## Features

- Fast, glow-heavy vector rendering (grid, particles, enemies) with a GPU
  blur post-process pass for the "Bloom" look
- Multiple enemy types: grunts, wanderers, weavers, snakes, spinners,
  repulsors, black holes / gravity wells, mayflies and more
- Dynamic wave variety: anti-repeat wave selection, mixed support waves and
  varied swarm entry patterns to reduce repetitive encounters
- Weapons, bombs, shields, score multipliers and gravity wells to destroy
- 1–4 player support:
  - Single player
  - Multiplayer co-op
  - Multiplayer versus
- Keyboard **and** gamepad (hot-plug support for up to 4 pads)
- Attract mode and persistent high scores
- **Top Scores screen** (from the title menu) and a **Graphics options
  screen** (press `Backspace`/`O` or the gamepad **Back** button on the title
  screen): fullscreen, window resolution, glow/bloom, per-system smoothing and
  vsync — saved to `settings.cfg`
- Cross-platform: macOS, Linux, Windows (MSVC) and AmigaOS 4

## Dependencies

- A C++20 compiler (`g++` / `clang++`)
- [SDL3](https://github.com/libsdl-org/SDL) development files, validated with
  SDL3 3.4.16+
- OpenGL 3.3 **core profile** support (functions are loaded at runtime via
  `SDL_GL_GetProcAddress`, so no GLEW/GLAD and no GLU is required)
- Optional (macOS, for `USE_BGFX=1` builds):
  [bgfx](https://github.com/bkaradzic/bgfx) built with Metal support, plus its
  `shaderc` tool if you want to change the shaders

## Building

The default `Makefile` auto-detects your OS and picks up SDL3 either from the
macOS framework install or via `pkg-config` (e.g. Homebrew / Linux distro
packages).

This repository is currently validated against SDL3 3.4.16.

```sh
make            # build the game (produces ./trigwars)
make run        # build and run
make clean      # remove build objects and the binary
make help       # show available targets

# Build with the bgfx interop host path enabled
make USE_BGFX=1
make run-bgfx   # build and run with USE_BGFX=1 (produces ./trigwars-bgfx)
```

For `USE_BGFX=1` the bgfx paths are derived from `BGFX_HOME` (a local bgfx
checkout, default `~/projects/bgfx`); `BGFX_LIBDIR`, `BGFX_CFLAGS` and
`BGFX_LIBS` can be overridden individually. `make bgfx-env` prints what is being
used:

```sh
make USE_BGFX=1                       # ~/projects/bgfx by default
make USE_BGFX=1 BGFX_HOME="$HOME/src/bgfx"
make USE_BGFX=1 BGFX_LIBS="..."       # e.g. a shared bgfx library
make USE_BGFX=1 bgfx-env              # show the detected paths and flags
```

bgfx is linked statically, so all three of `libbgfx`, `libbimg` and `libbx` are
on the link line (`bgfx` calls into `bimg` for image handling and into `bx`);
`tools/check_bgfx_libs.sh` inspects the flags before linking and explains the
usual mistake — a partial list, which otherwise only shows up as a wall of
`undefined symbols: bimg::...`. A `BGFX_LIBS` that names a static `libbgfx` but
drops one of its companions (a stale `export` in your shell, say) is completed
from `BGFX_LIBDIR` with a note from make; a complete list, or one naming a
*shared* bgfx (which bundles bimg and bx), is used exactly as given. Build the
libraries once with `make -C "$BGFX_HOME"` (add `shaderc` if you want to
regenerate the shaders).

`USE_BGFX=1` builds use their own object directory (`obj-bgfx/`) and their own
binary (`trigwars-bgfx`, so neither build can be mistaken for — or silently
reused as — the other), and are currently **macOS-only**: the backend renders
with bgfx/Metal and the embedded shaders are compiled for Metal (see
`tools/compile_shaders.sh`). On other platforms use the default `make` (OpenGL
backend).

> **Run from the project root.** The game loads `assets/sounds/` and
> `assets/images/` and writes its `scores.sav` high-score file relative to the
> current working directory, so launch it from here (or from a folder that
> contains those folders).

> **macOS `run-bgfx` note.** The bgfx build renders with **Metal** (bgfx has no
> OpenGL backend on Apple platforms) and keeps the game's existing look: the
> same CPU-side fixed-function emulation, the same glow/blur pass, just driven
> through bgfx. Close other GPU-heavy apps if you see frame pacing oddities —
> vsync is applied through the bgfx swap chain.

### macOS

Either install SDL3 with Homebrew:

```sh
brew install sdl3
make
```

…or install the official `SDL3.framework` into `/Library/Frameworks/` and
build with the framework-based makefile:

```sh
make -f makefile.macos
```

### Linux

```sh
# Debian / Ubuntu
sudo apt install libsdl3-dev libgl1-mesa-dev
make

# Fedora
sudo dnf install SDL3-devel mesa-libGL-devel
make
```

There is also a standalone `makefile.linux`.

### Windows

Use the standalone `makefile.win` with MinGW (with SDL3 available on the
include/library path).

### AmigaOS 4

Use `makefile.os4`.

## Controls

| Action        | Keyboard              | Gamepad               |
| ------------- | --------------------- | --------------------- |
| Move          | `W` `A` `S` `D`       | Left stick / D-pad    |
| Aim / fire    | Arrow keys + `Space`  | Right stick / trigger |
| Start player  | `1` `2` `3` `4`       | Start                 |
| Back / cancel | `Backspace`           | Back                  |
| Pause         | `P`                   | Pause                 |
| Options       | `O` / `Backspace` on the title screen | Back on the title screen |

Menus can be navigated with the analog stick **or** the D-pad (the D-pad also
drives the ship in-game).

## Project layout

Sources are grouped under `src/` and runtime assets under `assets/`:

```
Makefile, makefile.*   # build entry points (`make` from the repo root)
src/
  core/      main loop, game state, camera, input, settings, high scores
  entities/  entity base, player ships, enemies and projectiles
  render/    scene, grid/stars background, GPU glow, textures, fonts,
             gl3.{h,cpp}          (modern OpenGL 3.3 core backend, default)
             bgfx_backend.cpp     (bgfx backend, Metal on macOS — USE_BGFX=1)
             bgfx_bridge.{hpp,cpp}(bgfx device / window / frame submission)
             shaders/             bgfx shader sources + embedded Metal binaries
  audio/     SDL3 sound mixer
  ui/        menus (game-type select)
  math/      vectors/matrices, math helpers, shared constants
  vendor/    lodepng (third-party PNG codec)
assets/
  images/    runtime images (marquee, icon)
  sounds/    runtime sounds (.wav)
tools/
  classicalcfg_check.cpp      standalone classical.cfg loader check
  compile_shaders.sh          regenerate the embedded bgfx shaders (needs shaderc)
  check_bgfx_libs.sh          validate the bgfx link flags before linking
  bgfx_backend_test.cpp       headless bgfx backend verification
  run_bgfx_backend_test.sh    build + run the above
```

Run `make` from the repo root so the CWD-relative `assets/…` and
`scores.sav` paths resolve.

## Renderer backends

Both backends implement the same API (`src/render/gl3.h`), so all game code is
shared:

| | default (`make`) | `USE_BGFX=1` (`make run-bgfx`) |
| --- | --- | --- |
| API | OpenGL 3.3 core | bgfx (Metal on macOS) |
| Context | SDL GL context | SDL Metal view / bgfx swap chain |
| Shaders | GLSL strings compiled at runtime | pre-compiled bgfx blobs (`src/render/shaders/*.bin.h`) |
| Objects | `obj/` | `obj-bgfx/` |

### Verifying the bgfx backend without a display

`tools/run_bgfx_backend_test.sh` renders into an offscreen Metal framebuffer,
reads the pixels back and checks the things that are easy to get silently wrong
in a port: clip-space orientation, the NDC depth convention (Metal uses
`[0,1]`), blend modes, wide lines/points, client arrays (the grid), texture
orientation and glow/blur alignment.

```sh
tools/run_bgfx_backend_test.sh          # build + run, exits non-zero on failure
tools/compile_shaders.sh                # regenerate src/render/shaders/*.bin.h
```

`compile_shaders.sh` needs the `shaderc` tool from a bgfx checkout (run
`make shaderc` there once); the generated headers are committed, so normal
builds do not need it.

## License

GPL-3.0 — see [`gpl-3.0.txt`](gpl-3.0.txt).

Original game by the OpenGW project on SourceForge; this SDL3/C++20 port of it
is maintained here as Trigonometry Wars.
