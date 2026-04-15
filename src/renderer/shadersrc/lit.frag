#version 460
#extension GL_GOOGLE_include_directive : require
#include "common/shared.glsl"
#include "common/shared_material_read.glsl"

layout (location = 0) in vec2 frag_uv;
layout (location = 1) in vec3 frag_vcolor;

layout (location = 0) out vec4 out_color;

// vec3 spectral_rainbow(float t)
// {
//     vec3 c = 0.5 + 0.5 * cos(6.28318 * (t + vec3(0.0, 0.33, 0.67)));
//     return c;
// }

/*
    TODO: Pass in camera position in scene data nad do world space lighting, start with blinn phong?.
*/

void main()
{
    MaterialData mat;
    vec4 base_color;

    sample_material_basic(frag_uv, mat, base_color);

    // LIGHTING (TODO)
    // vec3 N = normalize(frag_world_normal);
    // vec3 V = normalize(scene.camera_pos - in_world_pos);

    vec4 final_color = vec4(frag_vcolor, 1.0) * base_color;

    out_color = vec4(
        final_color.rgb,
        process_alpha(final_color.a, mat.alpha_cutoff)
    );
}
