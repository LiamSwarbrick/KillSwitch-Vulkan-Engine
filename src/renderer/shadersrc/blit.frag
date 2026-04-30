#version 460
#extension GL_GOOGLE_include_directive : require
#include "common/shared.glsl"

#extension GL_EXT_samplerless_texture_functions : require

layout (location = 0) in vec2 frag_uv;

layout (location = 0) out vec4 out_color;

void main()
{
    // NOTE: Blit pass puts the texture it blit's in pass.texture_indices[0]
    //       and samples with the linear sampler
    out_color = texture(sampler2D(
        global_textures[nonuniformEXT(push.pass.texture_indices[0])],
        global_samplers[FG_SAMPLER_LINEAR_BLACK_BORDER]),
        frag_uv
    );
}
