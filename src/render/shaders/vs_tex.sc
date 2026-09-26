$input a_position, a_color0, a_texcoord0
$output v_color0, v_texcoord0

/*
 * vs_tex.sc - textured geometry.
 *
 * a_position is already in clip space (see vs_solid.sc).
 */

#include <bgfx_shader.sh>

void main()
{
    gl_Position = a_position;
    v_color0    = a_color0;
    v_texcoord0 = a_texcoord0;
}
