#version 460
#extension GL_GOOGLE_include_directive : require
#include "common/shared.glsl"
#include "common/shared_vertex_fetch.glsl"

layout (location = 0) out vec2 uv;
layout (location = 1) out vec3 color;
layout (location = 2) out vec3 world_pos;
layout (location = 3) out vec3 world_normal;

invariant gl_Position;

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
    vec4 world_pos_homo = model_matrix * vec4(v_pos, 1.0);
    
    // NOTE: Because our transforms have scale components, we can't just use the model matrix on the normal
    mat3 normal_matrix = transpose(inverse(mat3(model_matrix)));  // mat3 because we don't want translation

    uv = v_uv;
    color = v_color;
    world_pos = world_pos_homo.xyz;
    world_normal = normalize(normal_matrix * v_normal);

    gl_Position = scene.view_proj * world_pos_homo;
}
