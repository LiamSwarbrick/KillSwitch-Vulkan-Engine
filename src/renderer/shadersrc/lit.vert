#version 460
#extension GL_GOOGLE_include_directive : require
#include "common/shared.glsl"
#include "common/shared_vertex_fetch.glsl"

layout (location = 0) in vec2 frag_uv;
layout (location = 1) in vec3 frag_vcolor;

// Multipass materials require different shaders have reproducable vertex positions
invariant gl_Position;

// vec3 hemispheric_ambient_light(vec3 N)
// {
//     const vec3 below_color = vec3(0.3, 0.1, 0.05);
//     const vec3 above_color = vec3(0.1, 0.1, 0.4);

//     float mix_factor = (N.y + 1.0) / 2.0;
//     vec3 color = mix(below_color, above_color, mix_factor);

//     return color;
// }

void main()
{
    SceneData scene  = SceneBuffer(push.dc.scene_ptr).scene;
    
    uint vertex_buf_index;
    vec3 v_pos;
    vec2 v_uv;
    vec3 v_normal;
    vec3 v_color;

    fetch_vertex(gl_VertexIndex, vertex_buf_index, v_pos, v_uv, v_normal, v_color);

    mat4 model_matrix = compute_model_matrix(vertex_buf_index);
    vec4 world_pos = model_matrix * vec4(v_pos, 1.0);

    frag_uv = v_uv;
    frag_color = v_color + hemispheric_ambient_light(v_normal);

    gl_Position = scene.view_proj * world_pos;
}
