#pragma once

#include <SDL3/SDL.h>

bool bgfx_bridge_init(SDL_Window* window, int width, int height, bool vsync);
void bgfx_bridge_shutdown();

void bgfx_bridge_resize(int width, int height, bool vsync);
void bgfx_bridge_begin_frame();
void bgfx_bridge_end_frame();

bool bgfx_bridge_active();
const char* bgfx_bridge_last_error();
