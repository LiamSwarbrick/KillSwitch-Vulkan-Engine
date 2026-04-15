#version 460
#extension GL_GOOGLE_include_directive : require
#include "common/shared.glsl"
#include "common/shared_material_read.glsl"

layout (location = 0) in vec2 frag_uv;

layout (location = 0) out vec4 frag_color;

void main()
{
    // NOTE: Blit pass puts the texture it blit's in pass.texture_indices[0]
    //       and samples with the linear sampler
    frag_color = texture(sampler2D(
        global_textures[nonuniformEXT(push.pass.texture_indices[0])],
        global_samplers[FG_SAMPLER_LINEAR_REPEAT]),
        frag_uv
    );
}
