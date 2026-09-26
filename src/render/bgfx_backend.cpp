// ---------------------------------------------------------------------------
// bgfx_backend.cpp — bgfx render backend for Trigonometry Wars
//
// This is the bgfx sibling of gl3.cpp: it implements exactly the same
// fixed-function-compatible API (see gl3.h) that all the game code draws
// through, but it renders with **bgfx** instead of raw OpenGL.
//
// On macOS that means the game runs on Metal: bgfx has no OpenGL backend on
// Apple platforms any more (upstream removed it), so Metal is the only way to
// hand presentation to bgfx — which is the whole point of the `USE_BGFX=1`
// build.
//
// The strategy mirrors gl3.cpp one-to-one:
//   * software matrix stacks + CPU vertex transform into clip space,
//   * wide lines / points expanded to screen-space triangles,
//   * geometry accumulated in per-state batches and drawn from transient
//     vertex buffers (one bgfx draw call per state change instead of one per
//     legacy glBegin/glEnd pair),
//   * the glow/blur post-process runs in bgfx render targets with the same
//     separable Gaussian kernel as the OpenGL backend.
//
// Backend conventions handled here (verified against the bgfx Metal backend by
// tools/bgfx_backend_test.cpp):
//   * clip space y is "up" for the swap chain and for render targets alike, so
//     the scene geometry needs no flip,
//   * Metal's NDC depth range is [0,1] while the legacy ortho/perspective
//     matrices produce [-1,1]; the depth is remapped on the CPU when the
//     backend reports !homogeneousDepth (see fixDepth()),
//   * render-target textures store NDC y=+1 in row 0 (the opposite of OpenGL),
//     so the blur and composite passes flip V while sampling (see
//     g_flipTexelY).
// ---------------------------------------------------------------------------

#include "render/gl3.h"

#include <bgfx/bgfx.h>

#include "render/shaders/fs_blur.bin.h"
#include "render/shaders/fs_solid.bin.h"
#include "render/shaders/fs_tex.bin.h"
#include "render/shaders/vs_blur.bin.h"
#include "render/shaders/vs_solid.bin.h"
#include "render/shaders/vs_tex.bin.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

// ===========================================================================
// Views
//
// bgfx executes views in ascending order, which also orders the render passes:
// the glow pass has to run before the blur chain, and the composite last.
// ===========================================================================
namespace {

enum : bgfx::ViewId {
    VIEW_GLOW = 0,       // scene rendered into the (low-res) glow target
    VIEW_BLUR_H0 = 1,    // glow -> ping (horizontal)
    VIEW_BLUR_V0 = 2,    // ping -> glow (vertical)
    VIEW_BLUR_H1 = 3,    // glow -> ping (horizontal, 2nd iteration)
    VIEW_BLUR_V1 = 4,    // ping -> glow (vertical, 2nd iteration)
    VIEW_SCENE = 5,      // scene rendered to the back buffer
    VIEW_COMPOSITE = 6,  // glow added on top of the back buffer
};

} // namespace

// ===========================================================================
// Software matrix stacks (column-major 4x4, matches the GL layout)
// ===========================================================================
namespace {

constexpr int MAX_STACK = 16;
constexpr int MAT4 = 16;

struct MatrixStack
{
    float s[MAX_STACK][MAT4] {};
    int sp { 0 };

    MatrixStack() { loadIdentityAt(0); }

    void loadIdentityAt(int i)
    {
        for (int k = 0; k < MAT4; ++k)
            s[i][k] = 0.0f;
        s[i][0] = s[i][5] = s[i][10] = s[i][15] = 1.0f;
    }
    void push()
    {
        if (sp < MAX_STACK - 1) {
            memcpy(s[sp + 1], s[sp], sizeof(s[sp]));
            ++sp;
        }
    }
    void pop()
    {
        if (sp > 0)
            --sp;
    }
};

MatrixStack g_projStack;
MatrixStack g_mvStack;
GLenum g_matrixMode = GL_MODELVIEW;

MatrixStack& curStack()
{
    return g_matrixMode == GL_PROJECTION ? g_projStack : g_mvStack;
}

void matMul(const float a[16], const float b[16], float out[16])
{
    float tmp[16];
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k)
                sum += a[k * 4 + r] * b[c * 4 + k];
            tmp[c * 4 + r] = sum;
        }
    }
    memcpy(out, tmp, sizeof(tmp));
}

// combined = proj * modelview
void combinedMatrix(float out[16])
{
    matMul(g_projStack.s[g_projStack.sp], g_mvStack.s[g_mvStack.sp], out);
}

// out = m * in  (in/out are 4-component, column vector)
void matTransform(const float m[16], const float in[4], float out[4])
{
    for (int r = 0; r < 4; ++r) {
        out[r] = m[0 * 4 + r] * in[0] + m[1 * 4 + r] * in[1] + m[2 * 4 + r] * in[2]
            + m[3 * 4 + r] * in[3];
    }
}

} // namespace

// ===========================================================================
// Immediate-mode style state
// ===========================================================================
namespace {

// Current render target (draw calls are submitted to this view).
bgfx::ViewId g_targetView = VIEW_SCENE;

struct ImmVertex
{
    float x, y, z;
    float r, g, b, a;
    float u, v;
    bool hasUV { false };
};

std::vector<ImmVertex> g_imm;
GLenum g_beginMode = 0;
bool g_insideBegin = false;

float g_curColor[4] = { 1, 1, 1, 1 };
float g_curUV[2] = { 0, 0 };
bool g_curHasUV = false;
float g_lineWidth = 1.0f;
float g_pointSize = 1.0f;

bool g_textureEnabled = false; // emulated GL_TEXTURE_2D enable
GLuint g_boundTexture = 0;     // virtual texture id bound to unit 0

GLint g_viewport[4] = { 0, 0, 800, 600 };
bool g_blendEnabled = false;
GLenum g_blendSrc = GL_SRC_ALPHA;
GLenum g_blendDst = GL_ONE;
bool g_depthTestEnabled = false;

float g_clearColor[4] = { 0, 0, 0, 1 };

// Client arrays (grid)
bool g_clientVertex = false;
bool g_clientColor = false;
int g_clientVertexSize = 0;
GLenum g_clientVertexType = 0;
GLsizei g_clientVertexStride = 0;
const GLvoid* g_clientVertexPtr = nullptr;
int g_clientColorSize = 0;
GLenum g_clientColorType = 0;
GLsizei g_clientColorStride = 0;
const GLvoid* g_clientColorPtr = nullptr;

bool g_loaded = false;

// Backend conventions reported by bgfx (see the file header).
bool g_depthZeroToOne = false;
bool g_flipTexelY = false;

} // namespace

