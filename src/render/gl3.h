#pragma once

// ---------------------------------------------------------------------------
// gl3.h — Modern OpenGL 3.3 core render backend for OpenGW
//
// This header is the single graphics entry point for the game.  It provides:
//
//   1. A thin, dependency-free subset of the OpenGL 3.3 CORE API surface that
//      the game actually uses (types + constants only, no GL headers pulled in).
//   2. A faithful re-implementation of the *legacy fixed-function* immediate
//      mode API (glBegin/glEnd/glVertex*/glColor4f/glTexCoord*, matrix stack,
//      glLineWidth/glPointSize, client arrays, GL_QUADS, ...) implemented on
//      top of that modern core pipeline — shaders, VAO/VBO, FBOs.
//
// The fixed-function symbols are #defined to gfx_* so existing game code keeps
// reading naturally while actually hitting the modern backend.
//
// Unlike the old code there is NO GLU and NO immediate-mode rasterizer:
//   - geometry is transformed with software matrices and rasterised through
//     GLSL shaders + vertex buffers (batched into triangles),
//   - wide lines / points are expanded to screen-space geometry (legacy
//     glLineWidth/glPointSize semantics preserved),
//   - the "glow/blur" effect is GPU-based (FBO ping-pong Gaussian blur)
//     instead of CPU read-back.
// ---------------------------------------------------------------------------

#include <cstdint>

// ---------------------------------------------------------------------------
// OpenGL-ish scalar/type aliases (values match the GL spec)
// ---------------------------------------------------------------------------
using GLenum = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLbitfield = unsigned int;
using GLfloat = float;
using GLclampf = float;
using GLdouble = double;
using GLboolean = unsigned char;
using GLbyte = char;
using GLubyte = unsigned char;
using GLshort = short;
using GLushort = unsigned short;
using GLchar = char;
using GLvoid = void;

// ---------------------------------------------------------------------------
// Constants (subset actually used by OpenGW; standard GL values)
// ---------------------------------------------------------------------------
#define GL_FALSE 0
#define GL_TRUE 1

#define GL_POINTS 0x0000
#define GL_LINES 0x0001
#define GL_LINE_LOOP 0x0002
#define GL_LINE_STRIP 0x0003
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN 0x0006
#define GL_QUADS 0x0007

#define GL_ZERO 0
#define GL_ONE 1
#define GL_SRC_COLOR 0x0300
#define GL_ONE_MINUS_SRC_COLOR 0x0301
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DST_ALPHA 0x0304
#define GL_ONE_MINUS_DST_ALPHA 0x0305
#define GL_DST_COLOR 0x0306
#define GL_ONE_MINUS_DST_COLOR 0x0307

#define GL_BYTE 0x1400
#define GL_UNSIGNED_BYTE 0x1401
#define GL_SHORT 0x1402
#define GL_UNSIGNED_SHORT 0x1403
#define GL_INT 0x1404
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_DOUBLE 0x140A

#define GL_MODELVIEW 0x1700
#define GL_PROJECTION 0x1701
#define GL_TEXTURE_2D 0x0DE1

#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_LINE_SMOOTH 0x0B20
#define GL_POINT_SMOOTH 0x0B10
#define GL_MULTISAMPLE 0x809D

#define GL_TEXTURE_ENV 0x2300
#define GL_TEXTURE_ENV_MODE 0x2200
#define GL_MODULATE 0x2100
#define GL_DECAL 0x2101

#define GL_VIEWPORT 0x0BA2
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601

#define GL_VERTEX_ARRAY 0x8074
#define GL_COLOR_ARRAY 0x8076

#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_COLOR_BUFFER_BIT 0x00004000

// Internal core objects (used by the backend itself)
#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_TEXTURE0 0x84C0
#define GL_FRAMEBUFFER 0x8D40
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_RGBA8 0x8058
#define GL_TRIANGLE_FAN_2 0x0006 // (alias kept for readability; same as GL_TRIANGLE_FAN)

// ---------------------------------------------------------------------------
// Fixed-function compatible API (implemented in gl3.cpp on the core pipeline)
// ---------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

void gfx_begin(GLenum mode);
void gfx_end();

void gfx_color4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);

void gfx_vertex2d(GLdouble x, GLdouble y);
void gfx_vertex3d(GLdouble x, GLdouble y, GLdouble z);
void gfx_vertex3f(GLfloat x, GLfloat y, GLfloat z);

void gfx_texcoord2d(GLdouble s, GLdouble t);
void gfx_texcoord2f(GLfloat s, GLfloat t);

void gfx_linewidth(GLfloat width);
void gfx_pointsize(GLfloat size);

