#version 450

#extension GL_EXT_scalar_block_layout : require

#define FS_GEOMETRY_UNIFORMS
#define FS_GEOMETRY_IN
#include "../engine/shared_glsl_defs.h"


// Write to G-Buffer Attachments
layout (location = 0) out vec4 out_albedo_roughness;  // (UNORM)  rgb:albedo,   a=roughness
layout (location = 1) out vec4 out_normal_metalness;  // (UNORM)  rgb:normal,   a=metal
layout (location = 2) out vec4 out_emissive_ao;       // (SFLOAT) rgb:emissive, a=ambient occlusion


void main()
{
    vec4 albedo_alpha  = texture(rgba_albedo_alpha_map,          v2f_texcoord);
    vec3 rma           = texture(rgb_roughness_metalness_ao_map, v2f_texcoord).rgb;
    vec3 emissive      = texture(rgb_emissive_map,               v2f_texcoord).rgb;
    vec3 normal = normal_mapped_normal();

    // Write to G-Buffers
    out_albedo_roughness = vec4(albedo_alpha.rgb, rma.r);
    // out_normal_metalness = vec4((normal + vec3(1.0)) / 2.0, rma.g);  // Pack from [-1..1] to [0..1]  // UNORM
    out_normal_metalness = vec4(normal, rma.g);  // SNORM
    out_emissive_ao      = vec4(emissive, rma.b);
}
