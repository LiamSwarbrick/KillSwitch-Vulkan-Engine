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

float luminance(vec3 c)
{
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

float karis_weight(vec3 c)
{
    // HDR is assumed linear here
    float luma = luminance(c);
    return 1.0 / (1.0 + luma);
}

void main()
{
    ivec2 src_size = textureSize(scene_texture, 0);
    vec2 texel = 1.0 / vec2(src_size);

    // Sample 2x2 neighborhood from full-res texture
    vec3 c0 = texture(scene_texture, v2f_uv + vec2(0.0, 0.0) * texel).rgb;
    vec3 c1 = texture(scene_texture, v2f_uv + vec2(texel.x, 0.0)).rgb;
    vec3 c2 = texture(scene_texture, v2f_uv + vec2(0.0, texel.y)).rgb;
    vec3 c3 = texture(scene_texture, v2f_uv + vec2(texel.x, texel.y)).rgb;

    // This stops black boxes forming from black pixels propagating
    c0 = max(c0, vec3(1e-4));
    c1 = max(c1, vec3(1e-4));
    c2 = max(c2, vec3(1e-4));
    c3 = max(c3, vec3(1e-4));

    // Karis average (firefly suppression)
    vec3 avg = (c0 + c1 + c2 + c3) * 0.25;
    avg *= karis_weight(avg);

    frag_color = vec4(avg, 1.0);

    // OLD:
    // // vec3 color = texture(scene_texture, v2f_uv, 0).rgb;
    // color = max(color, vec3(1e-4));

    // if (max(max(color.r, color.g), color.b) < 1.0)
    // {
    //     // Remember we don't discard here because the clear color is DONT_CARE. So just set to black
    //     frag_color =  vec4(vec3(0.0), 1.0);
    //     return;
    // }

    // frag_color = vec4(color, 1.0);
}