void gfx_enable(GLenum cap);
void gfx_disable(GLenum cap);
void gfx_blendfunc(GLenum src, GLenum dst);

void gfx_clearcolor(GLclampf r, GLclampf g, GLclampf b, GLclampf a);
void gfx_clear(GLbitfield mask);

void gfx_viewport(GLint x, GLint y, GLsizei width, GLsizei height);
void gfx_getintegerv(GLenum pname, GLint* params);

void gfx_matrixmode(GLenum mode);
void gfx_pushmatrix();
void gfx_popmatrix();
void gfx_loadidentity();
void gfx_ortho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
void gfx_translatef(GLfloat x, GLfloat y, GLfloat z);

void gfx_texenvf(GLenum target, GLenum pname, GLfloat param);

void gfx_gentextures(GLsizei n, GLuint* textures);
void gfx_bindtexture(GLenum target, GLuint texture);
void gfx_teximage2d(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border,
                    GLenum format, GLenum type, const GLvoid* pixels);
void gfx_texsubimage2d(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                       GLenum format, GLenum type, const GLvoid* pixels);
void gfx_texparameteri(GLenum target, GLenum pname, GLint param);
void gfx_texparameterf(GLenum target, GLenum pname, GLfloat param);

void gfx_enableclientstate(GLenum array);
void gfx_disableclientstate(GLenum array);
void gfx_vertexpointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer);
void gfx_colorpointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer);
void gfx_drawelements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);

// GLU helpers re-implemented without GLU
void gfx_perspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar);
void gfx_build2dmipmaps(GLenum target, GLint internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type,
                        const GLvoid* pixels);

#ifdef __cplusplus
}
#endif

// ---------------------------------------------------------------------------
// Map the legacy fixed-function names onto the modern backend
// ---------------------------------------------------------------------------
#define glBegin gfx_begin
#define glEnd gfx_end
#define glColor4f gfx_color4f
#define glVertex2d gfx_vertex2d
#define glVertex3d gfx_vertex3d
#define glVertex3f gfx_vertex3f
#define glTexCoord2d gfx_texcoord2d
#define glTexCoord2f gfx_texcoord2f
#define glLineWidth gfx_linewidth
#define glPointSize gfx_pointsize
#define glEnable gfx_enable
#define glDisable gfx_disable
#define glBlendFunc gfx_blendfunc
#define glClearColor gfx_clearcolor
#define glClear gfx_clear
#define glViewport gfx_viewport
#define glGetIntegerv gfx_getintegerv
#define glMatrixMode gfx_matrixmode
#define glPushMatrix gfx_pushmatrix
#define glPopMatrix gfx_popmatrix
#define glLoadIdentity gfx_loadidentity
#define glOrtho gfx_ortho
#define glTranslatef gfx_translatef
#define glTexEnvf gfx_texenvf
#define glGenTextures gfx_gentextures
#define glBindTexture gfx_bindtexture
#define glTexImage2D gfx_teximage2d
#define glTexSubImage2D gfx_texsubimage2d
#define glTexParameteri gfx_texparameteri
#define glTexParameterf gfx_texparameterf
#define glEnableClientState gfx_enableclientstate
#define glDisableClientState gfx_disableclientstate
#define glVertexPointer gfx_vertexpointer
#define glColorPointer gfx_colorpointer
#define glDrawElements gfx_drawelements
#define gluPerspective gfx_perspective
#define gluBuild2DMipmaps gfx_build2dmipmaps

// ---------------------------------------------------------------------------
// New backend helpers used by the host app (OpenGW.cpp) — not fixed-function
// ---------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// Initialise the modern GL context state (function loading, shaders, VAOs).
// Must be called once after the GL context is created and current.
void gfx_context_init();

// Release GPU resources. Call before destroying the context.
void gfx_context_shutdown();

// Window (back buffer) size changed — rebuilds GPU glow/blur targets.
void gfx_resize(int width, int height);

// Bind/unbind the glow render target (used to render the RENDERPASS_BLUR pass
// into a low-resolution texture instead of reading pixels back to the CPU).
void gfx_glow_bind();
void gfx_glow_unbind();

// Apply a separable Gaussian blur to the glow texture (result stays in the
// glow texture). Should be called with the default framebuffer bound.
void gfx_blur_glow();

// Additively composite the (already blurred) glow texture over the whole
// screen. alpha scales the brightness of the composite.
void gfx_draw_blurred_glow(float alpha);

bool gfx_glow_enabled();
void gfx_set_glow_enabled(bool enabled);
void gfx_glow_size(int* width, int* height);

#ifdef __cplusplus
}
#endif
