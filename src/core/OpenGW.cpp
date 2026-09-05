#include "core/controls.hpp"
#include "core/game.hpp"
#include "render/scene.hpp"
#include "core/settings.hpp"
#include "render/gl3.h"
#include "math/sincos.hpp"

#include <SDL3/SDL.h>

#include <cstdio>
#include <memory>

static SDL_Window* window;
static SDL_GLContext context;

static bool OGLCreate();
static void OGLDestroy();
static void OGLSize(int cx, int cy);
static void applySettingsToWindow();

std::unique_ptr<scene> oglScene;

static bool oglInited = false;

static void drawOffscreens();
static void run();

static int mWidth, mHeight;

static Uint32 lastTime;
static Uint32 fpsTime;
static int frameCount;
static int fps;

const bool* keyboardState;

static bool handleEvents()
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_EVENT_QUIT:
            printf("Quit\n");
            return false;
        case SDL_EVENT_WINDOW_RESIZED:
            OGLSize(e.window.data1, e.window.data2);
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
            theGame->mControls->handleGamepadAdded(e.cdevice.which);
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            theGame->mControls->handleGamepadRemoved(e.cdevice.which);
            break;
        }
    }

    keyboardState = SDL_GetKeyboardState(nullptr);
    return true;
}

int main(int /*argc*/, char** /*argv*/)
{
    printf("SDL_Init\n");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD)) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 0;
    }

    // ------------------------------------------------------------------
    // Request a modern OpenGL 3.3 CORE profile context with MSAA.
    // The old fixed-function / compatibility pipeline is no longer used.
    // ------------------------------------------------------------------
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (0) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }

    window = SDL_CreateWindow("OpenGW",
                              settings::get().displayWidth, settings::get().displayHeight, flags);

    if (window) {
        srand(SDL_GetTicks());
        make_sin_cos_tables();
        oglScene = std::make_unique<scene>();

        if (OGLCreate()) {
            run();
            OGLDestroy();
        }

        oglScene.reset();

        // Persist any graphics options to settings.cfg on exit.
        settings::edit().save();

        SDL_DestroyWindow(window);
    } else {
        printf("Failed to create window: %s\n", SDL_GetError());
    }

    printf("SDL_Quit\n");
    SDL_Quit();

    return 0;
}

static bool OGLCreate()
{
    context = SDL_GL_CreateContext(window);

    if (context == nullptr) {
        printf("SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_ShowSimpleMessageBox(0, "OpenGW - OpenGL error",
                                 "Could not create an OpenGL 3.3 core context.\n\n"
                                 "This system does not appear to support OpenGL 3.3+.",
                                 window);
        return false;
    }

    if (!SDL_GL_MakeCurrent(window, context)) {
        printf("SDL_GL_MakeCurrent failed: %s\n", SDL_GetError());
        SDL_GL_DestroyContext(context);
        context = nullptr;
        SDL_ShowSimpleMessageBox(0, "OpenGW - OpenGL error",
                                 "Could not make the OpenGL context current.\n\n"
                                 "Check the console output for details.",
                                 window);
        return false;
    }

    // Bring up the modern GL3 render backend (shaders, VAOs, FBO targets).
    gfx_context_init();

    if (!gfx_healthy()) {
        printf("gl3: renderer failed to initialise (see shader errors above)\n");
        gfx_context_shutdown();
        SDL_GL_MakeCurrent(nullptr, nullptr);
        SDL_GL_DestroyContext(context);
        context = nullptr;
        SDL_ShowSimpleMessageBox(0, "OpenGW - Shader error",
                                 "The modern GL renderer failed to start\n"
                                 "(shader compile/link errors - see console).",
                                 window);
        return false;
    }

    gfx_set_glow_enabled(settings::get().mEnableGlow);

    // (Re)create glow/blur render targets for the current window size.
    OGLSize(settings::get().displayWidth, settings::get().displayHeight);

    // Apply fullscreen/vsync from the (possibly restored) settings.
    applySettingsToWindow();

    oglInited = true;
    return true;
}

static void OGLDestroy()
{
    oglInited = false;

    gfx_context_shutdown();

    if (context) {
        SDL_GL_MakeCurrent(nullptr, nullptr);
        SDL_GL_DestroyContext(context);
        context = nullptr;
    }
}

