// ---------------------------------------------------------------------------
// gl3.cpp — Modern OpenGL 3.3 core render backend for OpenGW
//
// Implements the fixed-function-compatible API declared in gl3.h on top of a
// real modern OpenGL 3.3 core pipeline:
//   * all GL entry points are loaded dynamically via SDL_GL_GetProcAddress
//   * geometry is rasterised through GLSL shaders + VAO/VBO (no immediate
//     mode, no glBegin rasterizer, no GLU)
//   * software matrix stacks reproduce the legacy projection/modelview maths
//   * wide lines / points are expanded to screen-space triangles so the
//     legacy glLineWidth/glPointSize look is preserved
//   * GPU FBO ping-pong Gaussian blur replaces the old CPU read-back blur
// ---------------------------------------------------------------------------

#include "render/gl3.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

// ===========================================================================
// Typedefs for the GL function pointers we load (subset of core 3.3)
// ===========================================================================
using GLsizeiptr = std::ptrdiff_t;

using PFNGLENABLE = void (*)(GLenum);
using PFNGLDISABLE = void (*)(GLenum);
using PFNGLBLENDFUNC = void (*)(GLenum, GLenum);
using PFNGLCLEARCOLOR = void (*)(GLfloat, GLfloat, GLfloat, GLfloat);
using PFNGLCLEAR = void (*)(GLbitfield);
using PFNGLVIEWPORT = void (*)(GLint, GLint, GLsizei, GLsizei);
using PFNGLGETINTEGERV = void (*)(GLenum, GLint*);
using PFNGLREADPIXELS = void (*)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
using PFNGLGENVERTEXARRAYS = void (*)(GLsizei, GLuint*);
using PFNGLDELETEVERTEXARRAYS = void (*)(GLsizei, const GLuint*);
using PFNGLBINDVERTEXARRAY = void (*)(GLuint);
using PFNGLGENBUFFERS = void (*)(GLsizei, GLuint*);
using PFNGLDELETEBUFFERS = void (*)(GLsizei, const GLuint*);
using PFNGLBINDBUFFER = void (*)(GLenum, GLuint);
using PFNGLBUFFERDATA = void (*)(GLenum, GLsizeiptr, const void*, GLenum);
using PFNGLBUFFERSUBDATA = void (*)(GLenum, GLsizeiptr, GLsizeiptr, const void*);
using PFNGLENABLEVERTEXATTRIBARRAY = void (*)(GLuint);
using PFNGLDISABLEVERTEXATTRIBARRAY = void (*)(GLuint);
using PFNGLVERTEXATTRIBPOINTER = void (*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
using PFNGLUSEPROGRAM = void (*)(GLuint);
using PFNGLCREATESHADER = GLuint (*)(GLenum);
using PFNGLSHADERSOURCE = void (*)(GLuint, GLsizei, const GLchar* const*, const GLint*);
using PFNGLCOMPILESHADER = void (*)(GLuint);
using PFNGLGETSHADERIV = void (*)(GLuint, GLenum, GLint*);
using PFNGLGETSHADERINFOLOG = void (*)(GLuint, GLsizei, GLsizei*, GLchar*);
using PFNGLDELETESHADER = void (*)(GLuint);
using PFNGLCREATEPROGRAM = GLuint (*)(void);
using PFNGLATTACHSHADER = void (*)(GLuint, GLuint);
using PFNGLLINKPROGRAM = void (*)(GLuint);
using PFNGLGETPROGRAMIV = void (*)(GLuint, GLenum, GLint*);
using PFNGLGETPROGRAMINFOLOG = void (*)(GLuint, GLsizei, GLsizei*, GLchar*);
using PFNGLDELETEPROGRAM = void (*)(GLuint);
using PFNGLUNIFORM1I = void (*)(GLint, GLint);
using PFNGLUNIFORM1F = void (*)(GLint, GLfloat);
using PFNGLUNIFORM2F = void (*)(GLint, GLfloat, GLfloat);
using PFNGLGETUNIFORMLOCATION = GLint (*)(GLuint, const GLchar*);
using PFNGLGENTEXTURES = void (*)(GLsizei, GLuint*);
using PFNGLDELETETEXTURES = void (*)(GLsizei, const GLuint*);
using PFNGLBINDTEXTURE = void (*)(GLenum, GLuint);
using PFNGLACTIVETEXTURE = void (*)(GLenum);
using PFNGLTEXIMAGE2D = void (*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
using PFNGLTEXSUBIMAGE2D = void (*)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*);
using PFNGLTEXPARAMETERI = void (*)(GLenum, GLenum, GLint);
using PFNGLTEXPARAMETERF = void (*)(GLenum, GLenum, GLfloat);
using PFNGLGENERATEMIPMAP = void (*)(GLenum);
using PFNGLGENFRAMEBUFFERS = void (*)(GLsizei, GLuint*);
using PFNGLDELETEFRAMEBUFFERS = void (*)(GLsizei, const GLuint*);
using PFNGLBINDFRAMEBUFFER = void (*)(GLenum, GLuint);
using PFNGLFRAMEBUFFERTEXTURE2D = void (*)(GLenum, GLenum, GLenum, GLuint, GLint);
using PFNGLCHECKFRAMEBUFFERSTATUS = GLenum (*)(GLenum);
using PFNGLDRAWARRAYS = void (*)(GLenum, GLint, GLsizei);

struct GLProcs
{
    PFNGLENABLE pEnable { nullptr };
    PFNGLDISABLE pDisable { nullptr };
    PFNGLBLENDFUNC pBlendFunc { nullptr };
    PFNGLCLEARCOLOR pClearColor { nullptr };
    PFNGLCLEAR pClear { nullptr };
    PFNGLVIEWPORT pViewport { nullptr };
    PFNGLGETINTEGERV pGetIntegerv { nullptr };
    PFNGLREADPIXELS pReadPixels { nullptr };
    PFNGLGENVERTEXARRAYS pGenVertexArrays { nullptr };
    PFNGLDELETEVERTEXARRAYS pDeleteVertexArrays { nullptr };
    PFNGLBINDVERTEXARRAY pBindVertexArray { nullptr };
    PFNGLGENBUFFERS pGenBuffers { nullptr };
    PFNGLDELETEBUFFERS pDeleteBuffers { nullptr };
    PFNGLBINDBUFFER pBindBuffer { nullptr };
    PFNGLBUFFERDATA pBufferData { nullptr };
    PFNGLBUFFERSUBDATA pBufferSubData { nullptr };
    PFNGLENABLEVERTEXATTRIBARRAY pEnableVertexAttribArray { nullptr };
    PFNGLDISABLEVERTEXATTRIBARRAY pDisableVertexAttribArray { nullptr };
    PFNGLVERTEXATTRIBPOINTER pVertexAttribPointer { nullptr };
    PFNGLUSEPROGRAM pUseProgram { nullptr };
    PFNGLCREATESHADER pCreateShader { nullptr };
    PFNGLSHADERSOURCE pShaderSource { nullptr };
    PFNGLCOMPILESHADER pCompileShader { nullptr };
    PFNGLGETSHADERIV pGetShaderiv { nullptr };
    PFNGLGETSHADERINFOLOG pGetShaderInfoLog { nullptr };
    PFNGLDELETESHADER pDeleteShader { nullptr };
    PFNGLCREATEPROGRAM pCreateProgram { nullptr };
    PFNGLATTACHSHADER pAttachShader { nullptr };
    PFNGLLINKPROGRAM pLinkProgram { nullptr };
    PFNGLGETPROGRAMIV pGetProgramiv { nullptr };
    PFNGLGETPROGRAMINFOLOG pGetProgramInfoLog { nullptr };
    PFNGLDELETEPROGRAM pDeleteProgram { nullptr };
    PFNGLUNIFORM1I pUniform1i { nullptr };
    PFNGLUNIFORM1F pUniform1f { nullptr };
    PFNGLUNIFORM2F pUniform2f { nullptr };
    PFNGLGETUNIFORMLOCATION pGetUniformLocation { nullptr };
    PFNGLGENTEXTURES pGenTextures { nullptr };
    PFNGLDELETETEXTURES pDeleteTextures { nullptr };
    PFNGLBINDTEXTURE pBindTexture { nullptr };
    PFNGLACTIVETEXTURE pActiveTexture { nullptr };
    PFNGLTEXIMAGE2D pTexImage2D { nullptr };
    PFNGLTEXSUBIMAGE2D pTexSubImage2D { nullptr };
    PFNGLTEXPARAMETERI pTexParameteri { nullptr };
    PFNGLTEXPARAMETERF pTexParameterf { nullptr };
    PFNGLGENERATEMIPMAP pGenerateMipmap { nullptr };
    PFNGLGENFRAMEBUFFERS pGenFramebuffers { nullptr };
    PFNGLDELETEFRAMEBUFFERS pDeleteFramebuffers { nullptr };
    PFNGLBINDFRAMEBUFFER pBindFramebuffer { nullptr };
    PFNGLFRAMEBUFFERTEXTURE2D pFramebufferTexture2D { nullptr };
    PFNGLCHECKFRAMEBUFFERSTATUS pCheckFramebufferStatus { nullptr };
    PFNGLDRAWARRAYS pDrawArrays { nullptr };
};

static GLProcs P;
static bool g_loaded = false;

template <typename T>
static bool loadProc(T& out, const char* name)
{
    out = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
    return out != nullptr;
}

// ===========================================================================
// Software matrix stacks (column-major 4x4, matches GL layout)
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
        out[r] = m[0 * 4 + r] * in[0] + m[1 * 4 + r] * in[1] + m[2 * 4 + r] * in[2] + m[3 * 4 + r] * in[3];
    }
}

} // namespace

