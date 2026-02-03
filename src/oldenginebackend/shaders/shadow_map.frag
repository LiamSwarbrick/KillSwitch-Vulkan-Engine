#version 450

#extension GL_EXT_scalar_block_layout : require

#define FS_GEOMETRY_UNIFORMS
#include "../engine/shared_glsl_defs.h"

layout (location = 0) in vec2 v2f_texcoord;

layout (constant_id = 0) const bool IS_ALPHA_MASKED = false;

void main()
{
    if (IS_ALPHA_MASKED)
    {
        vec4 albedo_alpha  = texture(rgba_albedo_alpha_map, v2f_texcoord);

        // Alpha Masking:
        if (albedo_alpha.a < 0.5)
        {
            discard;
        }
    }
}
