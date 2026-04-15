#version 460
#extension GL_GOOGLE_include_directive : require
#include "common/shared.glsl"
#include "common/shared_vertex_fetch.glsl"

layout (location = 0) out vec2 frag_uv;

invariant gl_Position;

void main()
{
    SceneData scene  = SceneBuffer(push.dc.scene_ptr).scene;

    uint vertex_buf_index;
    vec3 v_pos;
    vec2 v_uv;

    fetch_vertex_pos_uv(gl_VertexIndex, vertex_buf_index, v_pos, v_uv);

    mat4 model_matrix = compute_model_matrix(vertex_buf_index);

    gl_Position = scene.view_proj * model_matrix * vec4(v_pos, 1.0);
    frag_uv = v_uv;
}
