// ---------------------------------------------------------------------------
// bgfx_frame_bench.cpp — headless frame-cost benchmark for the bgfx backend.
//
// The game itself needs a display, so frame-rate work on the Metal path has no
// way to be measured (or verified) in a headless/sandboxed session. This tool
// renders a *representative Endless-mode frame* offscreen — the same geometry
// the game submits, through the same gl3.h API — and reports where the time
// goes, so a change can be A/B compared instead of guessed at.
//
// It mirrors the game's frame composition (see src/core/trigwars.cpp
// drawOffscreens() and src/core/gamemodeClassical.cpp draw()):
//
//   * glow pass   : the scene geometry at the glow target's resolution
//   * primary pass: the grid (client arrays, GL_LINES, the dominant geometry),
//                   particles, enemies/players
//   * blur + additive composite
//
// The grid geometry is built exactly like src/render/grid.cpp does for the
// Endless arena (299x233 points -> ~139k line segments).
//
// Build/run: tools/run_bgfx_frame_bench.sh
// ---------------------------------------------------------------------------

#include "render/bgfx_backend_api.hpp"
#include "render/bgfx_bridge.hpp"
#include "render/gl3.h"

#include <bgfx/bgfx.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

uint16_t g_w = 1280;
uint16_t g_h = 800;

bgfx::FrameBufferHandle g_target = BGFX_INVALID_HANDLE;

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
double nowMs()
{
    using clock = std::chrono::steady_clock;
    static const clock::time_point start = clock::now();
    return std::chrono::duration<double, std::milli>(clock::now() - start).count();
}

// ---------------------------------------------------------------------------
// Per-section timings (printed at the end)
// ---------------------------------------------------------------------------
double g_tUpdateGrid = 0.0;
double g_tGridDraw = 0.0;
double g_tFlush = 0.0;

// ---------------------------------------------------------------------------
// Grid geometry (mirror of src/render/grid.cpp)
// ---------------------------------------------------------------------------
struct GridVertex
{
    float x, y, r, g, b, a;
};

std::vector<GridVertex> g_gridVerts;
std::vector<GLuint> g_gridElems;
std::size_t g_lightStartHorizontal = 0;
std::size_t g_lightStartVertical = 0;

int g_rx = 299; // Endless
int g_ry = 233;

// Grid point positions (world space, deformed like the spring simulation does).
std::vector<float> g_point;

