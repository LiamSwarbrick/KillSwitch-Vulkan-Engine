#ifndef L_SHARED_GLSL_DEFS_H
#define L_SHARED_GLSL_DEFS_H

#ifdef __cplusplus
    // The structs in this file are parsed by c++ and glsl compilers
    #include <glm/glm.hpp>
    #include <stdint.h>
    typedef glm::mat4  mat4;
    typedef glm::ivec2 ivec2;
    typedef glm::vec2  vec2;
    typedef glm::vec3  vec3;
    typedef glm::vec4  vec4;
    typedef uint32_t   uint;

    #include <math.h>
#else
    #define GLSL_ONLY
#endif

//
// Shared between GLSL and C++:
//

#ifdef GLSL_ONLY
    // Useful constants for our shaders
    #define PI 3.14159265358979323846
#endif

struct PointLight
{
    vec4 pos_and_radius;
    vec4 color_and_intensity;
};

struct SpotLight
{
    vec4 pos_and_radius;
    vec4 color_and_intensity;
    vec4 direction_and_cone_cutoff;
};

struct SceneBlock
{
    // BEWARE OF STRUCT PACKING: Hence each vec2 being adjacent with 2 floats to fill to a vec4

    mat4  view;
    mat4  projection;
    mat4  view_projection;
    ivec2 framebuffer_size;
    float time;
    float _padding0;

    vec4  camera_world_pos;
    float camera_near_plane;
    float camera_far_plane;
    vec2  _padding1;

    vec4 clear_color;

    vec4 scene_ambient_color;

    // TODO: Add spot light as the single shadow-mapped source
    SpotLight spotlight;
    mat4 spotlight_view_projection;
};

struct ObjectConstants
{
    mat4 model;
};


#define DEFINE_LightBuffer_CPP_GLSL_HEADER \
    uint point_light_count; \
    uint _padding[3];


#ifdef __cplusplus
    struct LightBufferHeader
    {
        DEFINE_LightBuffer_CPP_GLSL_HEADER
    };

    // NOTE: We won't realloc the GPU buffer if the light count is overrun for simplicity.
    #define MAX_POINT_LIGHTS_IN_GPU_BUFFER 5120

    // Define the size of the buffer that is allocated on init
    #define LIGHT_BUFFER_SIZE sizeof(LightBufferHeader) + \
        MAX_POINT_LIGHTS_IN_GPU_BUFFER * sizeof(PointLight)

    // Static checks that these structs are sized and packed correctly

    typedef struct SceneBlock SceneUniform_GLSL_ScalarBlock;
    static_assert(
        sizeof(SceneUniform_GLSL_ScalarBlock) <= UINT16_MAX+1,
        "Error: SceneUniform_GLSL_ScalarBlock is too large (must be <= 65536 bytes) for vkCmdUpdateBuffer."
    );
    static_assert(
        sizeof(SceneUniform_GLSL_ScalarBlock) % 4 == 0,
        "Error: SceneUniform_GLSL_ScalarBlock size must be a multiple of 4 for alignment."
    );


    typedef struct ObjectConstants Object_GLSL_PushConstants;
    static_assert(
        sizeof(Object_GLSL_PushConstants) <= 128,
        "Minimum size drivers can give push constants is 128 bytes, so we don't want to exceed it. (128 bytes is two mat4s)"
    );

#endif




#ifdef __cplusplus
    // NOTE: When making the debug pipeline, we set the shader specialisation constant for the active debug rendermode
    #define FS_DEBUG_VISUALS_ENTRYPOINT_IDS
#endif

//
// Shared GLSL: A shader should define what it needs before #include-ing this file.
//

// Define Entry points for the debug_visuals.frag
#ifdef FS_DEBUG_VISUALS_ENTRYPOINT_IDS

    #define    SHADER_ENTRYPOINT_ID_DEBUGVIZ_MIPLEVELS              1
    #define    SHADER_ENTRYPOINT_ID_DEBUGVIZ_FRAGDEPTH              2  
    #define    SHADER_ENTRYPOINT_ID_DEBUGVIZ_FRAGDEPTH_DERIVATIVES  3
    #define    SHADER_ENTRYPOINT_ID_DEBUGVIZ_BASELINE_OVERDRAW      4
    #define    SHADER_ENTRYPOINT_ID_DEBUGVIZ_BASIC_OVERSHADING      5
    // #define    SHADER_ENTRYPOINT_ID_DEBUGVIZ_MESH_DENSITY           6

