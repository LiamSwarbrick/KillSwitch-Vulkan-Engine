#version 460
#extension GL_GOOGLE_include_directive : require
#include "common/shared.glsl"
#include "common/shared_material_read.glsl"

layout (location = 0) in vec2 uv;
layout (location = 1) in vec3 color;
layout (location = 2) in vec3 world_pos;
layout (location = 3) in vec3 world_normal;

layout (location = 0) out vec4 out_color;

#define IS_CHARACTER (CURRENT_VERTEX_TYPE == VERTEX_TYPE_SKINNED)

vec3 spectral_rainbow(float t)
{
    vec3 c = 0.5 + 0.5 * cos(6.28318 * (t + vec3(0.0, 0.33, 0.67)));
    return c;
}


vec3 ground_brdf(vec3 N, vec3 V, vec3 light_pos)
{
    vec3 L = normalize(light_pos - world_pos);
    float half_lambert = max(dot(N, L), 0.0);// * 0.5 + 0.5;
    float rim = 1.0 - max(dot(V, N), 0.0);
    rim = pow(rim, 4.0);

    return vec3(half_lambert) + rim * spectral_rainbow(half_lambert);
}

vec3 toon_brdf(vec3 N, vec3 V, vec3 light_pos)
{
    // Half-Lambert: https://developer.valvesoftware.com/wiki/Half_Lambert
    // NOTE: Not using half lambert anymore because it's better for that horror aestetic.

    vec3 L = normalize(light_pos - world_pos);
    // float half_lambert = max(dot(N, L), 0.0);  // * 0.5 + 0.5

    float toon;
    float x = dot(N, L);
    // if (x < -0.4)
    //     toon = 0.0;
    // else if (x < 0.05)
    //     toon = 0.05;
    if (x < -0.4)
        toon = 0.05;
    else if (x < 0.25)
        toon = 0.2;
    else if (x < 0.80)
        toon = 0.66;
    else
        toon = 1.0;

    
    return vec3(toon);

    // // Add a tiny bit of rim light
    // float rim = 1.0 - max(dot(V, N), 0.0);
    // rim = pow(rim, 4.0);

    // rim = 0.0;
    // return vec3(toon + rim * 0.5);
    // return abs(vec3(toon) - rim * (1.0 - toon) * spectral_rainbow(1.0 - rim));
    // return vec3(toon) * mix(vec3(1.0), mix(spectral_rainbow(toon), spectral_rainbow(rim), toon), 1.0 * rim);
    // return vec3(toon) * mix(vec3(1.0), 0.5 + 0.5 * spectral_rainbow(rim), 1.0 * rim);
}

vec3 compute_direct_light(vec3 N, vec3 eye)
{
    vec3 light = vec3(0.0);

    // For each light
    // TODO: When adding lights in, also change the phase function in fog
    {
        vec3 V = normalize(eye - world_pos);

        // TEMP: 1 light, with lambert shading (meh) (replace later)
        vec3 light_pos = vec3(4.0, 2.0, 1.0);
        // vec3 light_pos = vec3(0.3, 1.5, 1.0);
        vec3 light_color = vec3(0.7, 0.7, 1.0);
        float light_intensity = 10.0;

        vec3 brdf;
        if (IS_CHARACTER)
        {
            brdf = toon_brdf(N, V, light_pos);
            // brdf = ground_brdf(N, V, light_pos);
        }
        else
        {
            // brdf = toon_brdf(N, V, light_pos);
            brdf = ground_brdf(N, V, light_pos);
        }

        vec3 point_light = brdf * light_color * light_intensity;
        float dist = length(light_pos - world_pos);
        point_light /= max(dist*dist, 1.0);
        light += point_light;
    }

    return light;
}

vec3 compute_ambient_light(vec3 N)
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

vec3 apply_dithered_fog(
    vec3 color,
    float depth,
    float near,
    float far,
    float dither
)
{
    float z_linear = (near * far) / (far - depth * (far - near));
    float fog = clamp((z_linear - 2.0) / 25.0, 0.0, 1.0);
    // float fog_step = fog > dither ? 1.0 : 0.0;
    // float fog_step = fog;

    // NOTE: I want colors to stay bright so that bright lights
    //       look good in the foggy distance (works better for HDR and bloom)

    // float fog = mix(0.0, 1.0, max((z_linear - 6.)/50.0, 0.0));
    // float fog = mix(0.0, 1.0, max((z_linear - 2.)/20.0, 0.0));
    float fog_step = fog > dither ? 1.0 : 0.0;

    vec3 fog_tint = vec3(0.005, 0.005, 0.02);
    vec3 fogged_color = fog_tint * fog_step;

    return mix(color, fogged_color, fog);
}

void main()
{
    SceneData scene  = SceneBuffer(push.dc.scene_ptr).scene;

    MaterialBuffer mb = MaterialBuffer(push.dc.material_ptr);
    MaterialData mat = mb.materials[push.dc.material_idx];

    // Dithering (not a postprocess pass bcuz it's not about pixely rendering)
    vec2 st = uv;
    float dith_threshold = dither_threshold(gl_FragCoord.xy / 4.0);
    vec2 dither_uv_offset = (vec2(dith_threshold) - 0.5) * (1.0 / texture2d_size(mat.texture_idx_basecolor));
    st += dither_uv_offset;

    vec4 base_color     = sample_texture2d_with_fallback(st, mat.texture_idx_basecolor, mat.sampler_idx, mat.base_color);
    vec4 emissive_color = sample_texture2d_with_fallback(st, mat.texture_idx_emissive,  mat.sampler_idx, vec4(mat.emissive_factor, 1.0));
    // base_color = vec4(1.0);

    vec3 N = normalize(world_normal);
    const vec3 direct_light = compute_direct_light(N, scene.cam_position);
    vec3 ambient = vec3(0.);
    // vec3 ambient = compute_ambient_light(N);
    vec3 lit_rgb = (direct_light + ambient) * base_color.rgb + emissive_color.rgb;

    const float color_levels = 4.0; // How many "shades" the texture can have
    vec3 tex_quantized = floor(base_color.rgb * color_levels) / color_levels;
    vec3 tex_remainder = fract(base_color.rgb * color_levels);

    vec3 dithered_tex;
    dithered_tex.r = tex_quantized.r + (tex_remainder.r > dith_threshold ? (1.0 / color_levels) : 0.0);
    dithered_tex.g = tex_quantized.g + (tex_remainder.g > dith_threshold ? (1.0 / color_levels) : 0.0);
    dithered_tex.b = tex_quantized.b + (tex_remainder.b > dith_threshold ? (1.0 / color_levels) : 0.0);
    lit_rgb *= dithered_tex;

    // Fog
    lit_rgb = apply_dithered_fog(
        lit_rgb,
        gl_FragCoord.z,
        scene.near_plane,
        scene.far_plane,
        dith_threshold
    );

    vec4 final_color = vec4(lit_rgb, 1.0);
    out_color = vec4(
        final_color.rgb,
        process_alpha(final_color.a, mat.alpha_cutoff)
    );

    // Normal shading
    // out_color = vec4((N + 1.)/2., 1.0);
}