// ===========================================================================
// Immediate-mode style state
// ===========================================================================
namespace {

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
bool g_depthTestEnabled = false;

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

// Virtual texture registry
struct TexRec
{
    GLuint real { 0 };
    int w { 0 };
    int h { 0 };
    bool valid { false };
};
std::vector<TexRec> g_tex;

GLuint g_solidProg { 0 };
GLuint g_texProg { 0 };
GLuint g_blurProg { 0 };
GLuint g_solidVAO { 0 }, g_texVAO { 0 };
GLuint g_solidVBO { 0 }, g_texVBO { 0 };
GLuint g_blurVAO { 0 }, g_blurVBO { 0 };

// Glow / blur targets
GLuint g_fboGlow { 0 }, g_texGlow { 0 };
GLuint g_fboPing { 0 }, g_texPing { 0 };
int g_glowW { 1 }, g_glowH { 1 };
int g_fbW { 800 }, g_fbH { 600 };
bool g_glowEnabled = true;
int g_glowScale = 2; // glow texture = window / scale

// Packed vertex layouts used on the GPU
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

std::vector<SolidVert> g_solidTris;
std::vector<TexVert> g_texTris;

} // namespace

namespace {
GLuint texIdToReal(GLuint id)
{
    if (id == 0)
        return 0;
    if (id >= g_tex.size() || !g_tex[id].valid)
        return 0;
    return g_tex[id].real;
}
} // namespace

