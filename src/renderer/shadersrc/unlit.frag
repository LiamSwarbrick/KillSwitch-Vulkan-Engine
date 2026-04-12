#version 460
#extension GL_GOOGLE_include_directive : require
#include "common/shared.glsl"
#include "common/shared_material_read.glsl"

layout (location = 0) in vec2 frag_uv;
layout (location = 1) in vec3 frag_vcolor;

layout (location = 0) out vec4 out_color;

void main()
{
    MaterialData mat;
    vec4 base_color;

    sample_material_basic(frag_uv, mat, base_color);

    vec4 final_color = vec4(frag_vcolor, 1.0) * base_color;

    out_color = vec4(
        final_color.rgb,
        final_color.a * process_alpha(final_color.a, mat.alpha_cutoff)
    );
}
