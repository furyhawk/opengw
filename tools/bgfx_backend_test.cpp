// ---------------------------------------------------------------------------
// bgfx_backend_test.cpp — headless verification for the bgfx render backend
//
// The game can only be run with a real display, which makes GPU regressions
// hard to catch. This tool drives the bgfx backend through the *same* gl3.h
// API the game uses, renders into an offscreen framebuffer with the Metal
// backend (no window needed), reads the result back and asserts on pixels.
//
// It covers the parts of the backend that are easy to get wrong when porting
// the OpenGL renderer to bgfx: clip-space orientation, NDC depth conventions,
// additive blending, wide lines/points, textured quads and the glow/blur pass
// alignment (a flipped V coordinate in the blur chain would silently offset the
// bloom).
//
// Build (from the repo root):
//   c++ -std=c++20 -Isrc -DUSE_BGFX_RENDERER \
//       $BGFX_HOME/include -I$BGFX_HOME/../bx/include -I$BGFX_HOME/../bimg/include \
//       -I$BGFX_HOME/include \
//       src/render/bgfx_backend.cpp tools/bgfx_backend_test.cpp \
//       $BGFX_HOME/.build/osx-arm64/bin/libbgfxRelease.a ... -framework Cocoa ...
//
// tools/run_bgfx_backend_test.sh does this for you.
// ---------------------------------------------------------------------------

#include "render/gl3.h"
#include "render/bgfx_backend_api.hpp"
#include "render/bgfx_bridge.hpp"
#include "render/bgfx_bridge.hpp"

#include <bgfx/bgfx.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

constexpr uint16_t kW = 256;
constexpr uint16_t kH = 256;
constexpr bgfx::ViewId kViewBlit = 15;

bgfx::FrameBufferHandle g_target = BGFX_INVALID_HANDLE;
bgfx::TextureHandle g_readback = BGFX_INVALID_HANDLE;
std::vector<uint32_t> g_pixels;

int g_failures = 0;
int g_checks = 0;

void check(bool ok, const char* what)
{
    ++g_checks;
    if (!ok) {
        ++g_failures;
        printf("  FAIL  %s\n", what);
    } else {
        printf("  ok    %s\n", what);
    }
}

// RGBA helpers (the read-back target uses RGBA8).
inline uint8_t rgbr(uint32_t p) { return uint8_t(p & 0xff); }

// Read back a pixel. Row 0 of the read-back buffer is the *top* row on Metal
// (its viewport has a top-left origin), so we index it directly as y-up NDC
// converted to "distance from top".
uint32_t pixelAt(int x, int yTopDown)
{
    if (x < 0 || yTopDown < 0 || x >= kW || yTopDown >= kH)
        return 0;
    return g_pixels[static_cast<size_t>(yTopDown) * kW + x];
}

uint32_t pixelNdc(float x, float y)
{
    const int px = static_cast<int>((x * 0.5f + 0.5f) * (kW - 1));
    const int py = static_cast<int>((0.5f - y * 0.5f) * (kH - 1)); // NDC +y = top
    return pixelAt(px, py);
}

// Everything the game does between gfx_begin_frame() and the swap: the glow
// pass, the primary pass, the blur and the composite.
void drawGameFrame(bool glow, float spotX, float spotY)
{
    gfx_begin_frame();

    if (glow) {
        gfx_glow_bind();
        gfx_clearcolor(0, 0, 0, 1);
        gfx_clear(GL_COLOR_BUFFER_BIT);
        // Bright spot, at the glow target's resolution.
        gfx_disable(GL_BLEND);
        gfx_color4f(1, 1, 1, 1);
        gfx_begin(GL_QUADS);
        gfx_vertex2d(spotX - 0.15f, spotY - 0.15f);
        gfx_vertex2d(spotX + 0.15f, spotY - 0.15f);
        gfx_vertex2d(spotX + 0.15f, spotY + 0.15f);
        gfx_vertex2d(spotX - 0.15f, spotY + 0.15f);
        gfx_end();
        gfx_glow_unbind();
    }

    gfx_clearcolor(0, 0, 0, 1);
    gfx_clear(GL_COLOR_BUFFER_BIT);

    if (glow) {
        gfx_blur_glow();
        gfx_draw_blurred_glow(1.0f);
    }
}