// ===========================================================================
// Small shader helpers
// ===========================================================================
namespace {

GLuint compileShader(GLenum type, const char* src)
{
    GLuint sh = P.pCreateShader(type);
    const GLchar* s = src;
    P.pShaderSource(sh, 1, &s, nullptr);
    P.pCompileShader(sh);
    GLint ok = 0;
    P.pGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLchar log[1024] {};
        P.pGetShaderInfoLog(sh, sizeof(log), nullptr, log);
        printf("gl3: shader compile error:\n%s\n", log);
    }
    return sh;
}

GLuint buildProgram(const char* vsSrc, const char* fsSrc)
{
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    GLuint prog = P.pCreateProgram();
    P.pAttachShader(prog, vs);
    P.pAttachShader(prog, fs);
    P.pLinkProgram(prog);
    GLint ok = 0;
    P.pGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLchar log[1024] {};
        P.pGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        printf("gl3: program link error:\n%s\n", log);
    }
    P.pDeleteShader(vs);
    P.pDeleteShader(fs);
    return prog;
}

const char* kSolidVS = R"GLSL(#version 330 core
layout(location = 0) in vec4 aPos;
layout(location = 1) in vec4 aColor;
out vec4 vColor;
void main() {
    gl_Position = aPos;
    vColor = aColor;
}
)GLSL";

const char* kSolidFS = R"GLSL(#version 330 core
in vec4 vColor;
out vec4 fragColor;
void main() {
    fragColor = vColor;
}
)GLSL";

const char* kTexVS = R"GLSL(#version 330 core
layout(location = 0) in vec4 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aUV;
out vec4 vColor;
out vec2 vUV;
void main() {
    gl_Position = aPos;
    vColor = aColor;
    vUV = aUV;
}
)GLSL";

const char* kTexFS = R"GLSL(#version 330 core
uniform sampler2D uTex;
in vec4 vColor;
in vec2 vUV;
out vec4 fragColor;
void main() {
    fragColor = texture(uTex, vUV) * vColor;
}
)GLSL";

const char* kBlurVS = R"GLSL(#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 vUV;
void main() {
    vUV = aPos * 0.5 + 0.5;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)GLSL";

const char* kBlurFS = R"GLSL(#version 330 core
uniform sampler2D uTex;
uniform vec2 uDir;
uniform vec2 uTexel;
in vec2 vUV;
out vec4 fragColor;
void main() {
    vec4 sum = texture(uTex, vUV) * 0.2270270270;
    sum += texture(uTex, vUV + uDir * uTexel * 1.3846153846) * 0.3162162162;
    sum += texture(uTex, vUV - uDir * uTexel * 1.3846153846) * 0.3162162162;
    sum += texture(uTex, vUV + uDir * uTexel * 3.2307692308) * 0.0702702703;
    sum += texture(uTex, vUV - uDir * uTexel * 3.2307692308) * 0.0702702703;
    fragColor = sum;
}
)GLSL";

} // namespace

