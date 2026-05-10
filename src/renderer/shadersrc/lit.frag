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

// Half-Lambert: https://developer.valvesoftware.com/wiki/Half_Lambert
//   half_lambert = dot(N, L) * 0.5 + 0.5  (sometimes it's squared though)
//   lambert      = max(dot(N, L), 0.0)

vec3 ground_brdf(vec3 N, vec3 V, vec3 L)
{
    float NdotL = dot(N, L);
    float lambert = max(NdotL, 0.0);
    return vec3(lambert);

    // // float rim = pow(1.0 - lambert, 4.0);
    // return vec3(lambert);// + (rim * spectral_rainbow(lambert));
    // float NdotV = dot(N, V);
    // float view_rim = pow(1.0 - max(NdotV, 0.0), 3.0);
    // float light_mask = smoothstep(-0.2, 0.5, NdotL);
    // float final_rim = view_rim * light_mask;


    // float brdf = NdotL*NdotL * 0.5 + 0.5;   // Half lambert
    // float rim = pow(1.0 - lambert, 4.0);
    // return vec3(lambert) + rim * spectral_rainbow(lambert);
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
    float z_linear,
    float dither
)
{
    const float fog_start = 3.0;
    const float fog_length = 20.0;
    
    float fog = clamp((z_linear - fog_start) / fog_length, 0.0, 1.0);
    fog *= 0.5;
    float fog_step = fog > dither ? 1.0 : 0.0;

    vec3 fog_color = vec3(0.005, 0.005, 0.025);
    // vec3 fog_color = vec3(0.005, 0.007, 0.03);
    vec3 fogged_color = fog_color * fog_step;

    return mix(color, fogged_color, fog);
}

