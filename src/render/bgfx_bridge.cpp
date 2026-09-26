#include "render/bgfx_bridge.hpp"

#include <cstdint>
#include <string>

#if defined(USE_BGFX_RENDERER)
#include <bgfx/bgfx.h>
#endif

namespace {

bool sActive = false;
std::string sLastError;

#if defined(USE_BGFX_RENDERER) && defined(__APPLE__)
// The Metal view SDL created for us (bgfx renders into its CAMetalLayer).
SDL_MetalView sMetalView = nullptr;
#endif

void setLastError(const char* msg)
{
    sLastError = msg ? msg : "unknown bgfx error";
}

#if defined(USE_BGFX_RENDERER)
// The renderer this build wants bgfx to use.  bgfx no longer ships an OpenGL
// backend on Apple platforms (upstream commit 928800fea, "macOS, iOS: Removed
// OpenGL/OpenGLES support"), so the only way to hand presentation to bgfx there
// is Metal — which is exactly what `USE_BGFX=1` does on macOS.  Everywhere else
// bgfx is asked for OpenGL so it can share the game's GL context.
bgfx::RendererType::Enum wantedRendererType()
{
#if defined(__APPLE__)
    return bgfx::RendererType::Metal;
#else
    return bgfx::RendererType::OpenGL;
#endif
}

// Only let bgfx take the window when it really has the backend we need.
// bgfx silently falls back to another renderer otherwise (to Metal instead of
// OpenGL on macOS), and because nothing used to be submitted through bgfx the
// window then showed the uninitialised native layer — the solid pink screen.
bool hasRendererBackend(bgfx::RendererType::Enum type)
{
    bgfx::RendererType::Enum types[bgfx::RendererType::Count];
    const uint8_t count = bgfx::getSupportedRenderers(bgfx::RendererType::Count, types);
    for (uint8_t i = 0; i < count; ++i) {
        if (types[i] == type)
            return true;
    }
    return false;
}
#endif // USE_BGFX_RENDERER

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
    const bgfx::RendererType::Enum wanted = wantedRendererType();
    if (!hasRendererBackend(wanted)) {
        setLastError("bgfx has no usable backend for this platform");
        return false;
    }

    bgfx::PlatformData pd {};
    pd.type = bgfx::NativeWindowHandleType::Default;

    void* nativeWindowHandle = nullptr;
    void* nativeDisplayType = nullptr;

    if (window != nullptr) {
#if defined(__APPLE__)
        // Let SDL create the Metal view; bgfx renders into its CAMetalLayer.
        sMetalView = SDL_Metal_CreateView(window);
        if (sMetalView == nullptr) {
            setLastError("SDL_Metal_CreateView failed");
            return false;
        }
        nativeWindowHandle = SDL_Metal_GetLayer(sMetalView);
        if (nativeWindowHandle == nullptr) {
            SDL_Metal_DestroyView(sMetalView);
            sMetalView = nullptr;
            setLastError("SDL_Metal_GetLayer returned no layer");
            return false;
        }
#else
        SDL_PropertiesID props = SDL_GetWindowProperties(window);
        if (!props) {
            setLastError("SDL_GetWindowProperties failed");
            return false;
        }
#if defined(_WIN32)
        nativeWindowHandle = SDL_GetPointerProperty(props, "SDL.window.win32.hwnd", nullptr);
#elif defined(__linux__)
        nativeWindowHandle = SDL_GetPointerProperty(props, "SDL.window.wayland.surface", nullptr);
        nativeDisplayType = SDL_GetPointerProperty(props, "SDL.window.wayland.display", nullptr);
        if (nativeWindowHandle != nullptr) {
            pd.type = bgfx::NativeWindowHandleType::Wayland;
        } else {
            const Sint64 x11Window = SDL_GetNumberProperty(props, "SDL.window.x11.window", 0);
            nativeWindowHandle = reinterpret_cast<void*>(static_cast<uintptr_t>(x11Window));
            nativeDisplayType = SDL_GetPointerProperty(props, "SDL.window.x11.display", nullptr);
        }
#endif
        if (nativeWindowHandle == nullptr) {
            setLastError("could not resolve native window handle from SDL");
            return false;
        }
#endif // __APPLE__
    }

    bgfx::Init init {};
    init.type = wanted;
    // Never let bgfx silently switch renderer: this build renders through the
    // backend we ask for, and a fallback (e.g. Metal failing over to Noop)
    // would leave the window blank instead of reporting the problem.
    init.fallback = false;
    init.reset = vsync ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;

    if (window != nullptr) {
        init.platformData = pd;
        init.swapChain.nwh = nativeWindowHandle;
        init.swapChain.ndt = nativeDisplayType;
        init.swapChain.width = static_cast<uint32_t>(width);
        init.swapChain.height = static_cast<uint32_t>(height);
    } else {
        // Headless: no swap chain (used by the offscreen verification tooling).
        init.swapChain.width = 0;
        init.swapChain.height = 0;
    }

    if (!bgfx::init(init)) {
        setLastError("bgfx::init failed");
        return false;
    }

    // Belt and braces: if bgfx still selected another renderer, shut it down
    // again rather than letting it own presentation and blank the window.
    if (bgfx::getRendererType() != wanted) {
        bgfx::shutdown();
        setLastError("bgfx initialised a different renderer than requested");
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
#if defined(__APPLE__)
    if (sMetalView != nullptr) {
        SDL_Metal_DestroyView(sMetalView);
        sMetalView = nullptr;
    }
#endif
#endif
    sActive = false;
}

void bgfx_bridge_resize(int width, int height, bool vsync)
{
#if defined(USE_BGFX_RENDERER)
    if (!sActive || width <= 0 || height <= 0)
        return;
    bgfx::SwapChain swapChain {};
    swapChain.width = static_cast<uint32_t>(width);
    swapChain.height = static_cast<uint32_t>(height);
    bgfx::reset(vsync ? BGFX_RESET_VSYNC : BGFX_RESET_NONE, &swapChain);
#else
    (void)width;
    (void)height;
    (void)vsync;
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
