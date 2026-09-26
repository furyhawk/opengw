$input v_texcoord0

/*
 * fs_blur.sc - separable 5-tap Gaussian blur (same kernel as the OpenGL 3.3
 * backend's glow pass).
 *
 * u_blurParams.xy = blur direction (1,0) or (0,1)
 * u_blurParams.zw = texel size of the source texture
 */

#include <bgfx_shader.sh>

SAMPLER2D(s_tex, 0);

uniform vec4 u_blurParams;

void main()
{
    vec2 dir   = u_blurParams.xy;
    vec2 texel = u_blurParams.zw;

    vec4 sum = texture2D(s_tex, v_texcoord0) * 0.2270270270;
    sum += texture2D(s_tex, v_texcoord0 + dir * texel * 1.3846153846) * 0.3162162162;
    sum += texture2D(s_tex, v_texcoord0 - dir * texel * 1.3846153846) * 0.3162162162;
    sum += texture2D(s_tex, v_texcoord0 + dir * texel * 3.2307692308) * 0.0702702703;
    sum += texture2D(s_tex, v_texcoord0 - dir * texel * 3.2307692308) * 0.0702702703;

    gl_FragColor = sum;
}