static void OGLSize(int cx, int cy)
{
    // Use the window's pixel size (accounts for retina / HiDPI scaling) so the
    // viewport, FBOs and clear always cover the whole back buffer. The aspect
    // ratio is identical, so game math is unaffected.
    int dw = cx;
    int dh = cy;
    if (window)
        SDL_GetWindowSizeInPixels(window, &dw, &dh);

    oglScene->size(dw, dh);
    mWidth = dw;
    mHeight = dh;

    gfx_resize(dw, dh);
}

// Applies any graphics-option changes made in the options screen (window
// resolution, fullscreen, vsync) to the live window / GL context.
static void applySettingsToWindow()
{
    // Track the *actual* state of the window / GL, so the first call here
    // applies whatever the (possibly restored) settings.cfg requests.
    static int lastW = 0;
    static int lastH = 0;
    static bool lastFullscreen = false; // windows start windowed
    static bool lastVsync = false;      // GL starts with swap interval 0

    const settings& s = settings::get();

    if (s.displayWidth != lastW || s.displayHeight != lastH) {
        SDL_SetWindowSize(window, s.displayWidth, s.displayHeight);
        lastW = s.displayWidth;
        lastH = s.displayHeight;
    }
    if (s.mFullscreen != lastFullscreen) {
        SDL_SetWindowFullscreen(window, s.mFullscreen);
        lastFullscreen = s.mFullscreen;
    }
    if (s.mVsync != lastVsync) {
        SDL_GL_SetSwapInterval(s.mVsync ? 1 : 0);
        lastVsync = s.mVsync;
    }
}

static void drawOffscreens()
{
    // Ensure we're drawing to the default framebuffer, the viewport covers
    // the whole back buffer, and it's cleared to black.
    gfx_begin_frame();

    if (settings::get().mEnableGlow && gfx_glow_enabled()) {
        // --------------------------------------------------------------
        // Glow pass: render the whole scene into a low-resolution texture
        // using a GPU framebuffer object (no CPU read-back any more).
        // --------------------------------------------------------------
        gfx_glow_bind();
        oglScene->draw(scene::RENDERPASS_BLUR);

        // --------------------------------------------------------------
        // Primary pass: draw the scene at full resolution.
        // --------------------------------------------------------------
        gfx_glow_unbind();
        oglScene->draw(scene::RENDERPASS_PRIMARY);

        // --------------------------------------------------------------
        // GPU Gaussian blur of the glow buffer, then add it back on top
        // with additive blending (the classic "bloom" look).
        // --------------------------------------------------------------
        gfx_blur_glow();

        const bool menu = (game::mGameMode == game::GAMEMODE_ATTRACT || game::mGameMode == game::GAMEMODE_CREDITED);
        gfx_draw_blurred_glow(menu ? 1.4f : 1.0f);
    } else {
        // Glow disabled — single full-resolution pass.
        oglScene->draw(scene::RENDERPASS_PRIMARY);
    }
}

static void updateFps(Uint32 now)
{
    ++frameCount;

    lastTime = now;

    if ((now - fpsTime) > 1000) {
        fpsTime = now;
        fps = (fps + frameCount) / 2;
        frameCount = 0;

        char buf[64];
        snprintf(buf, sizeof(buf), "OpenGW - FPS %d", fps);
        SDL_SetWindowTitle(window, buf);
    }
}

static void run()
{
    if (!oglInited)
        return;

    constexpr Uint32 logicRate = 60;
    constexpr Uint32 logicPeriod = 1000 / logicRate;

    Uint32 lastLogicUpdate = SDL_GetTicks();

    bool running = true;

    while (running) {
        const Uint32 now = SDL_GetTicks();

        // Fixed 60 Hz logic step. Cap catch-up iterations so a slow frame
        // (e.g. an OS hiccup or a resize) can't trigger a "spiral of death".
        int steps = 0;
        while ((now - lastLogicUpdate) > logicPeriod && steps < 5) {
            lastLogicUpdate += logicPeriod;
            ++steps;
            if (!handleEvents()) {
                running = false;
            }

            oglScene->run();
        }
        if ((now - lastLogicUpdate) > logicPeriod)
            lastLogicUpdate = now;

        // Apply any pending graphics-option changes to the window / GL state.
        applySettingsToWindow();

        drawOffscreens();

        SDL_GL_SwapWindow(window);
        updateFps(now);
    }
}