// ===========================================================================
// GPU resources
// ===========================================================================
namespace {

// Packed vertex layouts (identical to the OpenGL backend's).
struct SolidVert
{
    float x, y, z, w;
    float r, g, b, a;
};
struct TexVert
{
    float x, y, z, w;
    float r, g, b, a;
    float u, v;
};

bgfx::VertexLayout g_solidLayout;
bgfx::VertexLayout g_texLayout;
bgfx::VertexLayout g_blurLayout;

bgfx::ProgramHandle g_solidProg = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle g_texProg = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle g_blurProg = BGFX_INVALID_HANDLE;
bgfx::UniformHandle g_texSampler = BGFX_INVALID_HANDLE;
bgfx::UniformHandle g_blurParams = BGFX_INVALID_HANDLE;
bgfx::UniformHandle g_uvScale = BGFX_INVALID_HANDLE;

// Virtual texture registry (slot 0 == "no texture bound", as in gl3.cpp).
// gl3.h only declares the constants the OpenGL backend needed, so the GL
// default wrap mode is spelled out here (GL_REPEAT).
constexpr GLint kWrapRepeat = 0x2901;

struct TexRec
{
    bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
    int w { 0 };
    int h { 0 };
    bool valid { false };
    GLint minFilter { GL_LINEAR };
    GLint magFilter { GL_LINEAR };
    GLint wrapS { kWrapRepeat };
    GLint wrapT { kWrapRepeat };
};
std::vector<TexRec> g_tex;

// Glow / blur targets
bgfx::FrameBufferHandle g_fbGlow = BGFX_INVALID_HANDLE;
bgfx::FrameBufferHandle g_fbPing = BGFX_INVALID_HANDLE;
int g_glowW { 1 }, g_glowH { 1 };
int g_fbW { 800 }, g_fbH { 600 };
bool g_glowEnabled = true;
const int g_glowScale = 2; // glow texture = window / scale

// Optional offscreen replacement for the swap chain (headless verification,
// see tools/bgfx_backend_test.cpp).
bgfx::FrameBufferHandle g_backbuffer = BGFX_INVALID_HANDLE;

// bgfx sampler flags for a texture registry entry.  Linear filtering is the
// default, so only the "point" cases need flags.
uint64_t samplerFlags(const TexRec& rec)
{
    uint64_t flags = 0;
    if (rec.minFilter == GL_NEAREST)
        flags |= BGFX_SAMPLER_MIN_POINT;
    if (rec.magFilter == GL_NEAREST)
        flags |= BGFX_SAMPLER_MAG_POINT;
    if (rec.wrapS == GL_CLAMP_TO_EDGE)
        flags |= BGFX_SAMPLER_U_CLAMP;
    if (rec.wrapT == GL_CLAMP_TO_EDGE)
        flags |= BGFX_SAMPLER_V_CLAMP;
    return flags;
}

uint64_t blendFactor(GLenum f)
{
    switch (f) {
    case GL_ZERO:
        return BGFX_STATE_BLEND_ZERO;
    case GL_ONE:
        return BGFX_STATE_BLEND_ONE;
    case GL_SRC_COLOR:
        return BGFX_STATE_BLEND_SRC_COLOR;
    case GL_ONE_MINUS_SRC_COLOR:
        return BGFX_STATE_BLEND_INV_SRC_COLOR;
    case GL_SRC_ALPHA:
        return BGFX_STATE_BLEND_SRC_ALPHA;
    case GL_ONE_MINUS_SRC_ALPHA:
        return BGFX_STATE_BLEND_INV_SRC_ALPHA;
    case GL_DST_ALPHA:
        return BGFX_STATE_BLEND_DST_ALPHA;
    case GL_ONE_MINUS_DST_ALPHA:
        return BGFX_STATE_BLEND_INV_DST_ALPHA;
    case GL_DST_COLOR:
        return BGFX_STATE_BLEND_DST_COLOR;
    case GL_ONE_MINUS_DST_COLOR:
        return BGFX_STATE_BLEND_INV_DST_COLOR;
    default:
        return BGFX_STATE_BLEND_ONE;
    }
}

uint64_t currentState()
{
    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A;
    if (g_blendEnabled)
        state |= BGFX_STATE_BLEND_FUNC(blendFactor(g_blendSrc), blendFactor(g_blendDst));
    return state;
}

// Legacy matrices produce OpenGL-style NDC depth ([-1,1]); backends that want
// [0,1] need it remapped (Metal/Vulkan/Direct3D).
float fixDepth(float z, float w)
{
    return g_depthZeroToOne ? (z * 0.5f + w * 0.5f) : z;
}

template <typename V>
void submitVertices(bgfx::ViewId view, bgfx::ProgramHandle prog, const bgfx::VertexLayout& layout,
                    const V* verts, uint32_t count, uint64_t state, bgfx::TextureHandle tex,
                    uint64_t texFlags, bool textured)
{
    while (count > 0) {
        const uint32_t avail = bgfx::getAvailTransientVertexBuffer(count, layout);
        if (avail == 0)
            return; // out of transient memory this frame — drop the rest

        const uint32_t n = avail < count ? avail : count;

        bgfx::TransientVertexBuffer tvb;
        bgfx::allocTransientVertexBuffer(&tvb, n, layout);
        memcpy(tvb.data, verts, static_cast<size_t>(n) * sizeof(V));

        bgfx::setVertexBuffer(0, &tvb);
        if (textured)
            bgfx::setTexture(0, g_texSampler, tex, static_cast<uint32_t>(texFlags));
        bgfx::setState(state);
        bgfx::submit(view, prog);

        verts += n;
        count -= n;
    }
}

} // namespace

