#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require
#include "shared_constants.glsl"

// Bindless heap for textures and samplers is the only use of descriptor sets here
layout(set = 0, binding = 0) uniform texture2D global_textures[];
layout(set = 0, binding = 1) uniform sampler global_samplers[];

layout(push_constant, scalar) uniform PushConstants
{
    GraphicsPushConstants pc;
};

layout(location = 0) in vec2 in_uv;
layout(location = 1) in vec3 in_vcolor;

layout(location = 0) out vec4 out_color;

void main()
{
    MaterialBuffer mb = MaterialBuffer(pc.material_ptr);
    MaterialData mat = mb.materials[pc.material_idx];

    vec4 final_color = mat.base_color * vec4(in_vcolor, 1.0);

    // Bindless Sampling
    if (mat.texture_idx_basecolor != 0xFFFFFFFF)  // UINT32_MAX for 'no texture'
    {
        // As per GL_EXT_nonuniform_qualifier
        // "nonuniformEXT()" is required when indexing a descriptor array with a dynamic variable
        final_color *= texture(sampler2D(
            global_textures[nonuniformEXT(mat.texture_idx_basecolor)],
            global_samplers[mat.sampler_idx]),
            in_uv
        );
    }

    if (CURRENT_BLEND_MODE == BLEND_MODE_MASKED)
    {
        if (final_color.a < mat.alpha_cutoff) {
            discard;
        }
    }

    out_color = final_color;
    // out_color = vec4(1.0, 0.0, 0.0, 1.0);
}
