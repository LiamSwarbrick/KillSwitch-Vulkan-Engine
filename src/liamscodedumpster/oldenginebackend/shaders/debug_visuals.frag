#version 450

#extension GL_EXT_scalar_block_layout : require

#define FS_DEBUG_VISUALS_ENTRYPOINT_IDS
#define FS_GEOMETRY_UNIFORMS
#define FS_GEOMETRY_IN
#include "../engine/shared_glsl_defs.h"


layout (location = 0) out vec4 out_color;


// NOTE: Google's glslc compiler only supports non-"main" entrypoints for HLSL instead of GLSL.
// This means I have to switch based on mode instead
layout (constant_id = 0) const int ENTRYPOINT_SELECT_ID = 0;


vec3 palette(float t, vec3 a, vec3 b, vec3 c, vec3 d)
{
    // Cosine colour palettes: (https://iquilezles.org/articles/palettes/)
    return a + b*cos( 6.283185*(c*t+d) );
}

vec3 my_palette(float t)
{
    return palette(t, vec3(0.5), vec3(0.5), vec3(1.0), vec3(0.0, 0.33, 0.67));
}

void _entrypoint_show_miplevels()
{
    // Rendering mipmap levels
    vec2 miplevel_info = textureQueryLod(rgba_albedo_alpha_map, v2f_texcoord);
    int mip_level = int(miplevel_info.y);
    float max_mip = 8.0;

    float t = miplevel_info.x/max_mip;
    out_color = vec4(my_palette(t), 1.0);
}

void _entrypoint_show_fragdepth()
{
    // Rendering fragment depth
    const float near = 0.1;
    const float far = 400.0;

    float z = gl_FragCoord.z;
    float z_linear = (near * far) / (far - z * (far - near));
    float z_normalized = z_linear / far;
    out_color = vec4(vec3(z_normalized), 1.0);
}

void _entrypoint_show_fragdepth_derivatives()
{
    // Rendering partial derivatives of fragment depth
    float z = gl_FragCoord.z;
    float dzdx = dFdx(z);
    float dzdy = dFdy(z);
    float scale = 4000.0;
    float magnitude = length(vec2(dzdx, dzdy));
    float value = clamp(magnitude * scale, 0.0, 1.0);

    out_color = vec4(
        value,
        clamp(2.0*scale*dzdx, 0.0, 1.0),
        clamp(2.0*scale*dzdy, 0.0, 1.0),
        1.0
    );
}

void _entrypoint_show_baseline_overdraw()
{
    // Write a transparent white pixel, so that overlapping draws are shown.
    float alpha = 0.2;
    float r = 0.0;
    if (texture(rgba_albedo_alpha_map, v2f_texcoord).a < 0.5)
    {
        r = 1.0;
        alpha = 0.02;  // Discard fragments are much cheaper so these are visualised as less transparent.
    }
    out_color = vec4(r, r, 0.0, alpha);  // Implies 5 overdraws sum to an opaque black pixel.
}

void _entrypoint_show_basic_overshading()
{
    _entrypoint_show_baseline_overdraw();
}

void _entrypoint_show_mesh_density()
{
    // NOTE: OLD WAY OF DOING MESH DENSITY
    // I switched to using a geometry shader for an accurate test instead of one that is only right when the normals change across the vertices

    // Calculate how much the normal changes per pixel
    float delta_normal = length(dFdx(v2f_tbn_normal)) + length(dFdy(v2f_tbn_normal));

    // Subpixel triangles cause the hardware to struggle with derivatives, 
    // often resulting in very high values or noisy derivatives.
    float density = smoothstep(0.0, 0.5, delta_normal); 

    // Map density to colours via green (low) to white to  purple (high)
    
    vec3 col = (density < 0.5) 
            ? mix(vec3(0.0, 0.7, 0.2), vec3(1.0, 1.0, 1.0), density * 2.0) 
            : mix(vec3(1.0, 1.0, 1.0), vec3(0.5, 0.0, 1.0), (density - 0.5) * 2.0);

    out_color = vec4(0.8 * col, 1.0);
}

void _prevent_shit_getting_optimized_away_and_causing_warnings()
{
    vec4 shit = vec4(0.0);
    shit.xy  += v2f_texcoord;
    shit.xyz += v2f_worldpos;
    shit.xyz += v2f_tbn_normal;
    shit.xyz += v2f_tbn_tangent;
    shit.x   += v2f_bitangent_sign;
    shit -= shit;
    out_color += shit;
}

void main()
{
    if (ENTRYPOINT_SELECT_ID == 0) return;  // Invalid: This should emit a Vulkan Warning.

    _prevent_shit_getting_optimized_away_and_causing_warnings();
    
    if (ENTRYPOINT_SELECT_ID == SHADER_ENTRYPOINT_ID_DEBUGVIZ_MIPLEVELS)             _entrypoint_show_miplevels();
    if (ENTRYPOINT_SELECT_ID == SHADER_ENTRYPOINT_ID_DEBUGVIZ_FRAGDEPTH)             _entrypoint_show_fragdepth();
    if (ENTRYPOINT_SELECT_ID == SHADER_ENTRYPOINT_ID_DEBUGVIZ_FRAGDEPTH_DERIVATIVES) _entrypoint_show_fragdepth_derivatives();
    if (ENTRYPOINT_SELECT_ID == SHADER_ENTRYPOINT_ID_DEBUGVIZ_BASELINE_OVERDRAW)     _entrypoint_show_baseline_overdraw();
    if (ENTRYPOINT_SELECT_ID == SHADER_ENTRYPOINT_ID_DEBUGVIZ_BASIC_OVERSHADING)     _entrypoint_show_basic_overshading();
    //if (ENTRYPOINT_SELECT_ID == SHADER_ENTRYPOINT_ID_DEBUGVIZ_MESH_DENSITY)          _entrypoint_show_mesh_density();
}