void main()
{
#if 1
    SceneData scene  = SceneBuffer(push.dc.scene_ptr).scene;

    MaterialBuffer mb = MaterialBuffer(push.dc.material_ptr);
    MaterialData mat = mb.materials[push.dc.material_idx];

    // Dithering (not a postprocess pass bcuz it's not about pixely rendering)
    float dither_scale = float(scene.rendertarget_size.y) / 270.0;  // <- Basically, dither as if 270p
    float dith_threshold = dither_threshold(gl_FragCoord.xy / dither_scale);
    vec4 base_color     = sample_texture2d_with_fallback(uv, mat.texture_idx_basecolor, mat.sampler_idx, mat.base_color);
    vec4 emissive_color = sample_texture2d_with_fallback(uv, mat.texture_idx_emissive,  mat.sampler_idx, vec4(mat.emissive_factor, 0.0));
    // base_color = vec4(1.0);

    if (!IS_CHARACTER)
    {
        float variation = sin(world_pos.x * 0.1) * cos(world_pos.z * 0.1);
        float sat_factor = 1.0 + (variation * 0.5); 
        float luma = dot(base_color.rgb, vec3(0.2126, 0.7152, 0.0722));
        base_color.rgb = mix(vec3(luma), base_color.rgb, sat_factor);

        base_color.rgb *= mix(1.0, 1.5, sat_factor);
    }
    
    // Lighting
    vec3 N = normalize(world_normal);
    vec3 V = normalize(scene.cam_position - world_pos);

    vec3 direct_light = vec3(0.);

    LightsHeader header = LightsHeaderBuffer(push.dc.lights_header_ptr).header;
    PointLightBuffer pl_buf = PointLightBuffer(header.point_lights_ptr);
    SpotLightBuffer sl_buf  = SpotLightBuffer(header.spot_lights_ptr);

    // Multiple shadow maps
    SpotLightShadowMapIndexBuffer sl_shadow_map_indices = SpotLightShadowMapIndexBuffer(header.spotlight_shadowmap_index_buf_ptr);
    ShadowMapSpotLightCamerasBuffer shadow_map_sl_cameras = ShadowMapSpotLightCamerasBuffer(header.shadowmap_spotlight_camera_buf_ptr);

    // Clustered shading buffers
    PointLightIndicesBuffer pl_indices = PointLightIndicesBuffer(header.point_light_indices_buf_ptr);
    SpotLightIndicesBuffer sl_indices  = SpotLightIndicesBuffer(header.spot_light_indices_buf_ptr);
    ClusterOffsetsBuffer clusters      = ClusterOffsetsBuffer(header.cluster_offsets_buf_ptr);

    // Get which 2D tile our fragment is in
    vec2 screen_uv = gl_FragCoord.xy / vec2(scene.rendertarget_size);
    uint tile_x = uint(screen_uv.x * float(CLUSTER_GRID_SIZE_X));
    uint tile_y = uint(screen_uv.y * float(CLUSTER_GRID_SIZE_Y));

    vec4 view_pos = scene.view * vec4(world_pos, 1.0);
    float depth =  abs(-view_pos.z);  // NOTE: DO NOT NEED abs() if we actually use LookAtRH(), but some of the code used LookAt and it took me hours to find this bug, so I'm keeping abs() in case  this shit happens again
    depth = clamp(depth, scene.near_plane, scene.far_plane);

    // Get z bin (clusters get exponentially bigger away from the camera)
    float log_z = log(depth / scene.near_plane) * scene.inv_log_far_over_near;
    uint tile_z = uint(log_z * float(CLUSTER_GRID_SIZE_Z));

    tile_x = clamp(tile_x, 0u, CLUSTER_GRID_SIZE_X - 1u);
    tile_y = clamp(tile_y, 0u, CLUSTER_GRID_SIZE_Y - 1u);
    tile_z = clamp(tile_z, 0u, CLUSTER_GRID_SIZE_Z - 1u);
    
    // Fetch cluster
    uint cluster_index = CLUSTER_INDEX(tile_x, tile_y, tile_z);
    Cluster cluster = clusters.clusters[cluster_index];

    if (DEBUG_RENDERMODE == DEBUG_RENDERMODE_CLUSTERED_SHADING_HEATMAP)
    {
        uint light_count = cluster.point_count + cluster.spot_count;
        float intensity = float(light_count) / 16.0; // tune max expected lights per cluster
        intensity = clamp(intensity, 0.0, 1.0);
        vec3 blue = vec3(0.0, 0.0, 1.0);
        vec3 red  = vec3(1.0, 0.0, 0.0);
        vec3 debug_color = mix(blue, red, intensity);
        out_color = vec4(debug_color, 1.0);
        return;
    }
    else if (DEBUG_RENDERMODE == DEBUG_RENDERMODE_CLUSTERED_SHADING_CLUSTERS)
    {
        vec3 grid = vec3(
            float(tile_x) / float(CLUSTER_GRID_SIZE_X),
            float(tile_y) / float(CLUSTER_GRID_SIZE_Y),
            float(tile_z) / float(CLUSTER_GRID_SIZE_Z)
        );
        out_color = vec4(grid, 1.0);
        return;
    }

    const uint max_lights_per_pixel = MAX_LIGHTS_PER_CLUSTER;
    
    for (uint i = 0; i < min(cluster.point_count, max_lights_per_pixel); ++i)
    {
        uint light_index = pl_indices.indices[cluster.point_offset + i];
        PointLight pl = pl_buf.point_lights[light_index];

        vec3 frag_to_light = pl.pos_and_radius.xyz - world_pos;
        float dist = length(frag_to_light);
        
        if (dist >= pl.pos_and_radius.w)
            continue;
        
        vec3 L = normalize(frag_to_light);
        vec3 brdf;
        if (IS_CHARACTER)
        {
            brdf = toon_brdf(N, V, L);
        }
        else
        {
            brdf = ground_brdf(N, V, L);
        }

        float attenuation = get_attenuation(dist, pl.pos_and_radius.w);
        vec3 radiance = brdf * pl.color_and_intensity.rgb * pl.color_and_intensity.a * attenuation;
        direct_light += radiance;
    }
    
    for (uint i = 0; i < min(cluster.spot_count, max_lights_per_pixel); ++i)
    {
        uint light_index = sl_indices.indices[cluster.spot_offset + i];
        SpotLight sl = sl_buf.spot_lights[light_index];

        float shadow_factor = 1.0;
        int spotlight_shadowmap_index = sl_shadow_map_indices.spotlight_shadowmap_index[light_index];  // One slot per light, with -1 set when spotlight i does not have a shadowmap
        if (spotlight_shadowmap_index >= 0)  // If shadow map is available for this light
        {
            // SHADOW MAPPING
            mat4 spotlight_view_proj = shadow_map_sl_cameras.shadowmap_spotlight_viewproj[spotlight_shadowmap_index];
            vec4 shadow_coord = spotlight_view_proj * vec4(world_pos, 1.0);
            shadow_coord.z -= 0.00025 * shadow_coord.w;  // Prevent shadow acne with a small bias (beware of peter panning)

            // NDC [-1,1] to [0,1]
            shadow_coord.xy = 0.5 * (shadow_coord.xy + shadow_coord.w);

            uint32_t shadowmap_texture_idx = push.pass.texture_indices[spotlight_shadowmap_index];
            shadow_factor = textureProj(sampler2DShadow(
                global_textures[nonuniformEXT(shadowmap_texture_idx)],
                global_samplers[FG_SAMPLER_SHADOW]),
                shadow_coord
            );
        }

        vec3 frag_to_light = sl.pos_and_radius.xyz - world_pos;
        float dist = length(frag_to_light);
        
        if (dist >= sl.pos_and_radius.w)
            continue;

        vec3 L = normalize(frag_to_light);
        vec3 light_dir = normalize(sl.direction);
        float cos_theta = dot(-L, light_dir);
        float inner = cos(sl.inner_cone_angle);
        float outer = cos(sl.outer_cone_angle);
        float angular_attenuation = smoothstep(outer, inner, cos_theta);

        // Early out if outside cone
        if (angular_attenuation <= 0.0)
            continue;
        
        vec3 brdf;
        if (IS_CHARACTER)
        {
            brdf = toon_brdf(N, V, L);
        }
        else
        {
            brdf = ground_brdf(N, V, L);
        }

        float attenuation = get_attenuation(dist, sl.pos_and_radius.w);
        vec3 radiance =
            brdf *
            sl.color_and_intensity.rgb *
            sl.color_and_intensity.a *
            attenuation *
            angular_attenuation;

        direct_light += radiance * shadow_factor;
    }

    vec3 ambient = vec3(0.00);
    // vec3 ambient = compute_ambient_light(N);
    vec3 lit_rgb = (direct_light + ambient) * base_color.rgb + emissive_color.rgb;

    // Already sampled the textured with a differed offset
    // But need to slightly quantize the high frequency textures we are using
    // so that toon shading looks better
    const float color_levels = 4.0;  // How many "shades" the texture can have
    vec3 tex_quantized = floor(base_color.rgb * color_levels) / color_levels;
    vec3 tex_remainder = fract(base_color.rgb * color_levels);

    // Alternative look which makes the lit regions more high fidelity. Maybe make it an option?
    // vec3 tex_quantized = floor(lit_rgb * color_levels) / color_levels;
    // vec3 tex_remainder = fract(lit_rgb * color_levels);

    vec3 dithered_tex;
    dithered_tex.r = tex_quantized.r + (tex_remainder.r > dith_threshold ? (1.0 / color_levels) : 0.0);
    dithered_tex.g = tex_quantized.g + (tex_remainder.g > dith_threshold ? (1.0 / color_levels) : 0.0);
    dithered_tex.b = tex_quantized.b + (tex_remainder.b > dith_threshold ? (1.0 / color_levels) : 0.0);
    lit_rgb *= dithered_tex;

    // Fog
    lit_rgb = apply_dithered_fog(
        lit_rgb,
        depth,
        dith_threshold
    );

    vec4 final_color = vec4(lit_rgb, 1.0);
    out_color = vec4(
        final_color.rgb,
        process_alpha(final_color.a, mat.alpha_cutoff)
    );

    // out_color += vec4(1.0);
#else
    // Normal shading
    out_color = vec4((normalize(world_normal) + 1.)/2., 1.0);
#endif
}
