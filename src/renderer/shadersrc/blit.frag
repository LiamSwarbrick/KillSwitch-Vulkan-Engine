#version 460
#extension GL_GOOGLE_include_directive : require
#include "common/shared.glsl"
#include "common/shared_material_read.glsl"

layout (location = 0) in vec2 frag_uv;

layout (location = 0) out vec4 out_color;


vec2 dumb_panini(vec2 uv, float strength, float aspect)
{
    vec2 p = uv * 2.0 - 1.0;
    p.x *= aspect;

    float r2 = dot(p, p);
    float k = 1.0 / (1.0 + strength * r2);

    p *= k;

    p.x /= aspect;
    return 0.5 * (p + 1.0);
}

vec2 lens_distortion(vec2 uv, float k, float aspect)
{
    vec2 p = uv * 2.0 - 1.0;
    p.x *= aspect;

    float r2 = dot(p, p);
    float f = 1.0 + k * r2 + 0.2 * k * r2 * r2;

    p *= f;

    p.x /= aspect;
    return 0.5 * (p + 1.0);
}

void main()
{
    // TEMP: Testing fun stuff (maybe use scene.resolution)
    SceneData scene  = SceneBuffer(push.dc.scene_ptr).scene;

    vec2 fun_uv = lens_distortion(frag_uv, +0.2, scene.aspect);  // Negative = wide-angle feel


    // NOTE: Blit pass puts the texture it blit's in pass.texture_indices[0]
    //       and samples with the linear sampler
    out_color = texture(sampler2D(
        global_textures[nonuniformEXT(push.pass.texture_indices[0])],
        global_samplers[FG_SAMPLER_LINEAR_BLACK_BORDER]),
        // frag_uv
        fun_uv
    );
}
