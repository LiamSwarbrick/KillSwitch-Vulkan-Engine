#version 460

#extension GL_EXT_scalar_block_layout : require

#define FS_DEFERRED_LIGHTING_IN
#define FS_PBR_BRDF
#include "../engine/shared_glsl_defs.h"


// Write to render-target
layout (location = 0) out vec4 out_color;


void main()
{
    float depth = texture(depth_texture, v2f_uv).r;

    // Set background to clear colour (since we gotta do this manually in deferred rendering)
    if (depth >= 1.0)
    {
        out_color = vec4(0.392, 0.584, 0.929, 1.0);
        return;
    }

    vec4 albedo_roughness = texture(albedo_roughness_texture, v2f_uv);  // R8G8B8A8_UNORM
    vec4 normal_metalness = texture(normal_metalness_texture, v2f_uv);  // A2B10G10R10_UNORM format!
    vec4 emissive_ao      = texture(emissive_ao_texture,      v2f_uv);  // R16G16B16A16_SFLOAT

    vec3  albedo     = albedo_roughness.rgb;
    float roughness  = albedo_roughness.a;
    // vec3  normal     = normal_metalness.rgb * 2.0 - vec3(1.0);  // <- Unpacks the normal from [0..1] to [-1..1]  // UNORM
    vec3  normal     = normal_metalness.rgb;  // SNORM
    float metalness  = normal_metalness.a;
    vec3  emissive   = emissive_ao.rgb;
    float ambient_occlusion = emissive_ao.a;

    // Reconstruct world pos from depth buffer and frag coord (the uv from the fullscreen triangle)
    vec2 screen_ndcs = v2f_uv * 2.0 - vec2(1.0);
    vec4 ndc_pos = vec4(screen_ndcs, depth, 1.0);
    vec4 world_pos_homogeneous = inverse(u_scene.view_projection) * ndc_pos;
    vec3 world_pos = world_pos_homogeneous.xyz / world_pos_homogeneous.w;

    // Lighting
    vec3 radiance = vec3(0.0);

    // TODO: Cleanup shadow mapping code so it isn't duplicated between forward and deferred.
    // However for now, since it's just a single shadow map applied to a hardcoded light instead of one in the lights array,
    // it is not yet ready for such an abstraction.

    // Shadow mapping the single spotlight
    {
        vec4 shadow_coord = u_scene.spotlight_view_projection * vec4(world_pos, 1.0);
        shadow_coord.z -= 0.0005 * shadow_coord.w;   // Prevent shadow acne with a small bias

        // Multi-sample for even higher than the hardware 2x2 PCF-filtering of the shadow map
        vec2 texelSize = 1.0 / vec2(textureSize(shadow_map, 0));
        float shadow_factor = 0.0;
        const int range = 1;
        for (int x = -range; x <= range; x++)
        {
            for (int y = -range; y <= range; y++)
            {
                vec4 offset_coord = shadow_coord;
                offset_coord.xy += vec2(x, y) * texelSize * shadow_coord.w; 
                shadow_factor += textureProj(shadow_map, offset_coord);  // Each call here uses the hardware 2x2 PCF
            }
        }
        int num_samples_along = (range - -range) + 1;
        shadow_factor /= num_samples_along * num_samples_along;  // Average the results (for range=1, we took 9 samples)

        // // Single sample shadow map:
        // float shadow_factor = textureProj(shadow_map, shadow_coord);

        radiance += shadow_factor * spotlight_radiance(
            u_scene.spotlight,
            world_pos,
            u_scene.camera_world_pos.xyz,
            normal,
            albedo,
            roughness,
            metalness
        );
    }

    for (uint i = 0; i < u_lights.point_light_count; ++i)
    {
        radiance += lighting_with_brdf_lambert_beckmann(
            u_lights.point_lights[i],
            world_pos,
            u_scene.camera_world_pos.xyz,
            normal,
            albedo,
            roughness,
            metalness
        );
    }

    const float EMISSIVE_BLOOM_BOOST = 75.0;
    radiance *= ambient_occlusion;
    radiance += emissive * EMISSIVE_BLOOM_BOOST;
    radiance += u_scene.scene_ambient_color.rgb * albedo;
    radiance = add_atmosphere(radiance, depth, u_scene.camera_near_plane, u_scene.camera_far_plane, u_scene.clear_color.rgb);

    out_color = vec4(radiance, 1.0);
}