// ===========================================================================
// Batching: geometry accumulates until the draw state changes, then it is
// submitted from a transient vertex buffer.  This replaces the one-draw-call-
// per-glBegin behaviour of the OpenGL backend with a handful of draw calls per
// frame.
// ===========================================================================
namespace {

enum class BatchKind { None, Solid, Tex };

struct Batch
{
    BatchKind kind { BatchKind::None };
    bgfx::ViewId view { VIEW_SCENE };
    GLuint texture { 0 };
    bool blendEnabled { false };
    GLenum blendSrc { GL_SRC_ALPHA };
    GLenum blendDst { GL_ONE };
    // The bgfx state this geometry was accumulated under.  It has to be
    // captured when the batch *starts*, because the legacy code sets the blend
    // mode *before* the primitives that use it: by the time a later primitive
    // flushes this batch, the current GL state already belongs to that next
    // draw (see beginBatch()).
    uint64_t state { 0 };
    std::vector<SolidVert> solid;
    std::vector<TexVert> tex;
};

Batch g_batch;

void flushBatch()
{
    if (!g_loaded)
        return;

    // The state the batch was built with -- not currentState(), which by now
    // describes the draw that triggered this flush.  Submitting with the wrong
    // state silently changes how the geometry blends: an attract-mode scene
    // drawn additively was then multiplied by the overlay quad's
    // DST_COLOR/ONE_MINUS_SRC_ALPHA and came out black.
    const uint64_t state = g_batch.state;

    if (!g_batch.solid.empty()) {
        submitVertices(g_batch.view, g_solidProg, g_solidLayout, g_batch.solid.data(),
                       static_cast<uint32_t>(g_batch.solid.size()), state, BGFX_INVALID_HANDLE, 0, false);
        g_batch.solid.clear();
    }

    if (!g_batch.tex.empty()) {
        // Same as gl3: geometry with no (valid) texture bound is dropped.
        const GLuint id = g_batch.texture;
        if (id != 0 && id < g_tex.size() && g_tex[id].valid && bgfx::isValid(g_tex[id].handle)) {
            const TexRec& rec = g_tex[id];
            submitVertices(g_batch.view, g_texProg, g_texLayout, g_batch.tex.data(),
                           static_cast<uint32_t>(g_batch.tex.size()), state, rec.handle,
                           samplerFlags(rec), true);
        }
        g_batch.tex.clear();
    }

    g_batch.kind = BatchKind::None;
}

// Called before appending geometry; flushes if any draw state changed.
void beginBatch(BatchKind kind, GLuint texture)
{
    if (g_batch.kind == kind && g_batch.texture == texture && g_batch.view == g_targetView
        && g_batch.blendEnabled == g_blendEnabled && g_batch.blendSrc == g_blendSrc
        && g_batch.blendDst == g_blendDst)
        return;

    flushBatch();
    g_batch.kind = kind;
    g_batch.texture = texture;
    g_batch.view = g_targetView;
    g_batch.blendEnabled = g_blendEnabled;
    g_batch.blendSrc = g_blendSrc;
    g_batch.blendDst = g_blendDst;
    g_batch.state = currentState();
}

} // namespace

