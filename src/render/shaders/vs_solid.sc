$input a_position, a_color0
$output v_color0

/*
 * vs_solid.sc - untextured (flat colour) geometry.
 *
 * a_position is already in clip space (the host emulates the legacy fixed
 * function pipeline on the CPU), so no model/view/projection transform is
 * applied here.
 */

#include <bgfx_shader.sh>

void main()
{
    gl_Position = a_position;
    v_color0    = a_color0;
}
