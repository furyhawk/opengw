// varying.def.sc - vertex attribute/varying declarations for the Trigonometry
// Wars bgfx shaders.  Shared by all vertex/fragment shaders in this folder.
//
// a_position carries *clip space* coordinates (x, y, z, w): the backend's
// fixed-function emulation transforms vertices on the CPU exactly like the
// OpenGL 3.3 backend does, so the shaders only have to pass them through.

vec4 v_color0    : COLOR0    = vec4(1.0, 0.0, 0.0, 1.0);
vec2 v_texcoord0 : TEXCOORD0 = vec2(0.0, 0.0);

vec4 a_position  : POSITION;
vec4 a_color0    : COLOR0;
vec2 a_texcoord0 : TEXCOORD0;