// Renders the frame, blits the back buffer into the read-back texture and waits
// for the result.
void capture()
{
    gfx_end_frame(); // submit the accumulated batches
    bgfx::blit(kViewBlit, { .handle = g_readback }, { .handle = bgfx::getTexture(g_target) });
    const uint32_t expected = bgfx::read({ .handle = g_readback }, g_pixels.data());
    uint32_t current = bgfx::frame();
    while (current < expected)
        current = bgfx::frame();
}

// ---------------------------------------------------------------------------

void testClearAndOrientation()
{
    printf("clear + clip-space orientation\n");

    gfx_begin_frame();
    gfx_disable(GL_BLEND);
    gfx_clearcolor(0, 0, 1, 1); // blue
    gfx_clear(GL_COLOR_BUFFER_BIT);

    // Red quad in the top-left quadrant of clip space (x -1..0, y 0..1).
    gfx_color4f(1, 0, 0, 1);
    gfx_begin(GL_QUADS);
    gfx_vertex2d(-1, 0);
    gfx_vertex2d(0, 0);
    gfx_vertex2d(0, 1);
    gfx_vertex2d(-1, 1);
    gfx_end();
    capture();

    check(rgbr(pixelNdc(-0.5f, 0.5f)) > 200, "quad drawn in the -x/+y quadrant is red");
    check(rgbr(pixelNdc(0.5f, 0.5f)) < 40, "the +x/+y quadrant stays blue/black");
    check(rgbr(pixelNdc(-0.5f, -0.5f)) < 40, "the -x/-y quadrant stays blue/black");
}

void testDepthRange()
{
    printf("NDC depth convention (perspective geometry must not be clipped)\n");

    gfx_begin_frame();
    gfx_disable(GL_BLEND);
    gfx_clearcolor(0, 0, 0, 1);
    gfx_clear(GL_COLOR_BUFFER_BIT);

    // A quad at z = -50 through a perspective projection lands *behind* the
    // near plane; with GL-style depth it is on screen, with a [0,1] depth
    // range it would be clipped away entirely unless the depth is remapped.
    gfx_matrixmode(GL_PROJECTION);
    gfx_pushmatrix();
    gfx_loadidentity();
    gfx_perspective(70.0, 1.0, 0.01, 1000.0);
    gfx_matrixmode(GL_MODELVIEW);
    gfx_pushmatrix();
    gfx_loadidentity();

    gfx_color4f(0, 1, 0, 1);
    gfx_begin(GL_QUADS);
    gfx_vertex3d(-20, -20, -50);
    gfx_vertex3d(20, -20, -50);
    gfx_vertex3d(20, 20, -50);
    gfx_vertex3d(-20, 20, -50);
    gfx_end();

    gfx_popmatrix();
    gfx_matrixmode(GL_PROJECTION);
    gfx_popmatrix();
    capture();

    check((pixelNdc(0.0f, 0.0f) >> 8 & 0xff) > 200, "perspective quad at z=-50 is visible");
}