// ===========================================================================
// Vertex / primitive assembly (identical maths to the OpenGL backend)
// ===========================================================================
namespace {

struct ClipV
{
    float x, y, z, w;
    float r, g, b, a;
    float u, v;
    bool hasUV;
};

void projectImmediate(const float m[16], std::vector<ClipV>& out)
{
    out.clear();
    out.reserve(g_imm.size());
    for (const auto& iv : g_imm) {
        const float in[4] = { iv.x, iv.y, iv.z, 1.0f };
        float c[4];
        matTransform(m, in, c);
        ClipV cv;
        cv.x = c[0];
        cv.y = c[1];
        cv.z = fixDepth(c[2], c[3]);
        cv.w = c[3];
        cv.r = iv.r;
        cv.g = iv.g;
        cv.b = iv.b;
        cv.a = iv.a;
        cv.u = iv.u;
        cv.v = iv.v;
        cv.hasUV = iv.hasUV;
        out.push_back(cv);
    }
}

// Convert NDC to pixel coordinates (window origin bottom-left).
void ndcToPx(const ClipV& c, float& px, float& py)
{
    const float invw = (c.w != 0.0f) ? (1.0f / c.w) : 0.0f;
    px = (c.x * invw * 0.5f + 0.5f) * static_cast<float>(g_viewport[2]);
    py = (c.y * invw * 0.5f + 0.5f) * static_cast<float>(g_viewport[3]);
}

void pxToNdc(float px, float py, float out[4])
{
    out[0] = (px / static_cast<float>(g_viewport[2])) * 2.0f - 1.0f;
    out[1] = (py / static_cast<float>(g_viewport[3])) * 2.0f - 1.0f;
    out[2] = fixDepth(0.0f, 1.0f);
    out[3] = 1.0f;
}

void pushSolid(float x, float y, float z, float w, float r, float g, float b, float a)
{
    beginBatch(BatchKind::Solid, 0);
    g_batch.solid.push_back(SolidVert { x, y, z, w, r, g, b, a });
}

void pushTex(float x, float y, float z, float w, float r, float g, float b, float a, float u, float v)
{
    beginBatch(BatchKind::Tex, g_boundTexture);
    g_batch.tex.push_back(TexVert { x, y, z, w, r, g, b, a, u, v });
}

void pushSolidClipPx(const ClipV& a, float px, float py)
{
    float ndc[4];
    pxToNdc(px, py, ndc);
    pushSolid(ndc[0], ndc[1], ndc[2], ndc[3], a.r, a.g, a.b, a.a);
}

// Emit a thick screen-space quad for one line segment (a->b), width in px.
void emitLineQuad(const ClipV& a, const ClipV& b)
{
    if (a.w <= 0.0f || b.w <= 0.0f)
        return;
    float ax, ay, bx, by;
    ndcToPx(a, ax, ay);
    ndcToPx(b, bx, by);

    float dx = bx - ax;
    float dy = by - ay;
    const float len = std::sqrt(dx * dx + dy * dy);
    float width = g_lineWidth;
    if (width < 1.0f)
        width = 1.0f;
    const float half = width * 0.5f;
    if (len < 1e-4f) {
        pushSolidClipPx(a, ax - half, ay - half);
        pushSolidClipPx(a, ax + half, ay - half);
        pushSolidClipPx(a, ax + half, ay + half);
        pushSolidClipPx(a, ax - half, ay - half);
        pushSolidClipPx(a, ax + half, ay + half);
        pushSolidClipPx(a, ax - half, ay + half);
        return;
    }

    const float nx = -dy / len * half;
    const float ny = dx / len * half;

    pushSolidClipPx(a, ax + nx, ay + ny);
    pushSolidClipPx(a, ax - nx, ay - ny);
    pushSolidClipPx(b, bx + nx, by + ny);

    pushSolidClipPx(a, ax - nx, ay - ny);
    pushSolidClipPx(b, bx - nx, by - ny);
    pushSolidClipPx(b, bx + nx, by + ny);
}

// Emit a screen-space square for a point, size in px.
void emitPointQuad(const ClipV& c)
{
    if (c.w <= 0.0f)
        return;
    float cx, cy;
    ndcToPx(c, cx, cy);
    float size = g_pointSize;
    if (size < 1.0f)
        size = 1.0f;
    const float half = size * 0.5f;
    pushSolidClipPx(c, cx - half, cy - half);
    pushSolidClipPx(c, cx + half, cy - half);
    pushSolidClipPx(c, cx + half, cy + half);
    pushSolidClipPx(c, cx - half, cy - half);
    pushSolidClipPx(c, cx + half, cy + half);
    pushSolidClipPx(c, cx - half, cy + half);
}

// glClear() is emulated with a fullscreen quad so that it keeps its place in
// the draw order and works for the back buffer *and* render targets (bgfx can
// only clear at the start of a view).
void emitClearQuad()
{
    SolidVert quad[6];
    const float z = fixDepth(0.0f, 1.0f);
    static const float corners[4][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
    static const int order[6] = { 0, 1, 2, 0, 2, 3 };
    for (int i = 0; i < 6; ++i) {
        const float* c = corners[order[i]];
        quad[i] = SolidVert { c[0], c[1], z, 1.0f, g_clearColor[0], g_clearColor[1], g_clearColor[2],
                              g_clearColor[3] };
    }

    flushBatch();
    submitVertices(g_targetView, g_solidProg, g_solidLayout, quad, 6,
                   BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A, BGFX_INVALID_HANDLE, 0, false);
}

// Fullscreen triangle used by the glow/blur passes (a_position is clip space).
// uvScale.xy maps clip space to texture coordinates, including the V flip that
// render-target textures need on Metal/Direct3D (see g_flipTexelY).
void submitFullscreenTriangle(bgfx::ViewId view, bgfx::TextureHandle tex, const float blurParams[4])
{
    const float z = fixDepth(0.0f, 1.0f);
    const float tri[3][4] = {
        { -1.0f, -1.0f, z, 1.0f },
        { 3.0f, -1.0f, z, 1.0f },
        { -1.0f, 3.0f, z, 1.0f },
    };

    bgfx::TransientVertexBuffer tvb;
    bgfx::allocTransientVertexBuffer(&tvb, 3, g_blurLayout);
    memcpy(tvb.data, tri, sizeof(tri));

    const float uvScale[4] = { 0.5f, g_flipTexelY ? -0.5f : 0.5f, 0.0f, 0.0f };

    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setTexture(0, g_texSampler, tex, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
    if (bgfx::isValid(g_blurParams))
        bgfx::setUniform(g_blurParams, blurParams);
    if (bgfx::isValid(g_uvScale))
        bgfx::setUniform(g_uvScale, uvScale);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::submit(view, g_blurProg);
}

} // namespace

// ===========================================================================
// Fixed-function compatible API
// ===========================================================================
extern "C" {

void gfx_begin(GLenum mode)
{
    g_imm.clear();
    g_beginMode = mode;
    g_insideBegin = true;
    g_curHasUV = false;
}

void gfx_end()
{
    if (!g_insideBegin)
        return;
    g_insideBegin = false;

    float m[16];
    combinedMatrix(m);

    std::vector<ClipV> clip;
    projectImmediate(m, clip);
    const std::size_t n = clip.size();

    if (g_beginMode == GL_POINTS) {
        for (std::size_t i = 0; i < n; ++i)
            emitPointQuad(clip[i]);
    } else if (g_beginMode == GL_LINES) {
        for (std::size_t i = 0; i + 1 < n; i += 2)
            emitLineQuad(clip[i], clip[i + 1]);
    } else if (g_beginMode == GL_LINE_STRIP) {
        for (std::size_t i = 0; i + 1 < n; ++i)
            emitLineQuad(clip[i], clip[i + 1]);
    } else if (g_beginMode == GL_LINE_LOOP) {
        for (std::size_t i = 0; i + 1 < n; ++i)
            emitLineQuad(clip[i], clip[i + 1]);
        if (n >= 2)
            emitLineQuad(clip[n - 1], clip[0]);
    } else {
        // Filled polygons: emitted as clip-space triangles.
        const bool textured = g_textureEnabled && (g_boundTexture != 0);
        auto emitTri = [&](std::size_t i0, std::size_t i1, std::size_t i2) {
            const ClipV& a = clip[i0];
            const ClipV& b = clip[i1];
            const ClipV& c = clip[i2];
            if (textured) {
                pushTex(a.x, a.y, a.z, a.w, a.r, a.g, a.b, a.a, a.u, a.v);
                pushTex(b.x, b.y, b.z, b.w, b.r, b.g, b.b, b.a, b.u, b.v);
                pushTex(c.x, c.y, c.z, c.w, c.r, c.g, c.b, c.a, c.u, c.v);
            } else {
                pushSolid(a.x, a.y, a.z, a.w, a.r, a.g, a.b, a.a);
                pushSolid(b.x, b.y, b.z, b.w, b.r, b.g, b.b, b.a);
                pushSolid(c.x, c.y, c.z, c.w, c.r, c.g, c.b, c.a);
            }
        };

        if (g_beginMode == GL_TRIANGLES) {
            for (std::size_t i = 0; i + 2 < n; i += 3)
                emitTri(i, i + 1, i + 2);
        } else if (g_beginMode == GL_TRIANGLE_STRIP) {
            for (std::size_t i = 0; i + 2 < n; ++i) {
                if (i & 1)
                    emitTri(i + 1, i, i + 2);
                else
                    emitTri(i, i + 1, i + 2);
            }
        } else if (g_beginMode == GL_TRIANGLE_FAN) {
            for (std::size_t i = 1; i + 1 < n; ++i)
                emitTri(0, i, i + 1);
        } else { // GL_QUADS (and any other polygon-ish mode)
            for (std::size_t i = 0; i + 3 < n; i += 4) {
                emitTri(i, i + 1, i + 2);
                emitTri(i, i + 2, i + 3);
            }
        }
    }

    g_imm.clear();
}

void gfx_color4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    g_curColor[0] = r;
    g_curColor[1] = g;
    g_curColor[2] = b;
    g_curColor[3] = a;
}

void gfx_vertex2d(GLdouble x, GLdouble y)
{
    gfx_vertex3d(x, y, 0.0);
}

void gfx_vertex3d(GLdouble x, GLdouble y, GLdouble z)
{
    if (!g_insideBegin)
        return;
    ImmVertex iv;
    iv.x = static_cast<float>(x);
    iv.y = static_cast<float>(y);
    iv.z = static_cast<float>(z);
    iv.r = g_curColor[0];
    iv.g = g_curColor[1];
    iv.b = g_curColor[2];
    iv.a = g_curColor[3];
    iv.u = g_curUV[0];
    iv.v = g_curUV[1];
    iv.hasUV = g_curHasUV;
    g_imm.push_back(iv);
}

void gfx_vertex3f(GLfloat x, GLfloat y, GLfloat z)
{
    gfx_vertex3d(x, y, z);
}

void gfx_texcoord2d(GLdouble s, GLdouble t)
{
    g_curUV[0] = static_cast<float>(s);
    g_curUV[1] = static_cast<float>(t);
    g_curHasUV = true;
}

void gfx_texcoord2f(GLfloat s, GLfloat t)
{
    gfx_texcoord2d(s, t);
}

void gfx_linewidth(GLfloat width)
{
    g_lineWidth = width;
}

void gfx_pointsize(GLfloat size)
{
    g_pointSize = size;
}

void gfx_enable(GLenum cap)
{
    switch (cap) {
    case GL_TEXTURE_2D:
        g_textureEnabled = true;
        break;
    case GL_BLEND:
        g_blendEnabled = true;
        break;
    case GL_DEPTH_TEST:
        g_depthTestEnabled = true;
        break;
    default:
        break;
    }
}

void gfx_disable(GLenum cap)
{
    switch (cap) {
    case GL_TEXTURE_2D:
        g_textureEnabled = false;
        break;
    case GL_BLEND:
        g_blendEnabled = false;
        break;
    case GL_DEPTH_TEST:
        g_depthTestEnabled = false;
        break;
    default:
        break;
    }
}

void gfx_blendfunc(GLenum src, GLenum dst)
{
    g_blendSrc = src;
    g_blendDst = dst;
}

void gfx_clearcolor(GLclampf r, GLclampf g, GLclampf b, GLclampf a)
{
    g_clearColor[0] = r;
    g_clearColor[1] = g;
    g_clearColor[2] = b;
    g_clearColor[3] = a;
}

void gfx_clear(GLbitfield /*mask*/)
{
    if (!g_loaded)
        return;
    emitClearQuad();
}

void gfx_viewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    g_viewport[0] = x;
    g_viewport[1] = y;
    g_viewport[2] = width;
    g_viewport[3] = height;
}

void gfx_getintegerv(GLenum pname, GLint* params)
{
    if (pname == GL_VIEWPORT) {
        params[0] = g_viewport[0];
        params[1] = g_viewport[1];
        params[2] = g_viewport[2];
        params[3] = g_viewport[3];
    }
}

void gfx_matrixmode(GLenum mode)
{
    if (mode == GL_PROJECTION || mode == GL_MODELVIEW)
        g_matrixMode = mode;
}

void gfx_pushmatrix()
{
    curStack().push();
}

void gfx_popmatrix()
{
    curStack().pop();
}

void gfx_loadidentity()
{
    curStack().loadIdentityAt(curStack().sp);
}

void gfx_ortho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
    float m[16] {};
    const float rl = static_cast<float>(right - left);
    const float tb = static_cast<float>(top - bottom);
    const float fn = static_cast<float>(zFar - zNear);
    m[0] = 2.0f / rl;
    m[5] = 2.0f / tb;
    m[10] = -2.0f / fn;
    m[12] = static_cast<float>(-(right + left) / rl);
    m[13] = static_cast<float>(-(top + bottom) / tb);
    m[14] = static_cast<float>(-(zFar + zNear) / fn);
    m[15] = 1.0f;
    memcpy(curStack().s[curStack().sp], m, sizeof(m));
}

