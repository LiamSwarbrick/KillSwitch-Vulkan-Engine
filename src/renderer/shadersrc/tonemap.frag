#version 460
#extension GL_GOOGLE_include_directive : require
#include "common/shared.glsl"

#extension GL_EXT_samplerless_texture_functions : require

layout (location = 0) in vec2 frag_uv;

layout (location = 0) out vec4 out_color;

vec2 lens_distortion(vec2 uv, float k, float aspect)
{
    // Negative k means wide-angle / fish-eye lens
    // Positive k means barrel distortion

    vec2 p = uv * 2.0 - 1.0;
    p.x *= aspect;

    float r2 = dot(p, p);
    float f = 1.0 + k * r2 + 0.2 * k * r2 * r2;

    p *= f;

    p.x /= aspect;
    return 0.5 * (p + 1.0);
}

float hash(float x)
{
    return fract(sin(x * 91.3458) * 47453.5453);
}

vec2 screenshake_offset(float t)
{
    return vec2(
        hash(t) * 2.0 - 1.0,
        hash(t + 17.0) * 2.0 - 1.0
    );
}

vec3 tonemap_reinhard(vec3 x)
{
    return x / (1.0 + x);
}

// vec3 tonemap_aces(vec3 x)
// {
//     const float a = 2.51;
//     const float b = 0.03;
//     const float c = 2.43;
//     const float d = 0.59;
//     const float e = 0.14;
//     return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
// }

void main()
{
    // LENS DISTORTION
    SceneData scene  = SceneBuffer(push.dc.scene_ptr).scene;
    vec2 uv = lens_distortion(frag_uv, scene.lens_distortion, scene.aspect);

    // SCREEN SHAKE
    if (scene.screenshake > 1e-12)
    {
        // Random direction
        vec2 shake = screenshake_offset(floor(scene.time * 120.0));
        shake.x *= 1.75;                      // Horizontal usually feels stronger
        shake *= 0.003 * scene.screenshake;  // Convert from pixels-ish to UV scale

        uv += shake;
    }

    // TONEMAPPING
    // NOTE: Currently LDR is sRGB because it matches swapchain image format
    //       Maybe LDR should be UNORM?
    // TODO: HDR Monitor support, should be a simple change to swapchain creation for the special colorspaces?

    // Read HDR linear color then output to monitor color-space.
    vec3 hdr = texture(sampler2D(
        global_textures[nonuniformEXT(push.pass.texture_indices[0])],
        global_samplers[FG_SAMPLER_LINEAR_BLACK_BORDER]),
        uv
    ).rgb;
    
    float exposure = 2.0;
    vec3 color = hdr * exposure;
    color = tonemap_reinhard(color);
    // color = tonemap_aces(color);  // <- Pure trash, but showcases that it looks bad for our game in the report
    

    // Slight contrast curvyness
    // const float mids_factor = 0.9;  // Previous
    const float mids_factor = 0.8;
    color = pow(color, vec3(mids_factor));  // <1 apparently brightens mids a bit

    // DO NOT GAMMA CORRECT: That is done automatically
    out_color = vec4(color, 1.0);
}
