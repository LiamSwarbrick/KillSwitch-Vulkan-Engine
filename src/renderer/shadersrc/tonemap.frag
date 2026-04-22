#version 460
#extension GL_GOOGLE_include_directive : require
#include "common/shared.glsl"
#include "common/shared_material_read.glsl"

layout (location = 0) in vec2 frag_uv;

layout (location = 0) out vec4 out_color;

vec3 tonemap_reinhard(vec3 x)
{
    return x / (1.0 + x);
}

void main()
{
    // NOTE: Currently LDR is sRGB because it matches swapchain image format
    //       Maybe LDR should be UNORM?
    // TODO: HDR Monitor support, is that easy peasy?

    // Read HDR linear color then output to monitor color-space.
    vec3 hdr = texture(sampler2D(
        global_textures[nonuniformEXT(push.pass.texture_indices[0])],
        global_samplers[FG_SAMPLER_LINEAR_BLACK_BORDER]),
        frag_uv
    ).rgb;
    
    // float exposure = 2.0;  // Thinking between 1.2–2.2
    float exposure = 2.2;
    vec3 color = hdr * exposure;
    color = tonemap_reinhard(color);

    // Slight contrast curvyness
    const float mids_factor = 0.8;
    color = pow(color, vec3(mids_factor));  // <1 apparently brightens mids a bit

    // DO NOT GAMMA CORRECT: That is done automatically
    out_color = vec4(color, 1.0);
}
