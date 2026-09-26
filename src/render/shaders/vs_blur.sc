$input a_position
$output v_texcoord0

/*
 * vs_blur.sc - fullscreen triangle for the glow/blur passes.
 *
 * The geometry is a single oversized triangle covering the whole target
 * (-1,-1) (3,-1) (-1,3); a_position is in clip space.
 *
 * u_uvScale.xy maps clip space to texture coordinates. It is (0.5, 0.5) for
 * backends whose render-target textures use OpenGL's row order and
 * (0.5, -0.5) for Metal/Direct3D, where NDC y=+1 lands in row 0.
 */

#include <bgfx_shader.sh>

uniform vec4 u_uvScale;

void main()
{
    v_texcoord0 = vec2_splat(0.5) + a_position.xy * u_uvScale.xy;
    gl_Position = vec4(a_position.xy, 0.0, 1.0);
}