void buildGrid()
{
    const float dark[4] = { 0.4f, 0.4f, 1.0f, 0.15f };
    const float light[4] = { 0.4f, 0.4f, 1.0f, 0.4f };

    g_gridVerts.clear();
    g_gridElems.clear();

    for (int i = 0; i < g_rx * g_ry; ++i) {
        GridVertex v {};
        v.r = dark[0];
        v.g = dark[1];
        v.b = dark[2];
        v.a = dark[3];
        g_gridVerts.push_back(v);
    }

    g_lightStartHorizontal = g_gridVerts.size();
    for (int y = 0; y < g_ry; y += 4) {
        for (int x = 0; x < g_rx; x++) {
            GridVertex v {};
            v.r = light[0];
            v.g = light[1];
            v.b = light[2];
            v.a = light[3];
            g_gridVerts.push_back(v);
        }
    }

    g_lightStartVertical = g_gridVerts.size();
    for (int x = 0; x < g_rx; x += 4) {
        for (int y = 0; y < g_ry; y++) {
            GridVertex v {};
            v.r = light[0];
            v.g = light[1];
            v.b = light[2];
            v.a = light[3];
            g_gridVerts.push_back(v);
        }
    }

    // Horizontal dark
    for (int y = 0; y < g_ry; y++) {
        GLuint vertex = static_cast<GLuint>(y * g_rx);
        for (int x = 0; x < g_rx - 1; x++) {
            if (y % 4 > 0) {
                g_gridElems.push_back(vertex++);
                g_gridElems.push_back(vertex);
            }
        }
    }
    // Vertical dark
    for (int x = 0; x < g_rx; x++) {
        GLuint vertex = static_cast<GLuint>(x);
        for (int y = 0; y < g_ry - 1; y++) {
            if (x % 4 > 0) {
                g_gridElems.push_back(vertex);
                vertex += static_cast<GLuint>(g_rx);
                g_gridElems.push_back(vertex);
            }
        }
    }
    // Horizontal light
    for (int y = 0, lightRow = 0; y < g_ry; y += 4, ++lightRow) {
        GLuint vertex = static_cast<GLuint>(g_lightStartHorizontal
                                            + static_cast<std::size_t>(lightRow) * static_cast<std::size_t>(g_rx));
        for (int x = 0; x < g_rx - 1; x++) {
            g_gridElems.push_back(vertex++);
            g_gridElems.push_back(vertex);
        }
    }
    // Vertical light
    for (int x = 0, lightColumn = 0; x < g_rx; x += 4, ++lightColumn) {
        GLuint vertex = static_cast<GLuint>(g_lightStartVertical
                                            + static_cast<std::size_t>(lightColumn) * static_cast<std::size_t>(g_ry));
        for (int y = 0; y < g_ry - 1; y++) {
            g_gridElems.push_back(vertex++);
            g_gridElems.push_back(vertex);
        }
    }

    g_point.assign(static_cast<std::size_t>(g_rx) * g_ry * 2, 0.0f);
    for (int y = 0; y < g_ry; ++y) {
        for (int x = 0; x < g_rx; ++x) {
            const std::size_t i = (static_cast<std::size_t>(y) * g_rx + x) * 2;
            g_point[i] = static_cast<float>(x);
            g_point[i + 1] = static_cast<float>(y);
        }
    }

    printf("grid %dx%d: %zu vertices, %zu indices (%zu segments)\n", g_rx, g_ry, g_gridVerts.size(),
           g_gridElems.size(), g_gridElems.size() / 2);
}

// Displace the grid with a travelling wave, like the spring simulation does.
void deformGrid(float t)
{
    const float amp = 0.8f;
    for (int y = 0; y < g_ry; ++y) {
        for (int x = 0; x < g_rx; ++x) {
            const std::size_t i = (static_cast<std::size_t>(y) * g_rx + x) * 2;
            const float u = static_cast<float>(x) * 0.35f + t;
            const float v = static_cast<float>(y) * 0.35f + t;
            g_point[i] = static_cast<float>(x) + std::sin(u) * amp;
            g_point[i + 1] = static_cast<float>(y) + std::cos(v) * amp;
        }
    }
}

// The grid thread's spring relaxation (see the "Run the grid" loop in
// src/render/grid.cpp): four passes over every interior point per logic tick.
// It runs on its own thread in the game, so it does not add to the frame time
// directly -- but it does compete for memory bandwidth, and it is what makes
// Endless mode's arena 5.9x more expensive to simulate.
void simulateGrid(bool damp, float dt)
{
    std::vector<float> vel(g_point.size(), 0.0f);
    const float accel = -12.0f * dt;
    const float damping = damp ? 0.6376f : 1.0f;

    for (int pass = 0; pass < 4; ++pass) {
        for (int y = 1; y < g_ry - 1; ++y) {
            for (int x = 1; x < g_rx - 1; ++x) {
                const std::size_t i = (static_cast<std::size_t>(y) * g_rx + x) * 2;
                const std::size_t left = i - 2;
                const std::size_t right = i + 2;
                const std::size_t up = i - static_cast<std::size_t>(g_rx) * 2;
                const std::size_t down = i + static_cast<std::size_t>(g_rx) * 2;

                const float avgX = (g_point[left] + g_point[right] + g_point[up] + g_point[down]) * 0.25f;
                const float avgY
                    = (g_point[left + 1] + g_point[right + 1] + g_point[up + 1] + g_point[down + 1]) * 0.25f;

                vel[i] += (g_point[i] - avgX) * accel;
                vel[i + 1] += (g_point[i + 1] - avgY) * accel;
                vel[i] *= damping;
                vel[i + 1] *= damping;
                g_point[i] += vel[i] * dt;
                g_point[i + 1] += vel[i + 1] * dt;

                // Keep the points in bounds, like grid.cpp does (branches, as
                // written there).
                if (g_point[i] < 0.0f)
                    g_point[i] = 0.0f;
                else if (g_point[i] > static_cast<float>(g_rx - 1))
                    g_point[i] = static_cast<float>(g_rx - 1);
                if (g_point[i + 1] < 0.0f)
                    g_point[i + 1] = 0.0f;
                else if (g_point[i + 1] > static_cast<float>(g_ry - 1))
                    g_point[i + 1] = static_cast<float>(g_ry - 1);
            }
        }
    }
}

