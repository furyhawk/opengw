#pragma once

// ---------------------------------------------------------------------------
// bgfx_backend_api.hpp — bgfx-specific hooks of the bgfx render backend.
//
// Only used by tooling (tools/bgfx_backend_test.cpp); the game itself talks to
// the backend exclusively through the shared gl3.h API.
// ---------------------------------------------------------------------------

#if defined(USE_BGFX_RENDERER)

#include <bgfx/bgfx.h>

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// Render the "back buffer" draws into an offscreen framebuffer instead of the
// bgfx swap chain. Used for headless (no display) verification: the tool
// renders a frame, reads the target back and asserts on the pixels.
// Pass BGFX_INVALID_HANDLE to go back to the swap chain.
void gfx_bgfx_set_backbuffer(bgfx::FrameBufferHandle fb, uint16_t width, uint16_t height);

#ifdef __cplusplus
}
#endif

#endif // USE_BGFX_RENDERER