void testBlendModes()
{
    printf("blend modes\n");

    gfx_begin_frame();
    gfx_disable(GL_BLEND);
    gfx_clearcolor(0, 0, 0, 1);
    gfx_clear(GL_COLOR_BUFFER_BIT);

    // Additive: 0.5 + 0.5 alpha-blended green over nothing.
    gfx_enable(GL_BLEND);
    gfx_blendfunc(GL_SRC_ALPHA, GL_ONE);
    gfx_color4f(0, 1, 0, 0.5f);
    gfx_begin(GL_QUADS);
    gfx_vertex2d(-0.5f, -0.5f);
    gfx_vertex2d(0.5f, -0.5f);
    gfx_vertex2d(0.5f, 0.5f);
    gfx_vertex2d(-0.5f, 0.5f);
    gfx_end();
    capture();

    const uint8_t g = uint8_t(pixelNdc(0.0f, 0.0f) >> 8);
    check(g > 100 && g < 160, "additive blending with alpha 0.5 gives ~half green");

    // DST_COLOR / ONE_MINUS_SRC_ALPHA (used by the grid):
    //   result = src*dst_color + dst*(1-src_alpha)
    // With an opaque red quad over a blue destination that is
    //   (1,0,0)*(0,0,1) + (0,0,1)*0 = black
    // so the grid's blend mode darkens rather than adds.
    gfx_begin_frame();
    gfx_disable(GL_BLEND);
    gfx_clearcolor(0, 0, 1, 1);
    gfx_clear(GL_COLOR_BUFFER_BIT);
    gfx_enable(GL_BLEND);
    gfx_blendfunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
    gfx_color4f(1, 0, 0, 1);
    gfx_begin(GL_QUADS);
    gfx_vertex2d(-1, -1);
    gfx_vertex2d(1, -1);
    gfx_vertex2d(1, 1);
    gfx_vertex2d(-1, 1);
    gfx_end();
    capture();

    const uint32_t p = pixelNdc(0.0f, 0.0f);
    const bool black = rgbr(p) < 40 && (p >> 8 & 0xff) < 40 && (p >> 16 & 0xff) < 40;
    check(black, "DST_COLOR/INV_SRC_ALPHA multiplies the destination (red * blue = black)");

    // Sanity checks for the two factors used above, each in its own frame so the
    // destination (blue) is unambiguous.
    gfx_begin_frame();
    gfx_disable(GL_BLEND);
    gfx_clearcolor(0, 0, 1, 1);
    gfx_clear(GL_COLOR_BUFFER_BIT);
    gfx_enable(GL_BLEND);
    gfx_blendfunc(GL_ZERO, GL_ONE); // dst only
    gfx_color4f(1, 0, 0, 1);
    gfx_begin(GL_QUADS);
    gfx_vertex2d(-1, -1);
    gfx_vertex2d(1, -1);
    gfx_vertex2d(1, 1);
    gfx_vertex2d(-1, 1);
    gfx_end();
    capture();
    const uint32_t dstOnly = pixelNdc(0.0f, 0.0f);

    gfx_begin_frame();
    gfx_disable(GL_BLEND);
    gfx_clearcolor(0, 0, 1, 1);
    gfx_clear(GL_COLOR_BUFFER_BIT);
    gfx_enable(GL_BLEND);
    gfx_blendfunc(GL_DST_COLOR, GL_ZERO); // src * dst
    gfx_color4f(1, 0, 0, 1);
    gfx_begin(GL_QUADS);
    gfx_vertex2d(-1, -1);
    gfx_vertex2d(1, -1);
    gfx_vertex2d(1, 1);
    gfx_vertex2d(-1, 1);
    gfx_end();
    capture();
    const uint32_t srcTimesDst = pixelNdc(0.0f, 0.0f);
    check((dstOnly >> 16 & 0xff) > 200 && rgbr(dstOnly) < 40, "blend factor ONE/ONE keeps the blue destination");
    check((srcTimesDst >> 16 & 0xff) < 40 && rgbr(srcTimesDst) < 40, "src(1,0,0) * dst(0,0,1) is black");
}

void testWideLinesAndPoints()
{
    printf("wide lines and points\n");

    gfx_begin_frame();
    gfx_disable(GL_BLEND);
    gfx_clearcolor(0, 0, 0, 1);
    gfx_clear(GL_COLOR_BUFFER_BIT);

    gfx_linewidth(8.0f);
    gfx_color4f(1, 0, 0, 1);
    gfx_begin(GL_LINES);
    gfx_vertex2d(-1, 0);
    gfx_vertex2d(1, 0);
    gfx_end();

    gfx_pointsize(16.0f);
    gfx_color4f(0, 0, 1, 1);
    gfx_begin(GL_POINTS);
    gfx_vertex2d(0, 0.5f);
    gfx_end();
    capture();

    // The line is 8px tall: count red pixels in the middle column.
    int linePixels = 0;
    for (int y = 0; y < kH; ++y) {
        if (rgbr(pixelAt(kW / 2, y)) > 200)
            ++linePixels;
    }
    check(linePixels >= 7 && linePixels <= 9, "8px wide line covers 8 rows");

    // The 16px blue point sits at NDC (0, 0.5); count it in the middle column.
    int pointPixels = 0;
    for (int y = 0; y < kH; ++y) {
        const uint32_t p = pixelAt(kW / 2, y);
        if ((p >> 16 & 0xff) > 200 && rgbr(p) < 40)
            ++pointPixels;
    }
    check(pointPixels >= 14 && pointPixels <= 18, "16px point covers ~16 rows");
}