// Same three vertex-refresh loops as grid::draw().
void updateGridVertices()
{
    std::size_t vertex = 0;
    for (int y = 0; y < g_ry; ++y) {
        std::size_t p = static_cast<std::size_t>(y) * g_rx * 2;
        for (int x = 0; x < g_rx; ++x) {
            g_gridVerts[vertex].x = g_point[p];
            g_gridVerts[vertex].y = g_point[p + 1];
            ++vertex;
            p += 2;
        }
    }

    vertex = g_lightStartHorizontal;
    for (int y = 0; y < g_ry; y += 4) {
        std::size_t p = static_cast<std::size_t>(y) * g_rx * 2;
        for (int x = 0; x < g_rx; x++) {
            g_gridVerts[vertex].x = g_point[p];
            g_gridVerts[vertex].y = g_point[p + 1];
            vertex++;
            p += 2;
        }
    }

    vertex = g_lightStartVertical;
    for (int x = 0; x < g_rx; x += 4) {
        std::size_t p = static_cast<std::size_t>(x) * 2;
        for (int y = 0; y < g_ry; y++) {
            g_gridVerts[vertex].x = g_point[p];
            g_gridVerts[vertex].y = g_point[p + 1];
            vertex++;
            p += static_cast<std::size_t>(g_rx) * 2;
        }
    }
}

