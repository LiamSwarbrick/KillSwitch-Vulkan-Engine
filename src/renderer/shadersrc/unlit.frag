#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require
#include "shared_constants.glsl"

// Bindless Texture Heap (Set 0, Binding 0)
layout(set = 0, binding = 0) uniform sampler2D global_textures[];

struct MaterialData
{
    vec4 base_color;
    uint32_t texture_idx;
    float alpha_cutoff;
    uint32_t padding[2];
};

layout(buffer_reference, scalar) readonly buffer MaterialBuffer { MaterialData materials[]; };

layout(push_constant, scalar) uniform PushConstants
{
    uint64_t global_ptr;
    uint64_t object_ptr;
    uint64_t vertex_ptr;
    uint64_t joint_ptr;
    uint64_t material_ptr;
    uint32_t material_idx;
} pc;

layout(location = 0) in vec2 in_uv;
layout(location = 1) in vec4 in_vcolor;

layout(location = 0) out vec4 out_color;

void main()
{
    MaterialBuffer mb = MaterialBuffer(pc.material_ptr);
    MaterialData mat = mb.materials[pc.material_idx];

    vec4 final_color = mat.base_color * in_vcolor;

    // Bindless Sampling
    if (mat.texture_idx != 0xFFFFFFFF)  // UINT32_MAX for 'no texture'
    {
        // GL_EXT_nonuniform_qualifier's nonuniformEXT() is required when indexing a descriptor array with a dynamic variable
        final_color *= texture(global_textures[nonuniformEXT(mat.texture_idx)], in_uv);
    }

    if (CURRENT_BLEND_MODE == BLEND_MODE_MASKED)
    {
        if (final_color.a < mat.alpha_cutoff) {
            discard;
        }
    }

    out_color = final_color;
}