void gfx_translatef(GLfloat x, GLfloat y, GLfloat z)
{
    float t[16] {};
    t[0] = t[5] = t[10] = t[15] = 1.0f;
    t[12] = x;
    t[13] = y;
    t[14] = z;
    float out[16];
    matMul(curStack().s[curStack().sp], t, out);
    memcpy(curStack().s[curStack().sp], out, sizeof(out));
}

void gfx_perspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar)
{
    const float f = static_cast<float>(1.0 / std::tan(fovy * 0.5 * (3.14159265358979323846 / 180.0)));
    float m[16] {};
    m[0] = f / static_cast<float>(aspect);
    m[5] = f;
    m[10] = static_cast<float>((zFar + zNear) / (zNear - zFar));
    m[11] = -1.0f;
    m[14] = static_cast<float>((2.0 * zFar * zNear) / (zNear - zFar));
    m[15] = 0.0f;
    memcpy(curStack().s[curStack().sp], m, sizeof(m));
}

void gfx_texenvf(GLenum /*target*/, GLenum /*pname*/, GLfloat /*param*/)
{
    // Modulate is hardwired in fs_tex.sc, exactly like the OpenGL backend.
}

// ---- texture object management -------------------------------------------

namespace {

// Upload RGBA8 pixels into a registry slot, (re)creating the bgfx texture.
void uploadTexture(GLuint id, int width, int height, const void* pixels)
{
    if (id == 0 || id >= g_tex.size() || pixels == nullptr)
        return;
    TexRec& rec = g_tex[id];

    if (bgfx::isValid(rec.handle)) {
        bgfx::destroy(rec.handle);
        rec.handle = BGFX_INVALID_HANDLE;
    }

    const uint32_t size = static_cast<uint32_t>(width) * static_cast<uint32_t>(height) * 4u;
    rec.handle = bgfx::createTexture2D(static_cast<uint16_t>(width), static_cast<uint16_t>(height), false, 1,
                                       bgfx::TextureFormat::RGBA8, samplerFlags(rec),
                                       bgfx::copy(pixels, size));
    rec.w = width;
    rec.h = height;
}

} // namespace

void gfx_gentextures(GLsizei n, GLuint* textures)
{
    // Slot 0 is reserved ("no texture bound"), so ids start at 1.
    if (g_tex.empty())
        g_tex.push_back(TexRec {});

    for (GLsizei i = 0; i < n; ++i) {
        GLuint slot = 0;
        for (std::size_t k = 1; k < g_tex.size(); ++k) {
            if (!g_tex[k].valid) {
                slot = static_cast<GLuint>(k);
                break;
            }
        }
        if (slot == 0) {
            slot = static_cast<GLuint>(g_tex.size());
            g_tex.push_back(TexRec {});
        } else if (bgfx::isValid(g_tex[slot].handle)) {
            bgfx::destroy(g_tex[slot].handle);
        }
        g_tex[slot] = TexRec {}; // fresh entry with GL's default sampler state
        g_tex[slot].valid = true;
        textures[i] = slot;
    }
}

void gfx_bindtexture(GLenum target, GLuint texture)
{
    if (target != GL_TEXTURE_2D)
        return;
    g_boundTexture = texture;
}