#endif


#ifdef VS_GEOMETRY_UNIFORMS
layout (push_constant, scalar) uniform ObjectConstantsBlock
{
    ObjectConstants pcs;
};

layout (scalar, set = 0, binding = 0) uniform SceneUniformBlock
{
    SceneBlock u_scene;
};
#endif

#ifdef VS_GEOMETRY_IO
layout (location = 0) in vec3 v_position;
layout (location = 1) in vec3 v_normal;
layout (location = 2) in vec2 v_texcoord;
layout (location = 3) in vec4 v_tangent;  // NOTE: v_tangent.w must store the bitangent sign

layout (location = 0) out vec2 v2f_texcoord;
layout (location = 1) out vec3 v2f_worldpos;
layout (location = 2) out vec3 v2f_tbn_normal;
layout (location = 3) out vec3 v2f_tbn_tangent;
layout (location = 4) out float v2f_bitangent_sign;
#endif

#ifdef FS_GEOMETRY_UNIFORMS
layout (scalar, set = 0, binding = 0) uniform SceneUniformBlock
{
    SceneBlock u_scene;
};

layout (set = 1, binding = 0) uniform sampler2D rgba_albedo_alpha_map;
layout (set = 1, binding = 1) uniform sampler2D rgb_roughness_metalness_ao_map;
layout (set = 1, binding = 2) uniform sampler2D rgb_normal_map;
layout (set = 1, binding = 3) uniform sampler2D rgb_emissive_map;
#endif

#ifdef FS_GEOMETRY_IN
layout (location = 0) in vec2 v2f_texcoord;
layout (location = 1) in vec3 v2f_worldpos;
layout (location = 2) in vec3 v2f_tbn_normal;
layout (location = 3) in vec3 v2f_tbn_tangent;
layout (location = 4) in float v2f_bitangent_sign;

    #ifdef FS_GEOMETRY_UNIFORMS
    vec3 normal_mapped_normal()
    {
        vec3 normal_sample = texture(rgb_normal_map, v2f_texcoord).rgb * 2.0 - vec3(1.0);
        vec3 T = normalize(v2f_tbn_tangent);
        vec3 N = normalize(v2f_tbn_normal);
        vec3 B = normalize(cross(N, T)) * v2f_bitangent_sign;
        mat3 tbn_matrix = mat3(T, B, N);
        vec3 normal = normalize(tbn_matrix * normal_sample);

        // NOTE: Currently for some unknown reason a few bits of geometry in the suntemple have crap tangents
        return length(T) > 0.5 ? normal : v2f_tbn_normal;
    }
    #endif
#endif

#ifdef FS_DEFERRED_LIGHTING_IN
#define FS_BIND_LIGHTS

layout (location = 0) in vec2 v2f_uv;

layout (scalar, set = 0, binding = 0) uniform SceneUniformBlock
{
    SceneBlock u_scene;
};

// G BUFFER TEXTURES
layout (set = 1, binding = 0) uniform sampler2D albedo_roughness_texture;
layout (set = 1, binding = 1) uniform sampler2D normal_metalness_texture;
layout (set = 1, binding = 2) uniform sampler2D emissive_ao_texture;
layout (set = 1, binding = 3) uniform sampler2D depth_texture;
#endif

#ifdef FS_BIND_LIGHTS
layout (scalar, set = 2, binding = 0) readonly buffer LightBuffer {
    DEFINE_LightBuffer_CPP_GLSL_HEADER
    PointLight point_lights[];
} u_lights;

layout (set = 3, binding = 0) uniform sampler2DShadow shadow_map;
#endif

