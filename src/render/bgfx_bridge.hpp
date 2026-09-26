#pragma once

// ---------------------------------------------------------------------------
// bgfx_bridge.hpp — bgfx host integration for Trigonometry Wars.
//
// When the game is built with `USE_BGFX=1` the whole render backend is replaced
// by src/render/bgfx_backend.cpp (bgfx, which means Metal on macOS) and this
// small bridge owns the bgfx device: it hands bgfx the native window/layer,
// keeps it in sync with window resizes and drives the frame loop.
//
// Passing a null window initialises bgfx *headless* (no swap chain), which is
// what the offscreen backend test in tools/ uses.
// ---------------------------------------------------------------------------

#include <SDL3/SDL.h>

// Bring bgfx up. Returns false — with bgfx_bridge_last_error() explaining why —
// when bgfx cannot provide the renderer this build needs.
bool bgfx_bridge_init(SDL_Window* window, int width, int height, bool vsync);
void bgfx_bridge_shutdown();

void bgfx_bridge_resize(int width, int height, bool vsync);
void bgfx_bridge_end_frame();

bool bgfx_bridge_active();
const char* bgfx_bridge_last_error();
