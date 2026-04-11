#version 460
#extension GL_GOOGLE_include_directive : require
#include "common/shared.glsl"

layout (location = 0) in vec2 frag_uv;

// NOTE: All this shader does is implicitly write to the depth buffer
void main()
{
    if (CURRENT_BLEND_MODE == BLEND_MODE_MASKED)
    {
        // Alpha masking
        MaterialData mat;
        vec4 base_color;

        sample_material_basic(frag_uv, mat, base_color);

        if (base_color.a < mat.alpha_cutoff)
        {
            discard;
        }
    }
}
