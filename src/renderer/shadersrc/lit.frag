#version 460
#extension GL_GOOGLE_include_directive : require
#include "common/shared.glsl"
#include "common/shared_material_read.glsl"

layout (location = 0) in vec2 uv;
layout (location = 1) in vec3 color;
layout (location = 2) in vec3 world_pos;
layout (location = 3) in vec3 world_normal;

layout (location = 0) out vec4 out_color;

// vec3 spectral_rainbow(float t)
// {
//     vec3 c = 0.5 + 0.5 * cos(6.28318 * (t + vec3(0.0, 0.33, 0.67)));
//     return c;
// }

#if 1
vec3 ground_brdf(vec3 N, vec3 V, vec3 light_pos)
{
    // Half-Lambert: https://developer.valvesoftware.com/wiki/Half_Lambert

    vec3 L = normalize(light_pos - world_pos);
    float half_lambert = max(dot(N, L), 0.0) * 0.5 + 0.5;
    float rim = 1.0 - max(dot(V, N), 0.0);
    rim = pow(rim, 3.0);

    return vec3(half_lambert + rim);
}

vec3 toon_brdf(vec3 N, vec3 V, vec3 light_pos)
{
    // Half-Lambert: https://developer.valvesoftware.com/wiki/Half_Lambert

    vec3 L = normalize(light_pos - world_pos);
    float half_lambert = max(dot(N, L), 0.0) * 0.5 + 0.5;

    // Create 3 distinct bands of light
    float levels = 3.0;
    float toon = ceil(half_lambert * levels) / levels;
    
    // Add a tiny bit of "rim" light logic
    float rim = 1.0 - max(dot(V, N), 0.0);
    rim = pow(rim, 4.0) * toon;  // Only show rim in lit areas

    return vec3(toon + rim * 0.5);
}
#endif

vec3 compute_direct_light(bool is_character, vec3 N, vec3 eye)
{
    vec3 light = vec3(0.0);

    // For each light
    {
        vec3 V = normalize(eye - world_pos);

        // TEMP: 1 light, with lambert shading (meh) (replace later)
        vec3 light_pos = vec3(1.0, 2.0, 1.0);
        vec3 light_color = vec3(0.7, 0.7, 1.0);
        float light_intensity = 1.0;

        vec3 brdf;
        if (is_character)
        {
            brdf = toon_brdf(N, V, light_pos);
            // brdf = ground_brdf(N, V, light_pos);
        }
        else
        {
            brdf = ground_brdf(N, V, light_pos);
        }

        vec3 point_light = brdf * light_color * light_intensity;
        float dist = length(light_pos - world_pos);
        point_light /= max(dist*dist, 1.0);
        light += point_light;
    }

    return light;
}

vec3 compute_ambient_light(bool is_character, vec3 N)
// {return vec3(0.0);
{
    // Simple SH approximation: Sky color from top, Ground color from bottom
    vec3 sky_color = vec3(0.1, 0.1, 0.15);
    vec3 ground_color = vec3(0.05, 0.04, 0.03);
    float hemisphere = dot(N, vec3(0, 1, 0)) * 0.5 + 0.5;
    return mix(ground_color, sky_color, hemisphere);
}

float dither_threshold(vec2 screen_pos)
{
    // 4x4 Bayer Matrix
    const int bayer[16] = int[](
        0,  8,  2,  10,
        12, 4,  14, 6,
        3,  11, 1,  9,
        15, 7,  13, 5
    );
    
    // Map screen coordinates to 0-3 range
    int x = int(mod(screen_pos.x, 4.0));
    int y = int(mod(screen_pos.y, 4.0));
    
    return float(bayer[x + y * 4]) / 16.0;
}

void main()
{
    SceneData scene  = SceneBuffer(push.dc.scene_ptr).scene;

    MaterialData mat;
    vec4 base_color;
    sample_material_basic(uv, mat, base_color);

    bool is_character = push.dc.joints_ptr != 0;
    is_character = !is_character;  // TEMP
    if (is_character)
    {
        base_color.rgb = vec3(1.0);
    }

    vec3 N = normalize(world_normal);
    vec3 direct_light = compute_direct_light(is_character, N, scene.cam_position);
    vec3 ambient = compute_ambient_light(is_character, N);

    vec3 lit_rgb = (direct_light + ambient);
    if (is_character)
    {
        float threshold = dither_threshold(gl_FragCoord.xy / 4.0);
        const float levels = 3.0;
        float quantized = floor(direct_light.r * levels) / levels;
        float remainder = fract(direct_light.r * levels);
        float dithered_light = quantized + (remainder > threshold ? (1.0 / levels) : 0.0);
        lit_rgb *= dithered_light;
    }
    lit_rgb *= base_color.rgb;

    vec4 final_color = vec4(lit_rgb, 1.0);
    out_color = vec4(
        final_color.rgb,
        process_alpha(final_color.a, mat.alpha_cutoff)
    );

    // Normal shading
    // out_color = vec4((N + 1.)/2., 1.0);
}
