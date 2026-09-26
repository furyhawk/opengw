$input v_color0, v_texcoord0

/*
 * fs_tex.sc - textured geometry, legacy "modulate" texture environment
 * (texture * interpolated colour).
 */

#include <bgfx_shader.sh>

SAMPLER2D(s_tex, 0);

void main()
{
    gl_FragColor = texture2D(s_tex, v_texcoord0) * v_color0;
}