void testTextures()
{
    printf("textured quads (modulate + uv orientation)\n");

    // 2x2 texture: red = top-left, green = top-right, blue = bottom-left,
    // white = bottom-right (in GL texture terms: v=0 is the *last* row).
    static const uint8_t image[2 * 2 * 4] = {
        255, 0, 0, 255, 0, 255, 0, 255, // v = 0: red, green  (first row in memory)
        0, 0, 255, 255, 255, 255, 255, 255, // v = 1: blue, white
    };

    GLuint id = 0;
    gfx_gentextures(1, &id);
    gfx_bindtexture(GL_TEXTURE_2D, id);
    gfx_texparameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gfx_texparameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gfx_teximage2d(GL_TEXTURE_2D, 0, 4, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

    gfx_begin_frame();
    gfx_disable(GL_BLEND);
    gfx_clearcolor(0, 0, 0, 1);
    gfx_clear(GL_COLOR_BUFFER_BIT);

    gfx_enable(GL_TEXTURE_2D);
    gfx_bindtexture(GL_TEXTURE_2D, id);
    gfx_color4f(1, 1, 1, 1);
    // Full-screen quad: uv (0,0) at the bottom-left vertex, matching the game's
    // texture.cpp.
    gfx_begin(GL_QUADS);
    gfx_texcoord2d(0, 1);
    gfx_vertex2d(-1, -1);
    gfx_texcoord2d(1, 1);
    gfx_vertex2d(1, -1);
    gfx_texcoord2d(1, 0);
    gfx_vertex2d(1, 1);
    gfx_texcoord2d(0, 0);
    gfx_vertex2d(-1, 1);
    gfx_end();
    gfx_disable(GL_TEXTURE_2D);
    capture();

    // Sample the centre of each texel of the 2x2 image: the first row in
    // memory (v=0) must appear at the top of the quad, the second at the
    // bottom, and texel 0 must be on the left.
    const uint32_t topLeft = pixelNdc(-0.5f, 0.5f);
    const uint32_t topRight = pixelNdc(0.5f, 0.5f);
    const uint32_t bottomLeft = pixelNdc(-0.5f, -0.5f);
    const uint32_t bottomRight = pixelNdc(0.5f, -0.5f);

    check(rgbr(topLeft) > 200 && (topLeft >> 8 & 0xff) < 60, "texel (0,0) - first column, "
                                                             "first row - is top-left");
    check((topRight >> 8 & 0xff) > 200 && rgbr(topRight) < 60, "texel (1,0) is top-right");
    check((bottomLeft >> 16 & 0xff) > 200, "texel (0,1) - second row - is bottom-left");
    check(rgbr(bottomRight) > 200 && (bottomRight >> 8 & 0xff) > 200 && (bottomRight >> 16 & 0xff) > 200,
          "texel (1,1) is bottom-right (white)");
}

void testClientArrays()
{
    printf("client arrays (grid path)\n");

    static const float verts[] = { -1, -1, 1, -1, 1, 1, -1, 1 };
    static const unsigned short idx[] = { 0, 1, 1, 2, 2, 3, 3, 0 };

    gfx_begin_frame();
    gfx_disable(GL_BLEND);
    gfx_clearcolor(0, 0, 0, 1);
    gfx_clear(GL_COLOR_BUFFER_BIT);

    gfx_linewidth(4.0f);
    gfx_color4f(0, 1, 1, 1);
    gfx_enableclientstate(GL_VERTEX_ARRAY);
    gfx_vertexpointer(2, GL_FLOAT, 0, verts);
    gfx_drawelements(GL_LINES, 8, GL_UNSIGNED_SHORT, idx);
    gfx_disableclientstate(GL_VERTEX_ARRAY);
    capture();

    // The bottom edge of the line loop runs along NDC y=-1; a 4px wide line
    // centred on it covers the last two rows.
    check((pixelNdc(0.0f, -0.999f) >> 8 & 0xff) > 150, "client-array line loop edge is drawn");
    check((pixelNdc(-0.99f, 0.0f) >> 8 & 0xff) > 150, "client-array line loop left edge is drawn");
}

// The glow pass is the easiest place to introduce a silent vertical flip: the
// bloom must line up with the geometry that produced it.
void testGlowAlignment()
{
    printf("glow/blur alignment\n");

    const float spotX = -0.5f;
    const float spotY = 0.5f; // top-left quadrant of NDC

    drawGameFrame(true, spotX, spotY);
    capture();

    // Compare the brightest pixel region of the composite with the spot.
    uint32_t best = 0;
    int bestX = 0, bestY = 0;
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const uint32_t p = pixelAt(x, y);
            const uint32_t lum = rgbr(p) + (p >> 8 & 0xff) + (p >> 16 & 0xff);
            if (lum > best) {
                best = lum;
                bestX = x;
                bestY = y;
            }
        }
    }

    const int expectX = static_cast<int>((spotX * 0.5f + 0.5f) * (kW - 1));
    const int expectY = static_cast<int>((0.5f - spotY * 0.5f) * (kH - 1));
    const int dx = bestX - expectX;
    const int dy = bestY - expectY;

    printf("        brightest pixel at (%d,%d), spot expected at (%d,%d)\n", bestX, bestY, expectX, expectY);
    check(best > 100, "the glow survives the blur + composite");
    check(dx > -24 && dx < 24, "the glow is horizontally aligned with its source");
    check(dy > -24 && dy < 24, "the glow is vertically aligned with its source");
}