// ===========================================================================
// Vertex / primitive assembly
// ===========================================================================
namespace {

struct ClipV
{
    float x, y, z, w;
    float r, g, b, a;
    float u, v;
    bool hasUV;
};

// Transform every submitted immediate-mode vertex into clip space.
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
        cv.z = c[2];
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
    out[2] = 0.0f;
    out[3] = 1.0f;
}

void pushSolid(float x, float y, float z, float w, float r, float g, float b, float a)
{
    SolidVert v { x, y, z, w, r, g, b, a };
    g_solidTris.push_back(v);
}

void pushTex(float x, float y, float z, float w, float r, float g, float b, float a, float u, float v)
{
    TexVert tv { x, y, z, w, r, g, b, a, u, v };
    g_texTris.push_back(tv);
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
        // Degenerate: draw a small pixel square
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

    // Two triangles making the quad a0..b1
    pushSolidClipPx(a, ax + nx, ay + ny); // a upper
    pushSolidClipPx(a, ax - nx, ay - ny); // a lower
    pushSolidClipPx(b, bx + nx, by + ny); // b upper

    pushSolidClipPx(a, ax - nx, ay - ny); // a lower
    pushSolidClipPx(b, bx - nx, by - ny); // b lower
    pushSolidClipPx(b, bx + nx, by + ny); // b upper
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

void flushDraw()
{
    if (!g_loaded)
        return;

    if (!g_solidTris.empty() && g_solidProg) {
        P.pUseProgram(g_solidProg);
        P.pBindVertexArray(g_solidVAO);
        P.pBindBuffer(GL_ARRAY_BUFFER, g_solidVBO);
        P.pBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(g_solidTris.size() * sizeof(SolidVert)),
                      g_solidTris.data(), GL_DYNAMIC_DRAW);
        P.pDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(g_solidTris.size()));
        g_solidTris.clear();
    }

    if (!g_texTris.empty() && g_texProg) {
        const GLuint real = texIdToReal(g_boundTexture);
        if (real != 0) {
            P.pUseProgram(g_texProg);
            P.pBindVertexArray(g_texVAO);
            P.pBindBuffer(GL_ARRAY_BUFFER, g_texVBO);
            P.pBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(g_texTris.size() * sizeof(TexVert)),
                          g_texTris.data(), GL_DYNAMIC_DRAW);
            P.pActiveTexture(GL_TEXTURE0);
            P.pBindTexture(GL_TEXTURE_2D, real);
            P.pDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(g_texTris.size()));
        }
        g_texTris.clear();
    }
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
        // Filled polygons: emitted as clip-space triangles (GPU perspective).
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

    flushDraw();
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
    if (!g_insideBegin)
        return;
    ImmVertex iv;
    iv.x = static_cast<float>(x);
    iv.y = static_cast<float>(y);
    iv.z = 0.0f;
    iv.r = g_curColor[0];
    iv.g = g_curColor[1];
    iv.b = g_curColor[2];
    iv.a = g_curColor[3];
    iv.u = g_curUV[0];
    iv.v = g_curUV[1];
    iv.hasUV = g_curHasUV;
    g_imm.push_back(iv);
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
    case GL_LINE_SMOOTH:
    case GL_POINT_SMOOTH:
        // Modern pipeline always renders smooth geometry; nothing to do.
        break;
    case GL_BLEND:
        g_blendEnabled = true;
        if (g_loaded)
            P.pEnable(GL_BLEND);
        break;
    case GL_DEPTH_TEST:
        g_depthTestEnabled = true;
        if (g_loaded)
            P.pEnable(GL_DEPTH_TEST);
        break;
    case GL_MULTISAMPLE:
        if (g_loaded)
            P.pEnable(GL_MULTISAMPLE);
        break;
    default:
        if (g_loaded)
            P.pEnable(cap);
        break;
    }
}

void gfx_disable(GLenum cap)
{
    switch (cap) {
    case GL_TEXTURE_2D:
        g_textureEnabled = false;
        break;
    case GL_LINE_SMOOTH:
    case GL_POINT_SMOOTH:
        break;
    case GL_BLEND:
        g_blendEnabled = false;
        if (g_loaded)
            P.pDisable(GL_BLEND);
        break;
    case GL_DEPTH_TEST:
        g_depthTestEnabled = false;
        if (g_loaded)
            P.pDisable(GL_DEPTH_TEST);
        break;
    case GL_MULTISAMPLE:
        if (g_loaded)
            P.pDisable(GL_MULTISAMPLE);
        break;
    default:
        if (g_loaded)
            P.pDisable(cap);
        break;
    }
}

