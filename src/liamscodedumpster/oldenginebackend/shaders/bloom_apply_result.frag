#version 460

#extension GL_EXT_scalar_block_layout : require

layout (location = 0) in vec2 v2f_uv;
layout (location = 0) out vec4 frag_color;

#include "../engine/shared_glsl_defs.h"

layout (scalar, set = 0, binding = 0) uniform SceneUniformBlock
{
    SceneBlock u_scene;
};
layout (set = 1, binding = 0) uniform sampler2D scene_texture;
layout (set = 1, binding = 1) uniform sampler2D bloom_texture;

void main()
{
    // Fullscreen triangle shader:
    vec3 scene_color = texture(scene_texture, v2f_uv).rgb;
    vec3 bloom_color = texture(bloom_texture, v2f_uv).rgb;

    vec3 color = scene_color + 1.0 * bloom_color;  // TODO: Could include u_scene.bloom_strength or something
    frag_color = vec4(color, 1.0);
}