void gfx_teximage2d(GLenum target, GLint level, GLint /*internalformat*/, GLsizei width, GLsizei height,
                    GLint /*border*/, GLenum format, GLenum type, const GLvoid* pixels)
{
    if (target != GL_TEXTURE_2D || g_boundTexture == 0 || level != 0 || !g_loaded)
        return;
    if (pixels == nullptr || type != GL_UNSIGNED_BYTE || width <= 0 || height <= 0)
        return;

    // The game only uploads RGBA8 (lodepng output); expand RGB8 if asked.
    if (format == GL_RGBA) {
        uploadTexture(g_boundTexture, width, height, pixels);
    } else if (format == GL_RGB) {
        const uint8_t* src = static_cast<const uint8_t*>(pixels);
        std::vector<uint8_t> rgba(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
        for (std::size_t i = 0, o = 0; i < rgba.size(); i += 4, o += 3) {
            rgba[i + 0] = src[o + 0];
            rgba[i + 1] = src[o + 1];
            rgba[i + 2] = src[o + 2];
            rgba[i + 3] = 255;
        }
        uploadTexture(g_boundTexture, width, height, rgba.data());
    }
}

void gfx_texsubimage2d(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                       GLenum /*format*/, GLenum /*type*/, const GLvoid* pixels)
{
    if (target != GL_TEXTURE_2D || g_boundTexture == 0 || level != 0 || !g_loaded)
        return;
    if (g_boundTexture >= g_tex.size() || !bgfx::isValid(g_tex[g_boundTexture].handle) || pixels == nullptr)
        return;

    const uint32_t size = static_cast<uint32_t>(width) * static_cast<uint32_t>(height) * 4u;
    bgfx::updateTexture2D(g_tex[g_boundTexture].handle, 0, 0, static_cast<uint16_t>(xoffset),
                          static_cast<uint16_t>(yoffset), static_cast<uint16_t>(width),
                          static_cast<uint16_t>(height), bgfx::copy(pixels, size));
}

void gfx_texparameteri(GLenum target, GLenum pname, GLint param)
{
    if (target != GL_TEXTURE_2D || g_boundTexture == 0 || g_boundTexture >= g_tex.size())
        return;
    TexRec& rec = g_tex[g_boundTexture];
    switch (pname) {
    case GL_TEXTURE_MIN_FILTER:
        rec.minFilter = param;
        break;
    case GL_TEXTURE_MAG_FILTER:
        rec.magFilter = param;
        break;
    case GL_TEXTURE_WRAP_S:
        rec.wrapS = param;
        break;
    case GL_TEXTURE_WRAP_T:
        rec.wrapT = param;
        break;
    default:
        break;
    }
    // Sampler flags are passed per draw (see flushBatch), so no recreation.
}

void gfx_texparameterf(GLenum target, GLenum pname, GLfloat param)
{
    gfx_texparameteri(target, pname, static_cast<GLint>(param));
}

void gfx_build2dmipmaps(GLenum target, GLint internalformat, GLsizei width, GLsizei height, GLenum format,
                        GLenum type, const GLvoid* pixels)
{
    // bgfx does not generate mip chains; the game's textures are sampled with
    // plain linear filtering anyway (see texture.cpp).
    gfx_teximage2d(target, 0, internalformat, width, height, 0, format, type, pixels);
}

// ---- client arrays (used by the grid) ------------------------------------

void gfx_enableclientstate(GLenum array)
{
    if (array == GL_VERTEX_ARRAY)
        g_clientVertex = true;
    else if (array == GL_COLOR_ARRAY)
        g_clientColor = true;
}

void gfx_disableclientstate(GLenum array)
{
    if (array == GL_VERTEX_ARRAY)
        g_clientVertex = false;
    else if (array == GL_COLOR_ARRAY)
        g_clientColor = false;
}

void gfx_vertexpointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer)
{
    g_clientVertexSize = size;
    g_clientVertexType = type;
    g_clientVertexStride = stride;
    g_clientVertexPtr = pointer;
}

void gfx_colorpointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer)
{
    g_clientColorSize = size;
    g_clientColorType = type;
    g_clientColorStride = stride;
    g_clientColorPtr = pointer;
}

void gfx_drawelements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices)
{
    if (!g_clientVertex || count <= 0)
        return;
    if (mode != GL_LINES && mode != GL_LINE_STRIP && mode != GL_LINE_LOOP)
        return;

    const bool hasColors = g_clientColor && g_clientColorPtr != nullptr;

    const auto fetchVertex = [&](std::size_t idx, ImmVertex& out) {
        const unsigned char* vp = static_cast<const unsigned char*>(g_clientVertexPtr);
        if (g_clientVertexStride == 0) {
            vp += idx * g_clientVertexSize * sizeof(GLfloat);
        } else {
            vp += idx * g_clientVertexStride;
        }
        const GLfloat* v = reinterpret_cast<const GLfloat*>(vp);
        out.x = v[0];
        out.y = (g_clientVertexSize > 1) ? v[1] : 0.0f;
        out.z = (g_clientVertexSize > 2) ? v[2] : 0.0f;

        out.r = g_curColor[0];
        out.g = g_curColor[1];
        out.b = g_curColor[2];
        out.a = g_curColor[3];

        if (hasColors) {
            const unsigned char* cp = static_cast<const unsigned char*>(g_clientColorPtr);
            if (g_clientColorStride == 0) {
                cp += idx * g_clientColorSize * sizeof(GLfloat);
            } else {
                cp += idx * g_clientColorStride;
            }
            const GLfloat* c = reinterpret_cast<const GLfloat*>(cp);
            out.r = c[0];
            out.g = (g_clientColorSize > 1) ? c[1] : out.g;
            out.b = (g_clientColorSize > 2) ? c[2] : out.b;
            out.a = (g_clientColorSize > 3) ? c[3] : 1.0f;
        }
        out.u = 0.0f;
        out.v = 0.0f;
        out.hasUV = false;
    };

    g_imm.clear();
    g_imm.reserve(count);

    if (type == GL_UNSIGNED_SHORT) {
        const GLushort* idx = static_cast<const GLushort*>(indices);
        for (GLsizei i = 0; i < count; ++i) {
            ImmVertex iv {};
            fetchVertex(idx[i], iv);
            g_imm.push_back(iv);
        }
    } else if (type == GL_UNSIGNED_INT) {
        const GLuint* idx = static_cast<const GLuint*>(indices);
        for (GLsizei i = 0; i < count; ++i) {
            ImmVertex iv {};
            fetchVertex(idx[i], iv);
            g_imm.push_back(iv);
        }
    } else {
        return;
    }

    // Reuse the same assembly as immediate mode.
    g_insideBegin = true;
    g_beginMode = mode;
    gfx_end();
    g_imm.clear();
}

} // extern "C"