void gfx_blendfunc(GLenum src, GLenum dst)
{
    if (g_loaded)
        P.pBlendFunc(src, dst);
}

void gfx_clearcolor(GLclampf r, GLclampf g, GLclampf b, GLclampf a)
{
    if (g_loaded)
        P.pClearColor(r, g, b, a);
}

void gfx_clear(GLbitfield mask)
{
    if (g_loaded)
        P.pClear(mask);
}

void gfx_viewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    g_viewport[0] = x;
    g_viewport[1] = y;
    g_viewport[2] = width;
    g_viewport[3] = height;
    if (g_loaded)
        P.pViewport(x, y, width, height);
}

void gfx_getintegerv(GLenum pname, GLint* params)
{
    if (pname == GL_VIEWPORT) {
        params[0] = g_viewport[0];
        params[1] = g_viewport[1];
        params[2] = g_viewport[2];
        params[3] = g_viewport[3];
    } else if (g_loaded) {
        P.pGetIntegerv(pname, params);
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
    // Texture environment is handled implicitly by the shaders (modulate).
}

// ---- texture object management -------------------------------------------

void gfx_gentextures(GLsizei n, GLuint* textures)
{
    for (GLsizei i = 0; i < n; ++i) {
        std::size_t slot = 0;
        for (std::size_t k = 1; k < g_tex.size(); ++k) {
            if (!g_tex[k].valid) {
                slot = k;
                break;
            }
        }
        if (slot == 0) {
            slot = g_tex.size();
            g_tex.push_back(TexRec {});
        }
        g_tex[slot].valid = true;
        textures[i] = static_cast<GLuint>(slot);
    }
}

void gfx_bindtexture(GLenum target, GLuint texture)
{
    if (target != GL_TEXTURE_2D)
        return;
    g_boundTexture = texture;
    // Bind the real texture so subsequent tex params / sampling target it.
    const GLuint real = texIdToReal(texture);
    if (g_loaded) {
        P.pActiveTexture(GL_TEXTURE0);
        P.pBindTexture(GL_TEXTURE_2D, real);
    }
}

void gfx_teximage2d(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border,
                    GLenum format, GLenum type, const GLvoid* pixels)
{
    if (target != GL_TEXTURE_2D || g_boundTexture == 0 || !g_loaded)
        return;
    TexRec& rec = g_tex[g_boundTexture];
    if (rec.real == 0)
        P.pGenTextures(1, &rec.real);
    rec.w = width;
    rec.h = height;
    P.pActiveTexture(GL_TEXTURE0);
    P.pBindTexture(GL_TEXTURE_2D, rec.real);
    GLint internal = internalformat;
    // Normalise common legacy internal formats to 8-bit RGBA.
    if (internalformat == 3)
        internal = GL_RGB;
    else if (internalformat == 4)
        internal = GL_RGBA;
    P.pTexImage2D(target, level, internal, width, height, border, format, type, pixels);
}

void gfx_texsubimage2d(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                       GLenum format, GLenum type, const GLvoid* pixels)
{
    if (target != GL_TEXTURE_2D || g_boundTexture == 0 || !g_loaded)
        return;
    const TexRec& rec = g_tex[g_boundTexture];
    if (rec.real == 0)
        return;
    P.pActiveTexture(GL_TEXTURE0);
    P.pBindTexture(GL_TEXTURE_2D, rec.real);
    P.pTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void gfx_texparameteri(GLenum target, GLenum pname, GLint param)
{
    if (target != GL_TEXTURE_2D || g_boundTexture == 0 || !g_loaded)
        return;
    const TexRec& rec = g_tex[g_boundTexture];
    if (rec.real == 0)
        return;
    P.pActiveTexture(GL_TEXTURE0);
    P.pBindTexture(GL_TEXTURE_2D, rec.real);
    P.pTexParameteri(target, pname, param);
}

void gfx_texparameterf(GLenum target, GLenum pname, GLfloat param)
{
    if (target != GL_TEXTURE_2D || g_boundTexture == 0 || !g_loaded)
        return;
    const TexRec& rec = g_tex[g_boundTexture];
    if (rec.real == 0)
        return;
    P.pActiveTexture(GL_TEXTURE0);
    P.pBindTexture(GL_TEXTURE_2D, rec.real);
    P.pTexParameterf(target, pname, param);
}

void gfx_build2dmipmaps(GLenum target, GLint /*internalformat*/, GLsizei width, GLsizei height, GLenum format,
                        GLenum type, const GLvoid* pixels)
{
    if (target != GL_TEXTURE_2D || g_boundTexture == 0 || !g_loaded)
        return;
    TexRec& rec = g_tex[g_boundTexture];
    if (rec.real == 0)
        P.pGenTextures(1, &rec.real);
    rec.w = width;
    rec.h = height;
    P.pActiveTexture(GL_TEXTURE0);
    P.pBindTexture(GL_TEXTURE_2D, rec.real);
    P.pTexImage2D(target, 0, GL_RGBA, width, height, 0, format, type, pixels);
    P.pGenerateMipmap(GL_TEXTURE_2D);
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
            // tightly packed arrays of scalar values
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

    // Build the referenced vertex list.
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
// Host app helpers: context, glow/blur FBOs
// ===========================================================================
namespace {

GLuint createGlowTexture(int w, int h)
{
    GLuint tex = 0;
    P.pGenTextures(1, &tex);
    P.pBindTexture(GL_TEXTURE_2D, tex);
    P.pTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    P.pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    P.pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    P.pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    P.pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

GLuint createGlowFbo(GLuint tex)
{
    GLuint fbo = 0;
    P.pGenFramebuffers(1, &fbo);
    P.pBindFramebuffer(GL_FRAMEBUFFER, fbo);
    P.pFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    P.pBindFramebuffer(GL_FRAMEBUFFER, 0);
    return fbo;
}

void drawFullscreenTexturedPass(GLuint program, GLuint tex, const float dir[2], float texelX, float texelY)
{
    P.pUseProgram(program);
    P.pBindVertexArray(g_blurVAO);
    P.pActiveTexture(GL_TEXTURE0);
    P.pBindTexture(GL_TEXTURE_2D, tex);
    P.pUniform1i(P.pGetUniformLocation(program, "uTex"), 0);
    P.pUniform2f(P.pGetUniformLocation(program, "uDir"), dir[0], dir[1]);
    P.pUniform2f(P.pGetUniformLocation(program, "uTexel"), texelX, texelY);
    P.pDrawArrays(GL_TRIANGLES, 0, 3);
}

} // namespace

extern "C" {

void gfx_context_init()
{
    if (g_loaded)
        return;

    // Load every entry point we need.
    loadProc(P.pEnable, "glEnable");
    loadProc(P.pDisable, "glDisable");
    loadProc(P.pBlendFunc, "glBlendFunc");
    loadProc(P.pClearColor, "glClearColor");
    loadProc(P.pClear, "glClear");
    loadProc(P.pViewport, "glViewport");
    loadProc(P.pGetIntegerv, "glGetIntegerv");
    loadProc(P.pReadPixels, "glReadPixels");
    loadProc(P.pGenVertexArrays, "glGenVertexArrays");
    loadProc(P.pDeleteVertexArrays, "glDeleteVertexArrays");
    loadProc(P.pBindVertexArray, "glBindVertexArray");
    loadProc(P.pGenBuffers, "glGenBuffers");
    loadProc(P.pDeleteBuffers, "glDeleteBuffers");
    loadProc(P.pBindBuffer, "glBindBuffer");
    loadProc(P.pBufferData, "glBufferData");
    loadProc(P.pBufferSubData, "glBufferSubData");
    loadProc(P.pEnableVertexAttribArray, "glEnableVertexAttribArray");
    loadProc(P.pDisableVertexAttribArray, "glDisableVertexAttribArray");
    loadProc(P.pVertexAttribPointer, "glVertexAttribPointer");
    loadProc(P.pUseProgram, "glUseProgram");
    loadProc(P.pCreateShader, "glCreateShader");
    loadProc(P.pShaderSource, "glShaderSource");
    loadProc(P.pCompileShader, "glCompileShader");
    loadProc(P.pGetShaderiv, "glGetShaderiv");
    loadProc(P.pGetShaderInfoLog, "glGetShaderInfoLog");
    loadProc(P.pDeleteShader, "glDeleteShader");
    loadProc(P.pCreateProgram, "glCreateProgram");
    loadProc(P.pAttachShader, "glAttachShader");
    loadProc(P.pLinkProgram, "glLinkProgram");
    loadProc(P.pGetProgramiv, "glGetProgramiv");
    loadProc(P.pGetProgramInfoLog, "glGetProgramInfoLog");
    loadProc(P.pDeleteProgram, "glDeleteProgram");
    loadProc(P.pUniform1i, "glUniform1i");
    loadProc(P.pUniform1f, "glUniform1f");
    loadProc(P.pUniform2f, "glUniform2f");
    loadProc(P.pGetUniformLocation, "glGetUniformLocation");
    loadProc(P.pGenTextures, "glGenTextures");
    loadProc(P.pDeleteTextures, "glDeleteTextures");
    loadProc(P.pBindTexture, "glBindTexture");
    loadProc(P.pActiveTexture, "glActiveTexture");
    loadProc(P.pTexImage2D, "glTexImage2D");
    loadProc(P.pTexSubImage2D, "glTexSubImage2D");
    loadProc(P.pTexParameteri, "glTexParameteri");
    loadProc(P.pTexParameterf, "glTexParameterf");
    loadProc(P.pGenerateMipmap, "glGenerateMipmap");
    loadProc(P.pGenFramebuffers, "glGenFramebuffers");
    loadProc(P.pDeleteFramebuffers, "glDeleteFramebuffers");
    loadProc(P.pBindFramebuffer, "glBindFramebuffer");
    loadProc(P.pFramebufferTexture2D, "glFramebufferTexture2D");
    loadProc(P.pCheckFramebufferStatus, "glCheckFramebufferStatus");
    loadProc(P.pDrawArrays, "glDrawArrays");

    g_loaded = true;

    if (!P.pDrawArrays || !P.pGenVertexArrays || !P.pGenFramebuffers) {
        printf("gl3: failed to load core OpenGL 3.3 entry points\n");
        return;
    }

    // Shaders
    g_solidProg = buildProgram(kSolidVS, kSolidFS);
    g_texProg = buildProgram(kTexVS, kTexFS);
    g_blurProg = buildProgram(kBlurVS, kBlurFS);

    // Solid VAO/VBO: 2 attributes (vec4 pos, vec4 color)
    P.pGenVertexArrays(1, &g_solidVAO);
    P.pBindVertexArray(g_solidVAO);
    P.pGenBuffers(1, &g_solidVBO);
    P.pBindBuffer(GL_ARRAY_BUFFER, g_solidVBO);
    P.pEnableVertexAttribArray(0);
    P.pVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(SolidVert), reinterpret_cast<const void*>(0));
    P.pEnableVertexAttribArray(1);
    P.pVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(SolidVert), reinterpret_cast<const void*>(16));

    // Textured VAO/VBO: 3 attributes (pos, color, uv)
    P.pGenVertexArrays(1, &g_texVAO);
    P.pBindVertexArray(g_texVAO);
    P.pGenBuffers(1, &g_texVBO);
    P.pBindBuffer(GL_ARRAY_BUFFER, g_texVBO);
    P.pEnableVertexAttribArray(0);
    P.pVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(TexVert), reinterpret_cast<const void*>(0));
    P.pEnableVertexAttribArray(1);
    P.pVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(TexVert), reinterpret_cast<const void*>(16));
    P.pEnableVertexAttribArray(2);
    P.pVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(TexVert), reinterpret_cast<const void*>(32));

    // Blur VAO/VBO: fullscreen triangle (2 attributes -> 2D position)
    P.pGenVertexArrays(1, &g_blurVAO);
    P.pBindVertexArray(g_blurVAO);
    P.pGenBuffers(1, &g_blurVBO);
    P.pBindBuffer(GL_ARRAY_BUFFER, g_blurVBO);
    const float tri[] = { -1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f };
    P.pBufferData(GL_ARRAY_BUFFER, sizeof(tri), tri, GL_STATIC_DRAW);
    P.pEnableVertexAttribArray(0);
    P.pVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<const void*>(0));

    P.pBindVertexArray(0);
    P.pBindBuffer(GL_ARRAY_BUFFER, 0);

    g_solidTris.reserve(65536);
    g_texTris.reserve(4096);

    // Initial state mirrors what the legacy pipeline expected.
    P.pDisable(GL_DEPTH_TEST);
    P.pDisable(GL_BLEND);
    P.pBlendFunc(GL_SRC_ALPHA, GL_ONE);
}

