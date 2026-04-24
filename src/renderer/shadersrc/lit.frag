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

// TODO: Use a spectral specular highlight for metal materials to create an rainbow shine
vec3 spectral_rainbow(float t)
{
    vec3 c = 0.5 + 0.5 * cos(6.28318 * (t + vec3(0.0, 0.33, 0.67)));
    return c;
}

vec3 ground_brdf(vec3 N, vec3 V, vec3 L)
{
    // Half-Lambert: https://developer.valvesoftware.com/wiki/Half_Lambert
    // NOTE: Not using half lambert anymore because it's too bright for that horror aestetic.
    //   half_lambert = dot(N, L) * 0.5 + 0.5
    //   lambert      = max(dot(N, L), 0.0)

    float brdf = dot(N, L) * 0.5 + 0.5;   // Half lambert
    return vec3(brdf);

    // float rim = pow(1.0 - half_lambert, 4.0);
    // brdf += rim;  // rim * spectral_rainbow(half_lambert);
}

vec3 toon_brdf(vec3 N, vec3 V, vec3 L)
{
    float toon;
    float x = dot(N, L);  //    Not clamping dot product so we can wrap it around more
    if (x < -0.4)         // <- (inspired by half lambert)
        toon = 0.05;
    else if (x < 0.25)
        toon = 0.2;
    else if (x < 0.80)
        toon = 0.66;
    else
        toon = 1.0;

    return vec3(toon);
}

#if 0
vec3 compute_direct_light(vec3 N, vec3 eye)
{
    vec3 V = normalize(eye - world_pos);


    vec3 light = vec3(0.0);


    // For each light
    // TODO: When adding lights in, also change the phase function in fog
    {
        

        // TEMP: 1 light, with lambert shading (meh) (replace later)
        vec3 light_pos = vec3(4.0, 2.0, 1.0);
        // vec3 light_pos = vec3(0.3, 1.5, 1.0);
        vec3 light_color = vec3(0.7, 0.7, 1.0);
        float light_intensity = 10.0;

        vec3 brdf;
        vec3 L = normalize(light_pos - world_pos);

        if (IS_CHARACTER)
            brdf = toon_brdf(N, V, L);
        else
            brdf = ground_brdf(N, V, L);

        vec3 point_light = brdf * light_color * light_intensity;
        float dist = length(light_pos - world_pos);
        point_light /= max(dist*dist, 1.0);
        light += point_light;
    }

    return light;
}
#endif

#if 0
vec3 compute_ambient_light(vec3 N)
// {return vec3(0.0);
{
    // NOTE: Not using ambient term anymore, it's too bright.
    // SH approximation: Sky color from top, Ground color from bottom
    vec3 sky_color = vec3(0.1, 0.1, 0.15);
    vec3 ground_color = vec3(0.05, 0.04, 0.03);
    float hemisphere = dot(N, vec3(0, 1, 0)) * 0.5 + 0.5;
    return mix(ground_color, sky_color, hemisphere);
}
#endif

float dither_threshold(vec2 screen_pos)
{
    // 4x4 Bayer Matrix
    const int bayer[16] = int[](
        0,  8,  2,  10,
        12, 4,  14, 6,
        3,  11, 1,  9,
        15, 7,  13, 5
    );
    
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
    float dither_scale = float(scene.rendertarget_size.y) / 270.0;  // <- Basically, dither as if 270p
    float dith_threshold = dither_threshold(gl_FragCoord.xy / dither_scale);
    vec4 base_color     = sample_texture2d_with_fallback(uv, mat.texture_idx_basecolor, mat.sampler_idx, mat.base_color);
    vec4 emissive_color = sample_texture2d_with_fallback(uv, mat.texture_idx_emissive,  mat.sampler_idx, vec4(mat.emissive_factor, 1.0));
    // base_color = vec4(1.0);
    
    // Shadow maps?
    // TODO

    // Lighting
    LightsData lighting = LightsBuffer(push.dc.lights_ptr).lights_data;

    vec3 N = normalize(world_normal);
    vec3 V = normalize(scene.cam_position - world_pos);
    
    vec3 direct_light = vec3(0.);
#if 1
    // NOTE: Only computing brdf for close enough lights
    //       But looping over all of them is not scalable (but no time for clusters at the mo)
    for (uint i = 0; i < lighting.num_point_lights; ++i)
    {
        PointLight pl = lighting.point_lights[i];

        vec3 frag_to_light = pl.pos_and_radius.xyz - world_pos;
        vec3 L = normalize(frag_to_light);

        if (length(frag_to_light) < pl.pos_and_radius.w)
        {
            vec3 brdf;
            if (IS_CHARACTER)
            {
                brdf = toon_brdf(N, V, L);
            }
            else
            {
                brdf = ground_brdf(N, V, L);
            }

            vec3 pl_radiance = brdf * pl.color_and_intensity.rgb * pl.color_and_intensity.a;
            float dist = length(pl.pos_and_radius.xyz - world_pos);
            pl_radiance *= get_attenuation(dist);//  /= max(dist*dist, 1.0);
            direct_light += pl_radiance;
        }
    }

    for (uint i = 0; i < lighting.num_spot_lights; ++i)
    {
        SpotLight pl = lighting.spot_lights[i];
        
    }
#endif
    vec3 ambient = vec3(0.);
    // vec3 ambient = compute_ambient_light(N);
    vec3 lit_rgb = (direct_light + ambient) * base_color.rgb + emissive_color.rgb;

    // Already sampled the textured with a differed offset
    // But need to slightly quantize the high frequency textures we are using
    // so that toon shading looks better
    const float color_levels = 4.0;  // How many "shades" the texture can have
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
