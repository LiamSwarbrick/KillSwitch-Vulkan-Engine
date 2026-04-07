#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require
#include "shared_constants.glsl"
#include "sampler_indices.glsl"

layout(set = 0, binding = 0) uniform texture2D global_textures[];
layout(set = 0, binding = 1) uniform sampler global_samplers[];

layout(push_constant, scalar) uniform PushConstants
{
    PushConstant_DrawCall dc_placeholder;  // TODO: If it turns out one of these is always placeholder, then i can remove the other and half the push constants size
    PushConstant_PassHeader pass;
} push;

layout(location = 0) in vec2 in_uv;

layout(location = 0) out vec4 frag_color;

void main()
{
    frag_color = texture(sampler2D(
        global_textures[nonuniformEXT(push.pass.texture_indices[0])],
        global_samplers[FG_SAMPLER_LINEAR_REPEAT]),
        in_uv
    );
}