// ===========================================================================
// Host app helpers: context, glow/blur targets
// ===========================================================================
namespace {

void destroyGlowTargets()
{
    if (bgfx::isValid(g_fbGlow)) {
        bgfx::destroy(g_fbGlow);
        g_fbGlow = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(g_fbPing)) {
        bgfx::destroy(g_fbPing);
        g_fbPing = BGFX_INVALID_HANDLE;
    }
}

void configureViews()
{
    // Scene view: the back buffer (or the offscreen replacement when headless).
    bgfx::setViewFrameBuffer(VIEW_SCENE, g_backbuffer);
    bgfx::setViewRect(VIEW_SCENE, 0, 0, static_cast<uint16_t>(g_fbW), static_cast<uint16_t>(g_fbH));
    bgfx::setViewClear(VIEW_SCENE, BGFX_CLEAR_NONE); // clears are emulated as quads
    bgfx::setViewMode(VIEW_SCENE, bgfx::ViewMode::Sequential);

    // The composite draws on top of the scene, so it must not clear.
    bgfx::setViewFrameBuffer(VIEW_COMPOSITE, g_backbuffer);
    bgfx::setViewRect(VIEW_COMPOSITE, 0, 0, static_cast<uint16_t>(g_fbW), static_cast<uint16_t>(g_fbH));
    bgfx::setViewClear(VIEW_COMPOSITE, BGFX_CLEAR_NONE);
    bgfx::setViewMode(VIEW_COMPOSITE, bgfx::ViewMode::Sequential);

    // Glow + blur views render into the low-resolution targets.
    const uint16_t gw = static_cast<uint16_t>(g_glowW);
    const uint16_t gh = static_cast<uint16_t>(g_glowH);
    const bgfx::ViewId glowViews[5] = { VIEW_GLOW, VIEW_BLUR_H0, VIEW_BLUR_V0, VIEW_BLUR_H1, VIEW_BLUR_V1 };
    for (bgfx::ViewId v : glowViews) {
        bgfx::setViewRect(v, 0, 0, gw, gh);
        bgfx::setViewClear(v, BGFX_CLEAR_NONE);
        bgfx::setViewMode(v, bgfx::ViewMode::Sequential);
    }
    bgfx::setViewFrameBuffer(VIEW_GLOW, g_fbGlow);
    bgfx::setViewFrameBuffer(VIEW_BLUR_H0, g_fbPing);
    bgfx::setViewFrameBuffer(VIEW_BLUR_V0, g_fbGlow);
    bgfx::setViewFrameBuffer(VIEW_BLUR_H1, g_fbPing);
    bgfx::setViewFrameBuffer(VIEW_BLUR_V1, g_fbGlow);
}

} // namespace

