#include "render/bgfx_bridge.hpp"

#include <cstdint>
#include <string>

#if defined(USE_BGFX_RENDERER)
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#endif

namespace {

bool sActive = false;
std::string sLastError;

void setLastError(const char* msg)
{
    sLastError = msg ? msg : "unknown bgfx error";
}

} // namespace

bool bgfx_bridge_init(SDL_Window* window, int width, int height, bool vsync)
{
    sActive = false;
    sLastError.clear();

#if !defined(USE_BGFX_RENDERER)
    (void)window;
    (void)width;
    (void)height;
    (void)vsync;
    setLastError("bgfx support was not compiled in (build with USE_BGFX=1)");
    return false;
#else
    if (window == nullptr) {
        setLastError("window was null");
        return false;
    }

    bgfx::PlatformData pd {};
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    if (!props) {
        setLastError("SDL_GetWindowProperties failed");
        return false;
    }

#if defined(__APPLE__)
    pd.nwh = SDL_GetPointerProperty(props, "SDL.window.cocoa.window", nullptr);
#elif defined(_WIN32)
    pd.nwh = SDL_GetPointerProperty(props, "SDL.window.win32.hwnd", nullptr);
#elif defined(__linux__)
    pd.nwh = SDL_GetPointerProperty(props, "SDL.window.wayland.surface", nullptr);
    pd.ndt = SDL_GetPointerProperty(props, "SDL.window.wayland.display", nullptr);
    if (pd.nwh == nullptr) {
        const Sint64 x11Window = SDL_GetNumberProperty(props, "SDL.window.x11.window", 0);
        pd.nwh = reinterpret_cast<void*>(static_cast<uintptr_t>(x11Window));
        pd.ndt = SDL_GetPointerProperty(props, "SDL.window.x11.display", nullptr);
    }
#endif

    if (pd.nwh == nullptr) {
        setLastError("could not resolve native window handle from SDL");
        return false;
    }

    pd.context = SDL_GL_GetCurrentContext();

    bgfx::renderFrame();

    bgfx::Init init {};
    init.type = bgfx::RendererType::OpenGL;
    init.platformData = pd;
    init.resolution.width = static_cast<uint32_t>(width);
    init.resolution.height = static_cast<uint32_t>(height);
    init.resolution.reset = vsync ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;

    if (!bgfx::init(init)) {
        setLastError("bgfx::init failed");
        return false;
    }

    sActive = true;
    return true;
#endif
}

void bgfx_bridge_shutdown()
{
#if defined(USE_BGFX_RENDERER)
    if (sActive)
        bgfx::shutdown();
#endif
    sActive = false;
}

void bgfx_bridge_resize(int width, int height, bool vsync)
{
#if defined(USE_BGFX_RENDERER)
    if (!sActive)
        return;
    bgfx::reset(static_cast<uint32_t>(width), static_cast<uint32_t>(height), vsync ? BGFX_RESET_VSYNC : BGFX_RESET_NONE);
#else
    (void)width;
    (void)height;
    (void)vsync;
#endif
}

void bgfx_bridge_begin_frame()
{
#if defined(USE_BGFX_RENDERER)
    if (sActive) {
        // Interop/presentation-only mode: the game frame is rendered by raw
        // OpenGL, so bgfx should not submit clears or draw commands here.
    }
#endif
}

void bgfx_bridge_end_frame()
{
#if defined(USE_BGFX_RENDERER)
    if (sActive)
        bgfx::frame();
#endif
}

bool bgfx_bridge_active()
{
    return sActive;
}

const char* bgfx_bridge_last_error()
{
    if (sLastError.empty())
        return "none";
    return sLastError.c_str();
}