void gfx_context_shutdown()
{
    if (!g_loaded)
        return;
    P.pDeleteProgram(g_solidProg);
    P.pDeleteProgram(g_texProg);
    P.pDeleteProgram(g_blurProg);
    P.pDeleteVertexArrays(1, &g_solidVAO);
    P.pDeleteVertexArrays(1, &g_texVAO);
    P.pDeleteVertexArrays(1, &g_blurVAO);
    P.pDeleteBuffers(1, &g_solidVBO);
    P.pDeleteBuffers(1, &g_texVBO);
    P.pDeleteBuffers(1, &g_blurVBO);
    g_loaded = false;
}

void gfx_resize(int width, int height)
{
    g_fbW = width;
    g_fbH = height;
    if (!g_loaded)
        return;

    const int scale = g_glowScale > 0 ? g_glowScale : 2;
    const int gw = width / scale > 0 ? width / scale : 1;
    const int gh = height / scale > 0 ? height / scale : 1;

    if (g_texGlow)
        P.pDeleteTextures(1, &g_texGlow);
    if (g_texPing)
        P.pDeleteTextures(1, &g_texPing);
    if (g_fboGlow)
        P.pDeleteFramebuffers(1, &g_fboGlow);
    if (g_fboPing)
        P.pDeleteFramebuffers(1, &g_fboPing);

    g_glowW = gw;
    g_glowH = gh;

    g_texGlow = createGlowTexture(gw, gh);
    g_texPing = createGlowTexture(gw, gh);
    g_fboGlow = createGlowFbo(g_texGlow);
    g_fboPing = createGlowFbo(g_texPing);

    P.pBindFramebuffer(GL_FRAMEBUFFER, 0);
    P.pViewport(0, 0, width, height);
    g_viewport[2] = width;
    g_viewport[3] = height;
}

