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

static void OGLCreate();
static void OGLDestroy();
static void OGLSize(int cx, int cy);

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

    Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL;
    if (0) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }

    window = SDL_CreateWindow("OpenGW",
                              settings::get().displayWidth, settings::get().displayHeight, flags);

    if (window) {
        srand(SDL_GetTicks());
        make_sin_cos_tables();
        oglScene = std::make_unique<scene>();

        OGLCreate();
        run();
        OGLDestroy();

        oglScene.reset();

        SDL_DestroyWindow(window);
    } else {
        printf("Failed to create window: %s\n", SDL_GetError());
    }

    printf("SDL_Quit\n");
    SDL_Quit();

    return 0;
}

static void OGLCreate()
{
    context = SDL_GL_CreateContext(window);

    if (context == nullptr) {
        printf("SDL_GL_CreateContext failed: %s\n", SDL_GetError());
    }

    if (!SDL_GL_MakeCurrent(window, context)) {
        printf("SDL_GL_MakeCurrent failed: %s\n", SDL_GetError());
    }

    // Bring up the modern GL3 render backend (shaders, VAOs, FBO targets).
    gfx_context_init();
    gfx_set_glow_enabled(settings::get().mEnableGlow);

    // (Re)create glow/blur render targets for the current window size.
    OGLSize(settings::get().displayWidth, settings::get().displayHeight);

    // Vsync: keep rendering in step with the display (and avoid burning the
    // GPU at thousands of uncapped frames per second).
    SDL_GL_SetSwapInterval(1);

    oglInited = true;
}

static void OGLDestroy()
{
    oglInited = false;

    gfx_context_shutdown();

    SDL_GL_MakeCurrent(nullptr, nullptr);
    SDL_GL_DestroyContext(context);
}

static void OGLSize(int cx, int cy)
{
    oglScene->size(cx, cy);
    mWidth = cx;
    mHeight = cy;

    gfx_resize(cx, cy);
}

static void drawOffscreens()
{
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

        drawOffscreens();

        SDL_GL_SwapWindow(window);
        updateFps(now);
    }
}
