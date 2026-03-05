#version 450
#extension GL_EXT_scalar_block_layout : require

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

#define VS_GEOMETRY_UNIFORMS
#include "../engine/shared_glsl_defs.h"

layout (location = 0) out float v_area;
layout (location = 1) out vec3 v_barycentric;

void main()
{
    // Get the screen space coordinates of the triangle from the clip space ones
    vec2 p0 = (gl_in[0].gl_Position.xy / gl_in[0].gl_Position.w) * u_scene.framebuffer_size;
    vec2 p1 = (gl_in[1].gl_Position.xy / gl_in[1].gl_Position.w) * u_scene.framebuffer_size;
    vec2 p2 = (gl_in[2].gl_Position.xy / gl_in[2].gl_Position.w) * u_scene.framebuffer_size;

    // Calculate area of triangle in pixels
    float area = 0.5 * abs(p0.x*(p1.y - p2.y) + p1.x*(p2.y - p0.y) + p2.x*(p0.y - p1.y));

    // Barycentrics are easy
    vec3 barycentrics[] = vec3[]( vec3(1, 0, 0), vec3(0, 1, 0), vec3(0, 0, 1) );

    // Emit vertices
    for(int i = 0; i < 3; i++) {
        gl_Position = gl_in[i].gl_Position;
        v_area = area; 
        v_barycentric = barycentrics[i];
        EmitVertex();
    }
    EndPrimitive();
}
