# OpenGW — Open Geometry Wars

OpenGW is an open-source, twin-stick arena shooter inspired by *Geometry Wars*.
Control a small neon ship on a vector-style grid, dodge swarms of enemies,
survive as long as you can and chase the high score.

This repository is a **SDL3** (previously SDL2) port of the original
[OpenGW project](https://sourceforge.net/projects/opengw/), modernised to C++20.

## Features

- Fast, glow-heavy vector rendering (grid, particles, enemies) with a blur
  post-process pass for the "Bloom" look
- Multiple enemy types: grunts, wanderers, weavers, snakes, spinners,
  repulsors, black holes / gravity wells, mayflies and more
- Weapons, bombs, shields, score multipliers and gravity wells to destroy
- 1–4 player support:
  - Single player
  - Multiplayer co-op
  - Multiplayer versus
- Keyboard **and** gamepad (hot-plug support for up to 4 pads)
- Attract mode and persistent high scores
- Cross-platform: macOS, Linux, Windows (MSVC) and AmigaOS 4

## Dependencies

- A C++20 compiler (`g++` / `clang++`)
- [SDL3](https://github.com/libsdl-org/SDL) development files
- OpenGL / GLU (legacy fixed-function pipeline is used)

## Building

The default `Makefile` auto-detects your OS and picks up SDL3 either from the
macOS framework install or via `pkg-config` (e.g. Homebrew / Linux distro
packages).

```sh
make            # build the game (produces ./opengw)
make run        # build and run
make clean      # remove build objects and the binary
make help       # show available targets
```

> **Run from the project root.** The game loads `sounds/` and `images/` and
> writes its `scores.sav` high-score file relative to the current working
> directory, so launch it from here (or from a folder that contains those
> folders).

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
sudo apt install libsdl3-dev libgl1-mesa-dev libglu1-mesa-dev
make

# Fedora
sudo dnf install SDL3-devel mesa-libGL-devel mesa-libGLU-devel
make
```

There is also a standalone `makefile.linux`.

### Windows

Use the Visual Studio solution `OpenGW.sln`, or the standalone
`makefile.win` (with SDL3 available on the include/library path).

### AmigaOS 4

Use `makefile.os4`.

## Controls

| Action        | Keyboard              | Gamepad               |
| ------------- | --------------------- | --------------------- |
| Move          | `W` `A` `S` `D`       | Left stick            |
| Aim / fire    | Arrow keys + `Space`  | Right stick / trigger |
| Start player  | `1` `2` `3` `4`       | Start                 |
| Back / cancel | `Backspace`           | Back                  |
| Pause         | `P`                   | Pause                 |

## Project layout

```
OpenGW.cpp        # main(): window, GL context, event loop
scene.cpp         # top-level game scene & rendering passes
game.cpp          # core game state, modes and sound setup
entity*.cpp/hpp   # player ships, enemies and projectiles
grid/particles    # background and particle effects
sounds/ images/   # runtime assets (loaded relative to CWD)
```

## License

GPL-3.0 — see [`gpl-3.0.txt`](gpl-3.0.txt).

Original game by the OpenGW project on SourceForge; SDL3/C++20 port maintained
separately on GitHub.
