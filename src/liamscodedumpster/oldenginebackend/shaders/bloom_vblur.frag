#version 460

#extension GL_EXT_scalar_block_layout : require

layout (location = 0) in vec2 v2f_uv;
layout (location = 0) out vec4 frag_color;

#include "../engine/shared_glsl_defs.h"

layout (scalar, set = 0, binding = 0) uniform SceneUniformBlock
{
    SceneBlock u_scene;
};

layout (set = 1, binding = 0) uniform sampler2D hblured_texture;

// Weights generated from ./bloom_print_weights.py
// 43-wide Gaussian blur, linear-sampling optimised
// 1 center tap + 10 paired taps per side = 43 taps total
const float CENTER_WEIGHT = 0.04508630619632941;
const int PAIRED_WEIGHT_COUNT = 10;
const float offsets[PAIRED_WEIGHT_COUNT] = float[](
    1.495370502671207,
    3.489199211318705,
    5.483031210517226,
    7.476868375857014,
    9.47071257664205,
    11.464565673628172,
    13.4584295167832,
    15.452305943074828,
    17.446196774291817,
    19.440103814903964
);
const float weights[PAIRED_WEIGHT_COUNT] = float[](
    0.08879554825835087,
    0.08349599141096506,
    0.07474111598731425,
    0.06369025752381112,
    0.051666119077179884,
    0.03989863148114717,
    0.029331169321237492,
    0.020526733663644915,
    0.013675060804411582,
    0.008672762806733876
);


void main()
{
    float texel_height = 1.0 / textureSize(hblured_texture, 0).y;

    // Center tap
    vec3 color = texture(hblured_texture, v2f_uv).rgb * CENTER_WEIGHT;

    // Paired taps
    for (int i = 0; i < PAIRED_WEIGHT_COUNT; ++i)
    {
        vec2 offset = vec2(0.0, offsets[i] * texel_height);

        vec3 sample_pos_side = texture(hblured_texture, v2f_uv + offset).rgb;
        vec3 sample_neg_side = texture(hblured_texture, v2f_uv - offset).rgb;

        color += (sample_pos_side + sample_neg_side) * weights[i];
    }

    frag_color = vec4(color, 1.0);
}