void drawGrid()
{
    glLineWidth(6.0f);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    glVertexPointer(2, GL_FLOAT, sizeof(GridVertex), g_gridVerts.data());
    glColorPointer(4, GL_FLOAT, sizeof(GridVertex),
                   reinterpret_cast<char*>(g_gridVerts.data()) + 2 * sizeof(GLfloat));
    const double t0 = nowMs();
    glDrawElements(GL_LINES, static_cast<GLsizei>(g_gridElems.size()), GL_UNSIGNED_INT, g_gridElems.data());
    g_tGridDraw += nowMs() - t0;

    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

// ---------------------------------------------------------------------------
// Particles (mirror of particle::draw(): a 6-point position stream -> 5
// segments per particle, bucketed by width).
// ---------------------------------------------------------------------------
struct Particle
{
    float pos[6][2];
    float r, g, b;
};

std::vector<Particle> g_particles;

void buildParticles(int count)
{
    g_particles.assign(static_cast<std::size_t>(count), Particle {});
    for (int i = 0; i < count; ++i) {
        Particle& p = g_particles[static_cast<std::size_t>(i)];
        const float x = 40.0f + (i * 37 % (g_rx - 80));
        const float y = 40.0f + (i * 53 % (g_ry - 80));
        for (int s = 0; s < 6; ++s) {
            p.pos[s][0] = x + s * 0.9f;
            p.pos[s][1] = y + std::sin(static_cast<float>(s)) * 0.9f;
        }
        p.r = 1.0f;
        p.g = 0.6f;
        p.b = 0.1f;
    }
}

void drawParticles(float width)
{
    glLineWidth(width);
    glBegin(GL_LINES);
    for (const Particle& p : g_particles) {
        float aa = 0.9f;
        for (int i = 0; i < 5 && aa > 0.0f; ++i) {
            glColor4f(p.r, p.g, p.b, aa);
            glVertex2d(p.pos[i][0], p.pos[i][1]);
            glColor4f(p.r, p.g, p.b, aa - 0.1f > 0.0f ? aa - 0.1f : 0.0f);
            glVertex2d(p.pos[i + 1][0], p.pos[i + 1][1]);
            aa -= 0.2f;
        }
    }
    glEnd();
}

// ---------------------------------------------------------------------------
// Enemies / players: a stand-in for the extra vector geometry of a busy frame
// (each enemy is a small polygon outline plus a few points).
// ---------------------------------------------------------------------------
void drawEnemies(int count)
{
    glLineWidth(4.0f);
    glBegin(GL_LINES);
    for (int i = 0; i < count; ++i) {
        const float x = 30.0f + (i * 71 % (g_rx - 60));
        const float y = 30.0f + (i * 97 % (g_ry - 60));
        for (int s = 0; s < 12; ++s) {
            const float a0 = s * 0.5236f;
            const float a1 = a0 + 0.5236f;
            glColor4f(0.2f, 1.0f, 0.4f, 1.0f);
            glVertex2d(x + std::cos(a0) * 2.0f, y + std::sin(a0) * 2.0f);
            glVertex2d(x + std::cos(a1) * 2.0f, y + std::sin(a1) * 2.0f);
        }
    }
    glEnd();

    glPointSize(4.0f);
    glBegin(GL_POINTS);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    for (int i = 0; i < count * 2; ++i) {
        glVertex2d(40.0f + (i * 61 % (g_rx - 80)), 40.0f + (i * 83 % (g_ry - 80)));
    }
    glEnd();
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------
bool g_useGlow = true;
bool g_drawGrid = true;
bool g_drawParticlePass = true;
bool g_simulateGrid = false;
int g_particleCount = 1500;
int g_enemies = 60;
float g_camZ = 96.0f; // Endless camera distance

void beginScenePass()
{
    gfx_matrixmode(GL_PROJECTION);
    gfx_pushmatrix();
    gfx_loadidentity();
    gfx_perspective(70.0, static_cast<double>(g_w) / static_cast<double>(g_h), 0.01, 1000.0);

    gfx_matrixmode(GL_MODELVIEW);
    gfx_pushmatrix();
    gfx_loadidentity();
    gfx_translatef(-g_rx * 0.5f, -g_ry * 0.5f, -g_camZ);

    gfx_disable(GL_DEPTH_TEST);
    gfx_enable(GL_BLEND);
    gfx_blendfunc(GL_SRC_ALPHA, GL_ONE);
}

void endScenePass()
{
    gfx_disable(GL_BLEND);
    gfx_popmatrix();
    gfx_matrixmode(GL_PROJECTION);
    gfx_popmatrix();
}

void buildFrame()
{
    gfx_begin_frame();

    if (g_useGlow) {
        gfx_glow_bind();
        gfx_clearcolor(0, 0, 0, 1);
        gfx_clear(GL_COLOR_BUFFER_BIT);
        beginScenePass();
        if (g_drawParticlePass)
            drawParticles(10.0f);
        drawEnemies(g_enemies);
        endScenePass();
        gfx_glow_unbind();
    }

    gfx_clearcolor(0, 0, 0, 1);
    gfx_clear(GL_COLOR_BUFFER_BIT);

    beginScenePass();
    if (g_drawGrid)
        drawGrid();
    if (g_drawParticlePass)
        drawParticles(4.0f);
    drawEnemies(g_enemies);
    endScenePass();

    if (g_useGlow) {
        gfx_blur_glow();
        gfx_draw_blurred_glow(1.0f);
    }

    const double tf = nowMs();
    gfx_end_frame();
    g_tFlush += nowMs() - tf;
}

} // namespace

int main(int argc, char** argv)
{
    int frames = 200;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--no-glow")
            g_useGlow = false;
        else if (arg == "--no-grid")
            g_drawGrid = false;
        else if (arg == "--no-particles")
            g_drawParticlePass = false;
        else if (arg == "--sim")
            g_simulateGrid = true;
        else if (arg.rfind("--gridres=", 0) == 0)
            sscanf(arg.c_str() + 10, "%dx%d", &g_rx, &g_ry);
        else if (arg.rfind("--frames=", 0) == 0)
            frames = atoi(arg.c_str() + 9);
        else if (arg.rfind("--enemies=", 0) == 0)
            g_enemies = atoi(arg.c_str() + 10);
        else if (arg.rfind("--particles=", 0) == 0)
            g_particleCount = atoi(arg.c_str() + 12);
        else if (arg.rfind("--size=", 0) == 0) {
            int w = 0, h = 0;
            sscanf(arg.c_str() + 7, "%dx%d", &w, &h);
            g_w = static_cast<uint16_t>(w);
            g_h = static_cast<uint16_t>(h);
        } else {
            printf("bgfx_frame_bench: unknown option %s\n", arg.c_str());
            return 2;
        }
    }

    if (!bgfx_bridge_init(nullptr, g_w, g_h, false)) {
        printf("bgfx_frame_bench: bgfx init failed: %s\n", bgfx_bridge_last_error());
        return 2;
    }

    printf("bgfx_frame_bench: %s backend (headless %ux%u, glow=%d, grid=%d)\n",
           bgfx::getRendererName(bgfx::getRendererType()), g_w, g_h, g_useGlow ? 1 : 0, g_drawGrid ? 1 : 0);

    g_target = bgfx::createFrameBuffer(g_w, g_h, bgfx::TextureFormat::RGBA8);
    if (!bgfx::isValid(g_target)) {
        printf("bgfx_frame_bench: framebuffer creation failed\n");
        bgfx_bridge_shutdown();
        return 2;
    }

    gfx_bgfx_set_backbuffer(g_target, g_w, g_h);
    gfx_context_init();
    gfx_resize(g_w, g_h);

    if (!gfx_healthy()) {
        printf("bgfx_frame_bench: backend failed to initialise\n");
        bgfx::destroy(g_target);
        bgfx_bridge_shutdown();
        return 2;
    }

    buildGrid();
    buildParticles(g_particleCount);

    // Warm up (shader/RT creation, buffer growth).
    for (int i = 0; i < 30; ++i) {
        deformGrid(static_cast<float>(i) * 0.05f);
        updateGridVertices();
        buildFrame();
        bgfx::frame();
    }

    double buildMs = 0.0;
    double frameMs = 0.0;
    g_tUpdateGrid = g_tGridDraw = g_tFlush = 0.0;
    for (int i = 0; i < frames; ++i) {
        deformGrid(static_cast<float>(i) * 0.05f);
        const double tu = nowMs();
        if (g_simulateGrid)
            simulateGrid(true, 0.3f);
        updateGridVertices();
        g_tUpdateGrid += nowMs() - tu;

        const double t0 = nowMs();
        buildFrame();
        const double t1 = nowMs();
        const uint32_t pending = bgfx::frame();
        const double t2 = nowMs();

        buildMs += t1 - t0;
        frameMs += t2 - t1;
        (void)pending;
    }

    const bgfx::Stats* stats = bgfx::getStats();
    printf("bgfx_frame_bench: %d frames  build=%.3fms  submit=%.3fms  total=%.3fms  (%.1f fps)\n", frames,
           buildMs / frames, frameMs / frames, (buildMs + frameMs) / frames, 1000.0 * frames / (buildMs + frameMs));
    if (stats != nullptr) {
        printf("bgfx_frame_bench: sections  grid-update=%.3fms  grid-draw=%.3fms  flush/submit-build=%.3fms  other=%.3fms\n",
               g_tUpdateGrid / frames, g_tGridDraw / frames, g_tFlush / frames,
               (buildMs - g_tUpdateGrid - g_tGridDraw - g_tFlush) / frames);        printf("bgfx_frame_bench: draws/frame=%.1f  transientVb=%dKiB  cpuFrame=%.2fms  waits render=%lldms submit=%lldms\n",
               static_cast<double>(stats->numDraw) / frames, stats->transientVbUsed / 1024,
               static_cast<double>(stats->cpuTimeFrame) / static_cast<double>(stats->cpuTimerFreq / 1000),
               static_cast<long long>(stats->waitRender / (stats->cpuTimerFreq / 1000)),
               static_cast<long long>(stats->waitSubmit / (stats->cpuTimerFreq / 1000)));
    }

    gfx_context_shutdown();
    bgfx::destroy(g_target);
    bgfx_bridge_shutdown();
    return 0;
}