#ifdef FS_PBR_BRDF
vec3 lighting_with_brdf_lambert_beckmann(PointLight light, vec3 frag_pos, vec3 eye_pos, vec3 normal, vec3 albedo, float roughness, float metalness)
{
    // Computes brdf * ndotl. (not just the brdf part)
    // Diffuse is Lambert, Specular uses Beckman NDF with schlick fresnel etc...

    float alpha = roughness * roughness;
    float M     = metalness;

    vec3 light_pos = light.pos_and_radius.xyz;
    vec3 frag_to_light = light_pos - frag_pos;
    vec3 l = normalize(frag_to_light);
    vec3 v = normalize(eye_pos - frag_pos);
    vec3 h = normalize(l + v);
    vec3 n = normal;

    float dothv = dot(h, v);
    vec3  F0    = (1.0 - M) * vec3(0.04) + (M * albedo);
    float fv_   = (1 - dothv);
    float fv_2  = fv_*fv_;
    vec3  Fv    = F0 + (vec3(1.0) - F0) * (fv_*fv_2*fv_2);
    vec3 L_diffuse = (albedo/radians(180.0)) * (vec3(1.0) - Fv) * (1 - M);

    float eps = 1e-16;  // Avoids zero division
    float alpha2 = alpha*alpha;    
    float dotnh = max(0.0, dot(n, h));
    float dotnh2 = dotnh * dotnh;
    float dotnh4 = dotnh2 * dotnh2;
    float Dh_numerator_exp_numerator = dotnh2 - 1.0;
    float Dh_numerator_exp_denom = alpha2 * dotnh2;
    float Dh_numerator = exp(Dh_numerator_exp_numerator / (Dh_numerator_exp_denom + eps));
    float Dh_denom = radians(180.0) * alpha2 * dotnh4;
    float Dh = Dh_numerator / (Dh_denom + eps);    

    float dotnv = max(0.0, dot(n, v));
    float dotnl = max(0.0, dot(n, l));
    float Glv_a = (2.0 * dotnh * dotnv) / (dothv + eps);
    float Glv_b = (2.0 * dotnh * dotnl) / (dothv + eps);
    float Glv = min(1.0, min(Glv_a, Glv_b));
    vec3 L_specular = (Dh * Fv * Glv) / (4.0 * dotnv * dotnl + eps);

    vec3 brdf = L_diffuse + L_specular;

    // Light source radiant exitance with standard distance squared falloff
    // TODO: Adjust to a falloff that reaches zero at a radius r, so that we can use light culling algorithms
    float distance_squared = dot(frag_to_light, frag_to_light);
    float attenuation = 1.0 / distance_squared;
    vec3 light_source_exitance = attenuation * light.color_and_intensity.rgb * light.color_and_intensity.a;

    return light_source_exitance * brdf * dotnl;
}

vec3 spotlight_radiance(SpotLight light, vec3 frag_pos, vec3 eye_pos, vec3 normal, vec3 albedo, float roughness, float metalness)
{
    vec3 light_pos = light.pos_and_radius.xyz;
    vec3 light_dir = light.direction_and_cone_cutoff.xyz;

    vec3 L = normalize(frag_pos - light_pos);

    float cos_theta = dot(L, light_dir);
    float cos_outer = light.direction_and_cone_cutoff.w;

    if (cos_theta < cos_outer)
    {
        return vec3(0.0);
    }

    // NOTE: The cone cutoff must be the outside of the penumbra, not the start of it
    // otherwise the shadowmap would be wrongly sized and we would not have shadows in the whole penumbra.
    float cos_inner = cos_outer + 0.02;
    float spot_factor = smoothstep(cos_outer, cos_inner, cos_theta);

    PointLight fake_point;
    fake_point.pos_and_radius     = light.pos_and_radius;
    fake_point.color_and_intensity = light.color_and_intensity;

    vec3 radiance =
        lighting_with_brdf_lambert_beckmann(
            fake_point,
            frag_pos,
            eye_pos,
            normal,
            albedo,
            roughness,
            metalness
        );

    return radiance * spot_factor;
}

vec3 add_atmosphere(vec3 radiance, float depth, float near_plane, float far_plane, vec3 sky_color)
{
    float z_linear = (near_plane * far_plane) / (far_plane - depth * (far_plane - near_plane));
    float z_normalized = z_linear / far_plane;
    vec3 fog_color = sky_color + vec3(0.12, 0.12, 0.0);  // Push it a bit towards white as an aestetic choice
    return mix(radiance, fog_color, z_normalized);
}
#endif

#endif  // L_SHARED_GLSL_DEFS_H
