#version 450

#extension GL_EXT_scalar_block_layout : require

#define FS_GEOMETRY_UNIFORMS
#define FS_GEOMETRY_IN
#define FS_BIND_LIGHTS
#define FS_PBR_BRDF
#include "../engine/shared_glsl_defs.h"

// Shader specialisation constants:
layout (constant_id = 0) const bool IS_ALPHA_MASKED = false;

// Write to render-target
layout (location = 0) out vec4 out_color;


void main()
{
    vec4 albedo_alpha  = texture(rgba_albedo_alpha_map, v2f_texcoord);

    if (IS_ALPHA_MASKED)
    {
        // Alpha Masking:
        if (albedo_alpha.a < 0.5)
        {
            discard;
        }
    }

    vec3 rma      = texture(rgb_roughness_metalness_ao_map, v2f_texcoord).rgb;
    vec3 emissive = texture(rgb_emissive_map,               v2f_texcoord).rgb;
    vec3 normal   = normal_mapped_normal();
    // vec3 normal   = v2f_tbn_normal;  // <-- TEMP FOR SCREENSHOTS WITHOUT NORMAL MAPPING

    vec3 albedo   = albedo_alpha.rgb;
    float roughness          = rma.r;
    float metalness          = rma.g;
    float ambient_occlusion  = rma.b;
    float depth = gl_FragCoord.z;
    vec3 world_pos = v2f_worldpos;


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
