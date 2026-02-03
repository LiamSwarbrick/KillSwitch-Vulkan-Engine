#version 450

#extension GL_EXT_scalar_block_layout : require

#define VS_GEOMETRY_UNIFORMS
#include "../engine/shared_glsl_defs.h"

layout (location = 0) in vec3 v_position;
layout (location = 2) in vec2 v_texcoord;  // Needed for alpha masked shadows

layout (location = 0) out vec2 v2f_texcoord;

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
    if (IS_ALPHA_MASKED)  p = apply_leaf_sway(p);  // NOTE: Temp for fun: Apply leaf sway to the alpha masked folliage.

    gl_Position  = u_scene.view_projection * p;
}
