#version 450

// NOTE: If we combine scalar block with intelligently order the
// structure parameters for correct alignment of each variable
// we will get both optimal packing and correct alignment.
#extension GL_EXT_scalar_block_layout : require

#define VS_GEOMETRY_UNIFORMS
#define VS_GEOMETRY_IO
#include "../engine/shared_glsl_defs.h"

// Temp for fun: Apply leaf sway to the alpha masked folliage.
layout (constant_id = 0) const bool IS_ALPHA_MASKED = false;


vec4 apply_leaf_sway(vec4 pos)
{
    // A deterministic per-leaf variation source:
    float variation = fract(sin(dot(pos.xy, vec2(12.9898, 78.233))) * 43758.5453);

    // Each leaf gets its own time offset
    float phase = variation * 6.2831;  // 2pi

    // Sway pattern (sin + cos for nicer motion)
    const float sway_freq = 3.0;
    const float sway_amp  = 0.2;
    float sway_x = sin(u_scene.time * sway_freq + phase);
    float sway_y = cos(u_scene.time * (sway_freq * 0.8) + phase);

    return pos + vec4(sway_x, sway_y, 0.0, 0.0) * sway_amp;
}

void main()
{
    v2f_texcoord = v_texcoord;
    
    vec4 p = pcs.model * vec4(v_position, 1.0);

    // NOTE: Temp for fun: Apply leaf sway to the alpha masked folliage.
    if (IS_ALPHA_MASKED)  p = apply_leaf_sway(p);

    v2f_worldpos = vec3(p);
    gl_Position  = u_scene.view_projection * p;

    // Normal mapping. NOTE: Not using normal_matrix = inv(trans(model)) because I'm assuming orthonormal model matrix:
    v2f_tbn_normal    = normalize((pcs.model * vec4(v_normal,      0.0)).xyz);
    v2f_tbn_tangent   = normalize((pcs.model * vec4(v_tangent.xyz, 0.0)).xyz);
    v2f_bitangent_sign = v_tangent.w;
}