void testGlowOffIsUnchanged()
{
    printf("glow disabled\n");

    gfx_set_glow_enabled(false);
    drawGameFrame(false, 0.0f, 0.0f);
    capture();

    const uint32_t p = pixelNdc(0.0f, 0.0f);
    check((rgbr(p) + (p >> 8 & 0xff) + (p >> 16 & 0xff)) < 40, "no bloom when glow is disabled");
    gfx_set_glow_enabled(true);
}

} // namespace

int main()
{
    // Headless init through the same bridge the game uses (null window = no
    // swap chain), so its code path is covered too.
    if (!bgfx_bridge_init(nullptr, kW, kH, false)) {
        printf("bgfx_backend_test: bgfx init failed: %s\n", bgfx_bridge_last_error());
        return 2;
    }

    printf("bgfx_backend_test: %s backend (headless %ux%u)\n",
           bgfx::getRendererName(bgfx::getRendererType()), kW, kH);

    g_target = bgfx::createFrameBuffer(kW, kH, bgfx::TextureFormat::RGBA8);
    g_readback = bgfx::createTexture2D(kW, kH, false, 1, bgfx::TextureFormat::RGBA8,
                                       BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK);
    g_pixels.assign(static_cast<size_t>(kW) * kH, 0);

    if (!bgfx::isValid(g_target) || !bgfx::isValid(g_readback)) {
        printf("bgfx_backend_test: framebuffer creation failed\n");
        bgfx_bridge_shutdown();
        return 2;
    }

    bgfx::setViewRect(kViewBlit, 0, 0, kW, kH);
    bgfx::setViewClear(kViewBlit, BGFX_CLEAR_NONE);

    gfx_bgfx_set_backbuffer(g_target, kW, kH);
    gfx_context_init();
    gfx_resize(kW, kH);

    if (!gfx_healthy()) {
        printf("bgfx_backend_test: backend failed to initialise\n");
        bgfx::destroy(g_readback);
        bgfx::destroy(g_target);
        bgfx_bridge_shutdown();
        return 2;
    }

    testClearAndOrientation();
    testDepthRange();
    testBlendModes();
    testWideLinesAndPoints();
    testTextures();
    testClientArrays();
    testGlowAlignment();
    testGlowOffIsUnchanged();

    gfx_context_shutdown();
    bgfx::destroy(g_readback);
    bgfx::destroy(g_target);
    bgfx_bridge_shutdown();

    printf("bgfx_backend_test: %d/%d checks passed\n", g_checks - g_failures, g_checks);
    return g_failures == 0 ? 0 : 1;
}