void gfx_glow_bind()
{
    if (!g_loaded)
        return;
    P.pBindFramebuffer(GL_FRAMEBUFFER, g_fboGlow);
    P.pViewport(0, 0, g_glowW, g_glowH);
    g_viewport[2] = g_glowW;
    g_viewport[3] = g_glowH;
}

void gfx_glow_unbind()
{
    if (!g_loaded)
        return;
    P.pBindFramebuffer(GL_FRAMEBUFFER, 0);
    P.pViewport(0, 0, g_fbW, g_fbH);
    g_viewport[2] = g_fbW;
    g_viewport[3] = g_fbH;
}

void gfx_blur_glow()
{
    if (!g_loaded)
        return;
    const float texelX = 1.0f / static_cast<float>(g_glowW);
    const float texelY = 1.0f / static_cast<float>(g_glowH);

    // The blur passes must overwrite, never blend with whatever is in the
    // (uncleared) ping/pong targets.
    P.pDisable(GL_BLEND);
    P.pDisable(GL_DEPTH_TEST);

    // Two horizontal+vertical iterations approximate the original double blur.
    for (int iter = 0; iter < 2; ++iter) {
        // Horizontal: glow -> ping
        P.pBindFramebuffer(GL_FRAMEBUFFER, g_fboPing);
        P.pViewport(0, 0, g_glowW, g_glowH);
        const float horiz[2] = { 1.0f, 0.0f };
        drawFullscreenTexturedPass(g_blurProg, g_texGlow, horiz, texelX, texelY);

        // Vertical: ping -> glow
        P.pBindFramebuffer(GL_FRAMEBUFFER, g_fboGlow);
        P.pViewport(0, 0, g_glowW, g_glowH);
        const float vert[2] = { 0.0f, 1.0f };
        drawFullscreenTexturedPass(g_blurProg, g_texPing, vert, texelX, texelY);
    }

    P.pBindFramebuffer(GL_FRAMEBUFFER, 0);
    P.pViewport(0, 0, g_fbW, g_fbH);
}