extern "C" {

void gfx_context_init()
{
    if (g_loaded)
        return;

    if (bgfx::getCaps() == nullptr) {
        printf("bgfx_backend: bgfx is not initialised\n");
        return;
    }

    const bgfx::Caps* caps = bgfx::getCaps();
    g_depthZeroToOne = !caps->homogeneousDepth;
    // Render targets store NDC y=+1 in row 0 when the backend's NDC origin is
    // at the *top* left (Metal/Direct3D viewport convention), which is the
    // opposite of OpenGL; the blur and composite passes have to flip V while
    // sampling them. Verified for Metal by tools/bgfx_backend_test.cpp.
    g_flipTexelY = !caps->originBottomLeft;

    g_solidLayout.begin()
        .add(bgfx::Attrib::Position, 4, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Float)
        .end();

    g_texLayout.begin()
        .add(bgfx::Attrib::Position, 4, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    g_blurLayout.begin()
        .add(bgfx::Attrib::Position, 4, bgfx::AttribType::Float)
        .end();

    const auto makeShader = [](const uint8_t* data, uint32_t size) {
        return bgfx::createShader(bgfx::copy(data, size));
    };

    bgfx::ShaderHandle vsSolid = makeShader(vs_solid, sizeof(vs_solid));
    bgfx::ShaderHandle fsSolid = makeShader(fs_solid, sizeof(fs_solid));
    bgfx::ShaderHandle vsTex = makeShader(vs_tex, sizeof(vs_tex));
    bgfx::ShaderHandle fsTex = makeShader(fs_tex, sizeof(fs_tex));
    bgfx::ShaderHandle vsBlur = makeShader(vs_blur, sizeof(vs_blur));
    bgfx::ShaderHandle fsBlur = makeShader(fs_blur, sizeof(fs_blur));

    if (!bgfx::isValid(vsSolid) || !bgfx::isValid(fsSolid) || !bgfx::isValid(vsTex)
        || !bgfx::isValid(fsTex) || !bgfx::isValid(vsBlur) || !bgfx::isValid(fsBlur)) {
        printf("bgfx_backend: shader creation failed\n");
        return;
    }

    g_solidProg = bgfx::createProgram(vsSolid, fsSolid, true);
    g_texProg = bgfx::createProgram(vsTex, fsTex, true);
    g_blurProg = bgfx::createProgram(vsBlur, fsBlur, true);

    g_texSampler = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);
    g_blurParams = bgfx::createUniform("u_blurParams", bgfx::UniformType::Vec4);
    g_uvScale = bgfx::createUniform("u_uvScale", bgfx::UniformType::Vec4);

    if (!bgfx::isValid(g_solidProg) || !bgfx::isValid(g_texProg) || !bgfx::isValid(g_blurProg)) {
        printf("bgfx_backend: program creation failed\n");
        return;
    }

    printf("bgfx_backend: %s backend | depth[0,1]=%d flipTexelY=%d\n",
           bgfx::getRendererName(bgfx::getRendererType()), g_depthZeroToOne ? 1 : 0, g_flipTexelY ? 1 : 0);

    g_loaded = true;

    // Initial state mirrors the legacy pipeline.
    g_blendEnabled = false;
    g_blendSrc = GL_SRC_ALPHA;
    g_blendDst = GL_ONE;
    g_depthTestEnabled = false;

    g_batch.solid.reserve(65536);
    g_batch.tex.reserve(4096);

    gfx_resize(g_fbW, g_fbH);
}

void gfx_context_shutdown()
{
    if (!g_loaded)
        return;

    flushBatch();
    destroyGlowTargets();

    for (TexRec& rec : g_tex) {
        if (bgfx::isValid(rec.handle))
            bgfx::destroy(rec.handle);
    }
    g_tex.clear();

    if (bgfx::isValid(g_solidProg))
        bgfx::destroy(g_solidProg);
    if (bgfx::isValid(g_texProg))
        bgfx::destroy(g_texProg);
    if (bgfx::isValid(g_blurProg))
        bgfx::destroy(g_blurProg);
    if (bgfx::isValid(g_texSampler))
        bgfx::destroy(g_texSampler);
    if (bgfx::isValid(g_blurParams))
        bgfx::destroy(g_blurParams);
    if (bgfx::isValid(g_uvScale))
        bgfx::destroy(g_uvScale);

    g_solidProg = g_texProg = g_blurProg = BGFX_INVALID_HANDLE;
    g_texSampler = g_blurParams = g_uvScale = BGFX_INVALID_HANDLE;
    g_loaded = false;
}

void gfx_resize(int width, int height)
{
    g_fbW = width;
    g_fbH = height;
    if (!g_loaded)
        return;

    const int gw = width / g_glowScale > 0 ? width / g_glowScale : 1;
    const int gh = height / g_glowScale > 0 ? height / g_glowScale : 1;

    flushBatch();
    destroyGlowTargets();

    g_glowW = gw;
    g_glowH = gh;

    g_fbGlow = bgfx::createFrameBuffer(static_cast<uint16_t>(gw), static_cast<uint16_t>(gh),
                                       bgfx::TextureFormat::RGBA8,
                                       BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
    g_fbPing = bgfx::createFrameBuffer(static_cast<uint16_t>(gw), static_cast<uint16_t>(gh),
                                       bgfx::TextureFormat::RGBA8,
                                       BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

    if (!bgfx::isValid(g_fbGlow) || !bgfx::isValid(g_fbPing))
        printf("bgfx_backend: glow render target creation failed (%dx%d)\n", gw, gh);

    configureViews();

    g_viewport[2] = g_fbW;
    g_viewport[3] = g_fbH;
}

void gfx_glow_bind()
{
    if (!g_loaded)
        return;
    flushBatch();
    g_targetView = VIEW_GLOW;
    g_viewport[2] = g_glowW;
    g_viewport[3] = g_glowH;
}

void gfx_glow_unbind()
{
    if (!g_loaded)
        return;
    flushBatch();
    g_targetView = VIEW_SCENE;
    g_viewport[2] = g_fbW;
    g_viewport[3] = g_fbH;
}

void gfx_blur_glow()
{
    if (!g_loaded || !bgfx::isValid(g_fbGlow) || !bgfx::isValid(g_fbPing))
        return;

    flushBatch();

    const float texelX = 1.0f / static_cast<float>(g_glowW);
    const float texelY = 1.0f / static_cast<float>(g_glowH);

    const bgfx::TextureHandle glowTex = bgfx::getTexture(g_fbGlow);
    const bgfx::TextureHandle pingTex = bgfx::getTexture(g_fbPing);

    const float horiz[4] = { 1.0f, 0.0f, texelX, texelY };
    const float vert[4] = { 0.0f, 1.0f, texelX, texelY };

    // Two horizontal+vertical iterations, matching the OpenGL backend.
    submitFullscreenTriangle(VIEW_BLUR_H0, glowTex, horiz);
    submitFullscreenTriangle(VIEW_BLUR_V0, pingTex, vert);
    submitFullscreenTriangle(VIEW_BLUR_H1, glowTex, horiz);
    submitFullscreenTriangle(VIEW_BLUR_V1, pingTex, vert);
}

void gfx_draw_blurred_glow(float alpha)
{
    if (!g_loaded || !bgfx::isValid(g_fbGlow))
        return;

    flushBatch();

    // Additive composite of the blurred glow over the whole screen.
    const float z = fixDepth(0.0f, 1.0f);
    const float v0 = g_flipTexelY ? 1.0f : 0.0f;
    const float v1 = g_flipTexelY ? 0.0f : 1.0f;
    TexVert quad[6] = {};
    quad[0] = { -1, -1, z, 1, 1, 1, 1, alpha, 0, v0 };
    quad[1] = { 1, -1, z, 1, 1, 1, 1, alpha, 1, v0 };
    quad[2] = { 1, 1, z, 1, 1, 1, 1, alpha, 1, v1 };
    quad[3] = { -1, -1, z, 1, 1, 1, 1, alpha, 0, v0 };
    quad[4] = { 1, 1, z, 1, 1, 1, 1, alpha, 1, v1 };
    quad[5] = { -1, 1, z, 1, 1, 1, 1, alpha, 0, v1 };

    const uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
        | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE);

    submitVertices(VIEW_COMPOSITE, g_texProg, g_texLayout, quad, 6, state, bgfx::getTexture(g_fbGlow),
                   BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, true);

    // The legacy code leaves additive blending enabled after the composite.
    g_blendEnabled = true;
    g_blendSrc = GL_SRC_ALPHA;
    g_blendDst = GL_ONE;
}

bool gfx_glow_enabled()
{
    return g_glowEnabled;
}

void gfx_set_glow_enabled(bool enabled)
{
    g_glowEnabled = enabled;
}

void gfx_glow_size(int* width, int* height)
{
    if (width)
        *width = g_glowW;
    if (height)
        *height = g_glowH;
}

void gfx_begin_frame()
{
    if (!g_loaded)
        return;

    flushBatch();
    g_targetView = VIEW_SCENE;
    configureViews();

    g_viewport[2] = g_fbW;
    g_viewport[3] = g_fbH;

    // The scene always draws at least an emulated clear, but touch the other
    // views so they are processed even when unused this frame.
    bgfx::touch(VIEW_SCENE);
    bgfx::touch(VIEW_COMPOSITE);
}

bool gfx_healthy()
{
    return g_loaded && bgfx::isValid(g_solidProg) && bgfx::isValid(g_texProg)
        && bgfx::isValid(g_blurProg);
}

void gfx_end_frame()
{
    // Hand the accumulated batches to bgfx. Must run before bgfx::frame().
    flushBatch();
}

// ---------------------------------------------------------------------------
// Offscreen back buffer (headless verification; see tools/bgfx_backend_test.cpp)
// ---------------------------------------------------------------------------
void gfx_bgfx_set_backbuffer(bgfx::FrameBufferHandle fb, uint16_t width, uint16_t height)
{
    g_backbuffer = fb;
    if (width > 0 && height > 0) {
        g_fbW = width;
        g_fbH = height;
        g_viewport[2] = width;
        g_viewport[3] = height;
    }
    if (g_loaded)
        configureViews();
}

} // extern "C"
