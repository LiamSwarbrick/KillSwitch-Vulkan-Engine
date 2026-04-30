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

vec3 tonemap_reinhard(vec3 x)
{
    return x / (1.0 + x);
}

void main()
{
    // LENS DISTORTION
    SceneData scene  = SceneBuffer(push.dc.scene_ptr).scene;
    vec2 uv = lens_distortion(frag_uv, scene.lens_distortion, scene.aspect);

    // TONEMAPPING
    // NOTE: Currently LDR is sRGB because it matches swapchain image format
    //       Maybe LDR should be UNORM?
    // TODO: HDR Monitor support, is that easy peasy?

    // Read HDR linear color then output to monitor color-space.
    vec3 hdr = texture(sampler2D(
        global_textures[nonuniformEXT(push.pass.texture_indices[0])],
        global_samplers[FG_SAMPLER_LINEAR_BLACK_BORDER]),
        uv
    ).rgb;
    
    float exposure = 2.0;
    vec3 color = hdr * exposure;
    color = tonemap_reinhard(color);

    // Slight contrast curvyness
    const float mids_factor = 0.9;
    color = pow(color, vec3(mids_factor));  // <1 apparently brightens mids a bit

    // DO NOT GAMMA CORRECT: That is done automatically
    out_color = vec4(color, 1.0);
}