void gfx_draw_blurred_glow(float alpha)
{
    if (!g_loaded)
        return;
    P.pBindFramebuffer(GL_FRAMEBUFFER, 0);
    P.pViewport(0, 0, g_fbW, g_fbH);

    P.pEnable(GL_BLEND);
    P.pBlendFunc(GL_SRC_ALPHA, GL_ONE);

    P.pUseProgram(g_texProg);
    P.pBindVertexArray(g_texVAO);
    P.pActiveTexture(GL_TEXTURE0);
    P.pBindTexture(GL_TEXTURE_2D, g_texGlow);
    P.pUniform1i(P.pGetUniformLocation(g_texProg, "uTex"), 0);

    TexVert quad[6] = {};
    // bottom-left
    quad[0] = { -1, -1, 0, 1, 1, 1, 1, alpha, 0, 0 };
    quad[1] = { 1, -1, 0, 1, 1, 1, 1, alpha, 1, 0 };
    quad[2] = { 1, 1, 0, 1, 1, 1, 1, alpha, 1, 1 };
    // second triangle
    quad[3] = { -1, -1, 0, 1, 1, 1, 1, alpha, 0, 0 };
    quad[4] = { 1, 1, 0, 1, 1, 1, 1, alpha, 1, 1 };
    quad[5] = { -1, 1, 0, 1, 1, 1, 1, alpha, 0, 1 };

    P.pBindBuffer(GL_ARRAY_BUFFER, g_texVBO);
    P.pBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_DYNAMIC_DRAW);
    P.pDrawArrays(GL_TRIANGLES, 0, 6);
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

} // extern "C"
